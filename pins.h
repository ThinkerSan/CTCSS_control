#ifndef _PINS_H_
#define _PINS_H_

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif

#define CONCAT(x,y)		x ## y	// Склеиваем две строковых переменных

#define DDR(x)		CONCAT(DDR,x)	// Определяет имя регистра ввода-вывода
#define PORT(x)		CONCAT(PORT,x)	// Определяет имя порта вовода-вывода
#define PIN(x)		CONCAT(PIN,x)	// Определяет имя ножки контроллера

#define PIN_OUTPUT(x)		(DDR(x) |= x ## _BIT)
#define PIN_INPUT(x)		(DDR(x) &= ~x ## _BIT)
#define SET_BIT(x)			(PORT(x) |= x ## _BIT)
#define CLR_BIT(x)			(PORT(x) &= ~x ## _BIT)
#define READ_BIT(x)			(PIN(x) & x ## _BIT)

/* пример определения портов
*
#define ONE_WIRE		B	// порт B
#define ONE_WIRE_BIT	(1<<1)	// pin 6 (для Attiny85)
#define RF_OTPUT		B	// порт B
#define RF_OTPUT_BIT	(1<<2)	// pin 7 (для Attiny85)
*
* Для вызова в программе использовать:
* SET_BIT(ONE_WIRE)
* PIN_OUTPUT(RF_OTPUT)
* и т.д.
*
*/
//
/* Определения портов подключения переферии */

//Выводы для подключения линий SDA и SCK к чипу FD650DK на плате индикации.
#define sdaPin        A0       // Порт PC0, вывод A0 на плате Arduino UNO (SDA).
#define sclPin        A1       // Порт PC1, вывод A1 на плате Arduino UNO (SCL).

// Выводы для подключения линий Data, Clock, Latch к чипу MX465
#define dataPin       D       // порт PD2, вывод 2 на плате Arduino UNO (Data).
#define dataPin_BIT   (1<<2)
#define clockPin      D		    // порт PD3, вывод 3 на плате Arduino UNO (Clock).
#define clockPin_BIT	(1<<3)
#define latchPin		  D		    // порт PD6, вывод 6 на плате Arduino UNO (Latch).
#define latchPin_BIT	(1<<6)

// По умолчанию эти выводы отключены в коде, так как могут использоваться
// аппаратные перемычки на плате с чипом MX465. Если необходимо управление
// на програмном уровне, то нужно установить параметр SERIAL_CONFIG в "1"
#define sEnableLowPin	      D			 // Порт PD4 вывод 4 на плате Arduino UNO. (D4 MX465).
#define sEnableLowPin_BIT	  (1<<4)
#define sEnableHighPin	    D			 // Порт PD5 вывод 5 на плате Arduino UNO. (D5 MX465).
#define sEnableHighPin_BIT	(1<<5)

#endif // _PINS_H_
