#include <cox.h>
#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_hid.h"
#include "usbd_desc.h"

/* In order to measure current in idle mode, define MEASURE_CURRENT_IN_SLEEP below. */
//#define MEASURE_CURRENT_IN_SLEEP

Timer timerHello;
USBD_HandleTypeDef usb;

static void taskHello(void *) {
  Serial.println();
  Serial.println("Serial) Hello World!");
  Serial2.println();
  Serial2.println("Serial2) Hello World!");

  digitalToggle(PB14);
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

void setup() {
  /* LD3 */
  pinMode(PB14, OUTPUT);
  digitalWrite(PB14, LOW);

  /* LD2 */
  pinMode(PB7, OUTPUT);
  digitalWrite(PB7, LOW);

  Serial.begin(115200);
  Serial.println("*** [ST Nucleo-F429ZI] Basic Functions ***");
  Serial.flush();
  Serial.onReceive(eventSerialRx);
  #ifndef MEASURE_CURRENT_IN_SLEEP
  Serial.listen();
  #endif

  Serial2.begin(115200);
  Serial2.println("*** [ST Nucleo-F429ZI] Basic Functions ***");
  Serial2.flush();
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
  timerHello.startPeriodic(1000);
}
