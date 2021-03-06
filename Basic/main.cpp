#include <cox.h>
#include <Timer.hpp>
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_hid.h"
#include "usbd_desc.h"

/* In order to measure current in idle mode, define MEASURE_CURRENT_IN_SLEEP below. */
//#define MEASURE_CURRENT_IN_SLEEP

Timer timerHello;
Timer timerAdc;
USBD_HandleTypeDef usb;
uint16_t countNoInit __attribute__((section(".noinit")));

static void taskAdc(void *) {
  Serial.printf("A0:%5d mV, ", map(analogRead(A0), 0, 4095, 0, 3300));
  Serial.printf("A1:%5d mV, ", map(analogRead(A1), 0, 4095, 0, 3300));
  Serial.printf("A2:%5d mV, ", map(analogRead(A2), 0, 4095, 0, 3300));
  Serial.printf("A3:%5d mV, ", map(analogRead(A3), 0, 4095, 0, 3300));
  Serial.printf("A4:%5d mV, ", map(analogRead(A4), 0, 4095, 0, 3300));
  Serial.printf("A5:%5d mV, ", map(analogRead(A5), 0, 4095, 0, 3300));
  Serial.printf("A6:%5d mV, ", map(analogRead(A6), 0, 4095, 0, 3300));
  Serial.printf("A7:%5d mV, ", map(analogRead(A7), 0, 4095, 0, 3300));
  Serial.printf("A8:%5d mV\n", map(analogRead(A8), 0, 4095, 0, 3300));
}

static void eventSerialRx(SerialPort &p) {
  while (p.available() > 0) {
    char c = p.read();
    p.write(c);
    digitalToggle(PB7);
  }
}

static void eventUserKeyPressed() {
  static int8_t cnt = 0;
  int8_t x = 0, y = 0;

  if (cnt++ > 0) {
    x = 5;
  } else {
    x = -5;
  }

  uint8_t buf[4];
  buf[0] = 0;
  buf[1] = x;
  buf[2] = y;
  buf[3] = 0;
  USBD_HID_SendReport(&usb, buf, 4);

  Serial.println("* User key is pressed.");
}

static void taskHello(void *) {
  struct timeval t;
  gettimeofday(&t, NULL);

  Serial.printf("[%lu.%06lu] (Serial) Hello World! count:%u\n", (uint32_t) t.tv_sec, t.tv_usec, countNoInit);
  Serial2.printf("[%lu.%06lu] (Serial2) Hello World! count:%u\n", (uint32_t) t.tv_sec, t.tv_usec, countNoInit);

  countNoInit++;

  digitalToggle(PB14);
}

void setup() {
  /* LD3 */
  pinMode(PB14, OUTPUT);
  digitalWrite(PB14, LOW);

  /* LD2 */
  pinMode(PB7, OUTPUT);
  digitalWrite(PB7, LOW);

  Serial.begin(115200);
  Serial.println("*** [ST Nucleo-F429ZI] Basic Functions ***");
  Serial.onReceive(eventSerialRx);
#ifndef MEASURE_CURRENT_IN_SLEEP
  Serial.listen();
#endif

  Serial2.begin(115200);
  Serial2.println("*** [ST Nucleo-F429ZI] Basic Functions ***");
  Serial2.onReceive(eventSerialRx);
#ifndef MEASURE_CURRENT_IN_SLEEP
  Serial2.listen();
#endif

  USBD_StatusTypeDef s;
  if ((s = USBD_Init(&usb, &hid, 0)) != USBD_OK) {
    Serial.printf("* USBD_Init() failed: %d\n", s);
    return;
  }
  if ((s = USBD_RegisterClass(&usb, USBD_HID_CLASS)) != USBD_OK) {
    Serial.printf("* USBD_RegisterClass() failed: %d\n", s);
    return;
  }
  if ((s = USBD_Start(&usb)) != USBD_OK) {
    Serial.printf("* USBD_Start() failed: %d\n", s);
    return;
  }

  pinMode(PC13, INPUT);
  attachInterrupt(PC13, eventUserKeyPressed, RISING);

  timerHello.onFired(taskHello, NULL);
  timerHello.startPeriodic(500);

  timerAdc.onFired(taskAdc, NULL);
  timerAdc.startPeriodic(5000);
}
