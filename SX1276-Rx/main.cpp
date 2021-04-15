#include <cox.h>
#include "SX1276Wiring.hpp"

SX1276Wiring SX1276 = SX1276Wiring(
  Spi,
  A0,  //Reset
  D10, //CS
  A4,  //RxTx
  D2,  //DIO0
  D3,  //DIO1
  D4,  //DIO2
  D5,  //DIO3
  A3   //DIO4
);

Timer tRSSI;
uint32_t tRxStarted, tRxDone;
int16_t rssiRxStarted;
char buf[20];
int8_t modem;
Radio::LoRaSF_t sf;
Radio::LoRaCR_t cr;
Radio::LoRaBW_t bw;
bool iq;
uint8_t syncword;

static void printRxDone(void *args) {
  RadioPacket *rxFrame = (RadioPacket *) args;

  printf( "[%lu us (d:%lu)] Rx is done!: RSSI:%d dB, CRC:%s, Length:%u, (",
          tRxDone, (tRxDone - tRxStarted),
          rxFrame->power,
          rxFrame->result == RadioPacket::SUCCESS ? "OK" : "FAIL",
          rxFrame->len);
  uint16_t i;
  for (i = 0; i < rxFrame->len; i++)
    printf("%02X ", rxFrame->buf[i]);
  printf("\b)\n");

  delete rxFrame;
}

static void eventOnRxDone(void *, GPIOInterruptInfo_t *) {
  tRxDone = micros();
  digitalWrite(PC9, LOW);

  RadioPacket *rxFrame = new RadioPacket(255);

  if (!rxFrame) {
    printf("Out of memory to read frame\n");
    SX1276.flushBuffer();
    return;
  }

  SX1276.readFrame(rxFrame);
  postTask(printRxDone, (void *) rxFrame);
  //SX1276.cca();
}

static void eventOnChannelBusy(void *, GPIOInterruptInfo_t *) {
  printf("Channel Busy!!\n");
  SX1276.cca();
}

static void printRxStarted(void *args) {
  printf("[%lu us] Rx is started... (%d dB)\n", tRxStarted, rssiRxStarted);
}

static void eventOnRxStarted(void *, GPIOInterruptInfo_t *) {
  digitalWrite(PC9, HIGH);
  tRxStarted = micros();
  rssiRxStarted = SX1276.getRssi();
  postTask(printRxStarted, NULL);
}

static void taskRSSI(void *args) {
  printf("[%lu us] RSSI: %d dB\n", micros(), SX1276.getRssi());
}

static void eventKeyboardInput(SerialPort &) {
  reboot();
}

static void appStart() {
  Serial.onReceive(eventKeyboardInput);

  /* All parameters are specified. */
  SX1276.begin();

  if (modem == 0) {
    SX1276.setRadio(sf, bw, cr, true, iq);
    SX1276.setSyncword(syncword);
  } else {
    SX1276.setRadio(50000, 50000, 83333, 25000);
  }

  SX1276.setChannel(917100000);
  SX1276.onRxStarted = eventOnRxStarted;
  SX1276.onRxDone = eventOnRxDone;
  SX1276.onChannelBusy = eventOnChannelBusy;
  SX1276.wakeup();
  //SX1276.cca();

  tRSSI.onFired(taskRSSI, NULL);
  //tRSSI.startPeriodic(1000);
}

static void inputSyncword(SerialPort &);
static void askSyncword() {
  printf("Enter syncword [0x12]:");
  Serial.onReceive(inputSyncword);
  Serial.inputKeyboard(buf, sizeof(buf));
}

static void inputSyncword(SerialPort &) {
  if (strlen(buf) == 0) {
    syncword = 0x12;
  } else {
    uint8_t sw = (uint8_t) strtoul(buf, NULL, 0);
    if (sw == 0) {
      printf("* Invalid syncword.\n");
      askSyncword();
      return;
    }

    syncword = sw;
  }

  printf("* Syncword: 0x%02X\n", syncword);
  appStart();
}

static void inputIQ(SerialPort &);
static void askIQ() {
  printf("Set IQ (0:normal, 1:inverted) [0]:");
  Serial.onReceive(inputIQ);
  Serial.inputKeyboard(buf, sizeof(buf));
}

static void inputIQ(SerialPort &) {
  if (strlen(buf) == 0 || strcmp(buf, "0") == 0) {
    printf("* Normal selected.\n");
    iq = true;
  } else if (strcmp(buf, "1") == 0) {
    printf("* inverted selected.\n");
    iq = false;
  } else {
    printf("* Unknown IQ mode\n");
    askIQ();
    return;
  }
  askSyncword();
}

static void inputBW(SerialPort &);
static void askBW() {
  printf("Set bandwidth (0:125kHz, 1:250kHz, 2:500kHz) [0]:");
  Serial.onReceive(inputBW);
  Serial.inputKeyboard(buf, sizeof(buf));
}

static void inputBW(SerialPort &) {
  if (strlen(buf) == 0 || strcmp(buf, "0") == 0) {
    printf("* 125kHz selected.\n");
    bw = Radio::BW_125kHz;
  } else if (strcmp(buf, "1") == 0) {
    printf("* 250kHz selected.\n");
    bw = Radio::BW_250kHz;
  } else if (strcmp(buf, "2") == 0) {
    printf("* 500kHz selected.\n");
    bw = Radio::BW_500kHz;
  } else {
    printf("* Unknown SF\n");
    askBW();
    return;
  }
  askIQ();
}

static void inputSF(SerialPort &);
static void askSF() {
  printf("Set SF (7, 8, 9, 10, 11, 12) [7]:");
  Serial.onReceive(inputSF);
  Serial.inputKeyboard(buf, sizeof(buf));
}

static void inputSF(SerialPort &) {
  if (strlen(buf) == 0 || strcmp(buf, "7") == 0) {
    printf("* SF7 selected.\n");
    sf = Radio::SF7;
  } else if (strcmp(buf, "8") == 0) {
    printf("* SF8 selected.\n");
    sf = Radio::SF8;
  } else if (strcmp(buf, "9") == 0) {
    printf("* SF9 selected.\n");
    sf = Radio::SF9;
  } else if (strcmp(buf, "10") == 0) {
    printf("* SF10 selected.\n");
    sf = Radio::SF10;
  } else if (strcmp(buf, "11") == 0) {
    printf("* SF11 selected.\n");
    sf = Radio::SF11;
  } else if (strcmp(buf, "12") == 0) {
    printf("* SF12 selected.\n");
    sf = Radio::SF12;
  } else {
    printf("* Unknown SF\n");
    askSF();
    return;
  }
  askBW();
}

static void inputModem(SerialPort &);
static void askModem() {
  printf("Select modem (0: LoRa, 1:FSK) [0]:");
  Serial.onReceive(inputModem);
  Serial.inputKeyboard(buf, sizeof(buf));
}

static void inputModem(SerialPort &) {
  if (strlen(buf) == 0 || strcmp(buf, "0") == 0) {
    printf("* LoRa selected.\n");
    modem = 0;
    askSF();
  } else if (strcmp(buf, "1") == 0) {
    printf("* FSK selected.\n");
    modem = 1;
    appStart();
  } else {
    printf("* Unknown modem\n");
    askModem();
  }
}

void setup(void) {
  Serial.begin(115200);
  printf("\n*** [ST Nucleo-F429ZI] SX1276 Low-Level Rx Control Example ***\n");

  pinMode(PC9, OUTPUT);
  digitalWrite(PC9, LOW);

#if 1
  Serial.listen();
  askModem();
#else
  modem = 0;
  sf = Radio::SF12;
  cr = Radio::CR_4_5;
  bw = Radio::BW_125kHz;
  iq = true;
  syncword = 0x34;
  appStart();
#endif
}
