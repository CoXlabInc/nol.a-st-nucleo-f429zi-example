#include <cox.h>

Timer timerHello;

static void taskHello(void *) {
  Serial.println();
  Serial.println("Serial) Hello World!");
  Serial2.println();
  Serial2.println("Serial2) Hello World!");
  SerialUSB.println();
  SerialUSB.printf("SerialUSB) Hello World!\n");
  digitalToggle(PB14);
}

static void eventSerialRx(SerialPort &p) {
  while (p.available() > 0) {
    char c = p.read();
    p.write(c);
    digitalToggle(PB7);
  }
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
  Serial.listen();

  Serial2.begin(115200);
  Serial2.println("*** [ST Nucleo-F429ZI] Basic Functions ***");
  Serial2.onReceive(eventSerialRx);
  Serial2.listen();

  SerialUSB.begin();
  SerialUSB.println("*** [ST Nucleo-F429ZI] Basic Functions ***");
  SerialUSB.onReceive(eventSerialRx);
  SerialUSB.listen();

  timerHello.onFired(taskHello, NULL);
  timerHello.startPeriodic(1000);
}
