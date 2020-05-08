/*
 *  Скетч для управления микросхемой MX465 CTCSS Encoder/Decoder.
 *  Управление не полное, так как делалось на заказ под определенные задачи.
 *  
 *  Управление построено на Atmega328P (можно на любой другой, только необходимо
 *  поправить конфигурацию портов в файле pins.h) с использованием платы индикации
 *  и контроля от китайского ресивера. За индикацию и кнопки отвечает микросхема
 *  FD650 (аналог TM1650).
 *  
 *  В коде используется готовая библиотека TM16xx. https://github.com/maxint-rd/TM16xx 
 *  
 *    Оригинальные коды клавиатуры с платы китайского ресивера:
 *    Standby: 27
 *    Menu:    15
 *    Ok:      11
 *    Up:      7
 *    Down:    3
 *    Left:    19
 *    Right:   23
 *    
 *  Управление генератором тона следующее:
 *    - Кнопка STANDBY (здесь и далее обозначение как на плате)
 *      При длительном нажатии включает/отключает генератор тона,
 *      на дисплее выводится надпись "OFF". При выключенном состоянии
 *      кнопки "+"/"-" на работу не влияют и параметры не меняют.
 *      
 *    - Кнопка MENU сохраняет параметры в энергонезависимую память.
 *      При этом дисплей мигнет три раза показывая сохраненные параметры.
 *      
 *    - Кнопки OK и UP соответственно "+" и "-".  
 *    
 *  P.S. Можно назначить любые другие сочетания кнопок, просто мне было так удобней. =)  
 *   
 *  
 *  Для правильной работы генератора необходимо подать на выводы 5 и 6 логическую "1" и "0"
 *  соответственно. Это можно сделать как аппаратно - перемычками, так и программно - включив
 *  константу SERIAL_CONFIG, но для этого необходимо еще две сигнальные линии от контроллера,
 *  что, на мой взгляд, не рационально.
 *  
 */
#include <EEPROM.h>
#include <util/delay_basic.h>
#include "TM1650.h"
#include "TM16xxDisplay.h"
#include "TM16xxButtons.h"
#include "pins.h"

// Включение/Выключение вывода отладочной информации в COM порт.
#define OPT_DEBUG_PRINT 0
#if(OPT_DEBUG_PRINT)
#define DEBUG_BEGIN(x) Serial.begin(x)
#define DEBUG_PRINT(x) (Serial.print(x))
#define DEBUG_PRINTBIN(x) (Serial.print(x, BIN))
#define DEBUG_PRINTLN(x) (Serial.println(x))
#define DEBUG_PRINTLNHEX(x) (Serial.println(x, HEX))
#define DEBUG_PRINTLNBIN(x) (Serial.println(x, BIN))
#else
#define DEBUG_BEGIN(x) 
#define DEBUG_PRINT(x) 
#define DEBUG_PRINTBIN(x)
#define DEBUG_PRINTLN(x) 
#define DEBUG_PRINTLNHEX(x)
#define DEBUG_PRINTLNBIN(x)
#endif

// Включает (1) или отключает (0) программное управление конфигурацией
// SERIAL ENABLE в MX465.
constexpr auto SERIAL_CONFIG = 0U;

/*
 *	Точки на дисплее. Считать с право на лево.
 *  ВИД       МАСКА для DOT_POS
 *	XXXX.     0x01
 *	XXX.X     0x02
 *	XX.XX     0x04
 *	X.XXX     0x08
 *	X.X.X.X.  0x0F
 */
constexpr auto DOT_POS = 2U;  // битовая маска для показа точки

// Кнопки
constexpr auto BT_TONE = 27U;   // вкл/выкл генератора тона
constexpr auto BT_SAVE = 15U;   // Сохранить настройки
constexpr auto BT_UP = 11U;     // Увеличение частоты
constexpr auto BT_DOWN = 7U;    // Уменьшение частоты

constexpr auto NOTONE = 48U;      // Конфигурация бит, отключающая генератор тона 
constexpr auto addr_count = 0U;   // Адрес в EEPROM счетчика
constexpr auto addr_switch = 1U;  // Адрес в EEPROM переключателя
constexpr auto INIT_ADDR = 1023U; // Адрес ключа инициализации первого запуска
constexpr auto INIT_KEY = 127U;   // Сам ключ

uint8_t nCount;	  // Счетчик нажатий
bool ctcssSwitch; // Флаг включения/отключения генератор тона
uint8_t arrSize;  // Размер массива. Вообще то можно и руками, но пусть будет автоматом - ресурсы позволяют =)

TM1650 module(sdaPin, sclPin);      // объект для работы с чипом FD650DK (аналог TM1650)
TM16xxDisplay display(&module, 4U); // объект для работы с четырехразрядным дисплеем чипов серии TM16xx
TM16xxButtons buttons(&module);     // объект для работы с кнопками чипов серии TM16xx

/*
 * Массив таблицы частот CTCSS для чипа MX465
 */
const uint8_t ctcssTable[] PROGMEM = {
	0x3F,	0x39,	0x1F,	0x3E,	0x0F,	0x3D,	0x1E,	0x3C,
	0x0E,	0x3B,	0x1D,	0x3A,	0x0D,	0x1C,	0x0C,	0x1B,
	0x0B,	0x1A,	0x0A,	0x19,	0x09,	0x18,	0x08,	0x17,
	0x07,	0x16,	0x31,	0x06,	0x15,	0x05,	0x14,	0x32,
	0x04,	0x33,	0x13,	0x34,	0x35,	0x03,	0x36,	0x12,
	0x02,	0x11,	0x37,	0x01,	0x10,	0x00,	0x38
};

/*
 * Массив частот для отображения на дисплее.
 * Порядок следования совпадает с предыдущим массивом (ctcssTable)
 */
const uint16_t freqTable[] PROGMEM = {
	670, 693, 719, 744, 770, 797, 825, 854,	885, 915,
	948, 974,	1000,	1035,	1072,	1109,	1148,	1188,	1230,	1273,
	1318,	1365,	1413,	1462,	1514,	1567,	1598,	1622,	1679,	1738,
	1799,	1835,	1862,	1899,	1928,	1966,	1995,	2035,	2065,	2107,
	2181,	2257,	2291,	2336,	2418,	2503,	2541
};

void setup()
{
	DEBUG_BEGIN(115200);

	// Настройка портов ввода/вывода для связи с чипом MX465
	PIN_OUTPUT(dataPin);
	PIN_OUTPUT(clockPin);
	PIN_OUTPUT(latchPin);
	CLR_BIT(latchPin);
	CLR_BIT(clockPin);
	CLR_BIT(dataPin);

	if (SERIAL_CONFIG)
	{
		PIN_OUTPUT(sEnableLowPin);
		PIN_OUTPUT(sEnableHighPin);
		CLR_BIT(sEnableLowPin);   // Установить при инициализации в "0" при работе в SPI
		SET_BIT(sEnableHighPin);  // Установить при инициализации в "1" при работе в SPI
	}

	// Проверка и запись, если отсутствуют, значений по умолчанию для
	// генератора тона.
	if (EEPROM.read(INIT_ADDR) != INIT_KEY)
	{
		EEPROM.write(INIT_ADDR, INIT_KEY);  // записать ключ инициализации первого запуска
		EEPROM.put(addr_count, 0U);         // установить частоту по умолчанию на 67 Гц.
		EEPROM.put(addr_switch, 1U);        // генератор тона включен.
	}

	EEPROM.get(addr_count, nCount);         // считать данные частоты из энергонезависимой памяти
	EEPROM.get(addr_switch, ctcssSwitch);   // считать данные о вкл./выкл. генератора тона

	arrSize = (sizeof(freqTable) / sizeof(uint16_t)) - 1;  // расчет размера массива

	sendByte();  // настроить MX465

	module.setupDisplay(true, 4);       // Инициализация дисплея с яркостью 4 (мин. 0 - макс. 7)
	module.clearDisplay();              // очищаем
	module.setDisplayToString("HALO");  // просто приветствие


	// запуск обработчиков нажатия кнопок. Некоторые не нужны, но пусть будут. :-)
	buttons.attachRelease(fnRelease);                 // срабатывает когда кнопку отпустили
	buttons.attachClick(fnClick);                     // срабатывает когда кнопка нажата
	//buttons.attachDoubleClick(fnDoubleclick);       // срабатывает при двойном клике
	buttons.attachLongPressStart(fnLongPressStart);   // срабатывает при старте долгого нажатия
	//buttons.attachLongPressStop(fnLongPressStop);   // срабатывает при завершении долгого нажатия
	buttons.attachDuringLongPress(fnLongPress);       // срабатывает при долгом нажатии

	delay(1000);

	module.clearDisplay();
	updateDisplay();
}

void loop()
{
	buttons.tick();  // опрос кнопок
}

/**
 * @brief Передача информации в MX465 по последовательному интерфейсу.
 *      P.S. Возможно придется поправить задержки т.к. не проверил в "железе".
 *
 */
void sendByte()
{
	uint8_t nByte;
	if (ctcssSwitch)
		nByte = pgm_read_word(&ctcssTable[nCount]);
	else
		nByte = NOTONE;
	// Данные передаются с лево на право. Сначала отправляем биты D5...D0
	// затем нужно отправить биты Rx/Tx и PTL, но так как эти биты всегда
	// выставлены в "0", то можно просто сдвинуть первые шесть бит в лево на два 
	nByte <<= 2;

	CLR_BIT(latchPin);
	CLR_BIT(clockPin);

  cli();  // oчистить флаг глобального прерывания

	for (uint8_t i = 0; i < 8; i++)
	{
		nByte & 0x80 ? SET_BIT(dataPin) : CLR_BIT(dataPin);
		_delay_loop_1(5);
		SET_BIT(clockPin);
		_delay_loop_1(5);
		CLR_BIT(clockPin);
		_delay_loop_1(5);
		nByte <<= 1;
	}

	SET_BIT(latchPin);
	_delay_loop_1(10);
	CLR_BIT(latchPin);
	CLR_BIT(dataPin);

  sei();  // установить флаг глобального прерывания
}

/**
 * @brief Сохраняет параметры настройки в память EEPROM.
 * Операция сохранения сопровождается миганием дисплея, заданными в параметрах
 * nDelay и nTimes
 *
 * @param   nDelay  Задержка мигания дисплея в мс.
 * @param   nTimes  Количество миганий.
 */
void fnSaveSetting(uint16_t nDelay = 500, uint8_t nTimes = 1)
{
	for (uint16_t i = 0; i < nTimes; i++)
	{
		module.clearDisplay();  // очистить
		delay(nDelay);          // задержка
		updateDisplay();        // показать
		delay(nDelay);          // задержка
	}

	delay(nDelay);

	sendByte();

	EEPROM.update(addr_count, nCount);        // Сохранить данные счетчика в EEPROM
	EEPROM.update(addr_switch, ctcssSwitch);  // Сохранить состояние генератора тона (вкл/выкл)
}

/**
 * @brief Обработчик одиночного нажатия кнопок. Используется для кнопок
 * больше/меньше и для включения/выключения генератора тона.
 *
 */
void fnClick(uint8_t nButton)
{
	switch (nButton)
	{
	case BT_UP:
	{
		// если нажата кнопка "+" и шумодав включен, увеличить счетчик на 1,
		// а при достижении максимума - начать с начала.
		(nCount < arrSize) ? nCount++ : nCount = 0;
		sendByte();
		updateDisplay();  // обновить показания дисплея
		break;
	}
	case BT_DOWN:
	{
		// кнопка "-" тоже что предыдущее действие, но в обратном порядке
		(nCount > 0U) ? nCount-- : nCount = arrSize;
		sendByte();
		updateDisplay();  // обновить показания дисплея
		break;
	}
	default:
		break;
	}
}

/**
 * @brief Обработчик длинного однократного нажатия кнопок. Используется
 *  для включения/выключения генератора тона.
 *
 */
void fnLongPressStart(byte nButton)
{
  if (nButton == BT_TONE) ctcssSwitch = !ctcssSwitch;
  sendByte();
  updateDisplay();
}

/**
 * @brief Обработчик длинного нажатия кнопок. Используется для кнопок
 * больше/меньше.
 *
 */
void fnLongPress(uint8_t nButton)
{
	static uint8_t tik_time = 0;  // счетчик-триггер нажатий кнопки

	if (ctcssSwitch && (nButton == BT_UP || nButton == BT_DOWN))
	{
		if (200 > tik_time)   // не прошло 200 мс?
			tik_time++;         // увеличиваем счетчик-триггер на единицу и выходим
		else                  // если 200 мс прошло увеличить/уменьшить счетчик на 1
		{
			if (nButton == BT_UP)
				(nCount < arrSize) ? nCount++ : nCount = 0;

			if (nButton == BT_DOWN)
				(nCount > 0U) ? nCount-- : nCount = arrSize;

			tik_time = 0;       // сбросим счетчик-триггер для следующего прохода

			sendByte();
			updateDisplay();    // обновим информацию на дисплее
		}
	}
}

/**
 * @brief Обработчик отпускания нажатой кнопки. Используется
 *  для сохранения параметров в энергонезависимую память.
 *
 */
void fnRelease(uint8_t nButton)
{
  if (nButton == BT_SAVE) fnSaveSetting(200, 3);  // сохранить, мигнув дисплеем 3 раза
}

/**
 * @brief Обновляет информацию на дисплее.
 *
 */
void updateDisplay()
{
	if (ctcssSwitch)  // Если генератор тона включен - показать частоту, иначе показать "Off"
		display.setDisplayToDecNumber(pgm_read_word(&freqTable[nCount]), DOT_POS, false);
	else
		display.setDisplayToString(F(" OFF"));
}
