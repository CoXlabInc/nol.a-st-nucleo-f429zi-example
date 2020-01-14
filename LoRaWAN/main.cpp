#include <cox.h>
#include <algorithm>
#include <LoRaMacKR920SKT.hpp>
#include <ctype.h>
#include "SX1276Wiring.hpp"

#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_hid.h"
#include "usbd_desc.h"

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

/* In order to measure current in idle mode, define MEASURE_CURRENT_IN_SLEEP below. */
//#define MEASURE_CURRENT_IN_SLEEP

Timer timerHello;
Timer timerSend;

USBD_HandleTypeDef usb;

LoRaMacKR920SKT LoRaWAN = LoRaMacKR920SKT(SX1276);

#define OVER_THE_AIR_ACTIVATION 1

#if (OVER_THE_AIR_ACTIVATION == 1)
static const uint8_t devEui[] = "\x00\x11\x22\x33\x44\x55\x66\x77";
static const uint8_t appEui[] = "\x00\x00\x00\x00\x00\x00\x00\x00";
static const uint8_t appKey[] = "\xf9\x2b\x61\x4f\x43\x20\x1a\x97\xc7\x2a\x12\x7b\x96\x7d\x83\x3c";
#else

static const uint8_t NwkSKey[] = "\xa4\x88\x55\xad\xe9\xf8\xf4\x6f\xa0\x94\xb1\x98\x36\xc3\xc0\x86";
static const uint8_t AppSKey[] = "\x7a\x56\x2a\x75\xd7\xa3\xbd\x89\xa3\xde\x53\xe1\xcf\x7f\x1c\xc7";
static uint32_t DevAddr = 0x06e632e8;
#endif //OVER_THE_AIR_ACTIVATION

static void taskPeriodicSend(void *) {
  LoRaMacFrame *f = new LoRaMacFrame(255);
  if (!f) {
    printf("* Out of memory\n");
    return;
  }

  f->port = 1;
  f->type = LoRaMacFrame::CONFIRMED;
  strcpy((char *) f->buf, "Test");
  f->len = strlen((char *) f->buf);

  /* Uncomment below lines to specify parameters manually. */
  // f->freq = 922500000;
  // f->modulation = Radio::MOD_LORA;
  // f->meta.LoRa.bw = Radio::BW_125kHz;
  // f->meta.LoRa.sf = Radio::SF7;
  // f->power = 1; /* Index 1 => MaxEIRP - 2 dBm */
  f->numTrials = 5;

  error_t err = LoRaWAN.send(f);
  printf("* Sending periodic report (%p:%s (%u byte)): %d\n", f, f->buf, f->len, err);
  if (err != ERROR_SUCCESS) {
    delete f;
    timerSend.startOneShot(10000);
  }
}

static void eventLoRaWANJoin(
  LoRaMac &,
  bool joined,
  const uint8_t *joinedDevEui,
  const uint8_t *joinedAppEui,
  const uint8_t *joinedAppKey,
  const uint8_t *joinedNwkSKey,
  const uint8_t *joinedAppSKey,
  uint32_t joinedDevAddr,
  const RadioPacket &frame,
  uint32_t airTime
) {
#if (OVER_THE_AIR_ACTIVATION == 1)
  if (joined) {
    if (joinedNwkSKey && joinedAppSKey) {
      // Status is OK, node has joined the network
      printf("* Joined to the network!\n");
      postTask(taskPeriodicSend, NULL);
    } else {
      printf("* PseudoAppKey joining done!\n");
    }
  } else {
    printf("* Join failed. Retry to join\n");
    LoRaWAN.beginJoining(devEui, appEui, appKey);
  }
#endif
}

static void eventLoRaWANSendDone(LoRaMac &, LoRaMacFrame *frame) {
  printf("* Send done(%d): [%p] destined for port[%u], Freq:%lu Hz, Power:%d dBm, # of Tx:%u, ", frame->result, frame, frame->port, frame->freq, frame->power, frame->numTrials);
  if (frame->modulation == Radio::MOD_LORA) {
    const char *strBW[] = { "Unknown", "125kHz", "250kHz", "500kHz", "Unexpected value" };
    if (frame->meta.LoRa.bw > 3) {
      frame->meta.LoRa.bw = (Radio::LoRaBW_t) 4;
    }
    printf("LoRa, SF:%u, BW:%s, ", frame->meta.LoRa.sf, strBW[frame->meta.LoRa.bw]);
  } else if (frame->modulation == Radio::MOD_FSK) {
    printf("FSK, ");
  } else {
    printf("Unkndown modulation, ");
  }
  if (frame->type == LoRaMacFrame::UNCONFIRMED) {
    printf("UNCONFIRMED");
  } else if (frame->type == LoRaMacFrame::CONFIRMED) {
    printf("CONFIRMED");
  } else if (frame->type == LoRaMacFrame::MULTICAST) {
    printf("MULTICAST (error)");
  } else if (frame->type == LoRaMacFrame::PROPRIETARY) {
    printf("PROPRIETARY");
  } else {
    printf("unknown type");
  }
  printf(" frame\n");
  delete frame;

  timerSend.startOneShot(10000);
}

static void eventLoRaWANReceive(LoRaMac &lw, const LoRaMacFrame *frame) {
  printf("* Received:");
  for (uint8_t i = 0; i < frame->len; i++) {
    printf(" %02X", frame->buf[i]);
  }
  printf(" (");
  frame->printTo(Serial);
  printf(")\n");

  if (
    (frame->type == LoRaMacFrame::CONFIRMED || lw.framePending) &&
    lw.getNumPendingSendFrames() == 0
  ) {
    // If there is no pending send frames, send an empty frame to ack or pull more frames.
    LoRaMacFrame *emptyFrame = new LoRaMacFrame(0);
    if (emptyFrame) {
      error_t err = LoRaWAN.send(emptyFrame);
      if (err != ERROR_SUCCESS) {
        delete emptyFrame;
      }
    }
  }
}

static void eventLoRaWANJoinRequested(LoRaMac &, uint32_t frequencyHz, const LoRaMac::DatarateParams_t &dr) {
  printf("* JoinRequested(Frequency: %lu Hz, Modulation: ", frequencyHz);
  if (dr.mod == Radio::MOD_FSK) {
    printf("FSK\n");
  } else if (dr.mod == Radio::MOD_LORA) {
    const char *strLoRaBW[] = { "UNKNOWN", "125kHz", "250kHz", "500kHz" };
    printf("LoRa, SF:%u, BW:%s\n", dr.param.LoRa.sf, strLoRaBW[dr.param.LoRa.bw]);
  }
}

static void eventLoRaWANLinkADRReqReceived(LoRaMac &l, const uint8_t *payload) {
  printf("* LoRaWAN LinkADRReq received: [");
  for (uint8_t i = 0; i < 4; i++) {
    printf(" %02X", payload[i]);
  }
  printf(" ]\n");
}

static void printChannelInformation(LoRaMac &lw) {
  for (uint8_t i = 0; i < lw.MaxNumChannels; i++) {
    const LoRaMac::ChannelParams_t *p = lw.getChannel(i);
    if (p) {
      printf(" - [%u] Frequency:%lu Hz\n", i, p->Frequency);
    } else {
      printf(" - [%u] disabled\n", i);
    }
  }

  const LoRaMac::DatarateParams_t *dr = lw.getDatarate(lw.getCurrentDatarateIndex());
  printf(" - Default DR%u:", lw.getCurrentDatarateIndex());
  if (dr->mod == Radio::MOD_LORA) {
    printf("LoRa(SF%u BW:", dr->param.LoRa.sf);
    switch (dr->param.LoRa.bw) {
    case Radio::BW_125kHz: printf("125kHz\n"); break;
    case Radio::BW_250kHz: printf("250kHz\n"); break;
    case Radio::BW_500kHz: printf("500kHz\n"); break;
    default: printf("(unexpected:%u)\n", dr->param.LoRa.bw); break;
    }
  } else if (dr->mod == Radio::MOD_FSK) {
    printf("FSK\n");
  } else {
    printf("Unknown modulation\n");
  }

  printf(" - Default Tx index: %d\n", lw.getCurrentTxPowerIndex());
  printf(
    " - # of repetitions of unconfirmed uplink frames: %u\n",
    lw.getNumRepetitions()
  );
}

static void eventLoRaWANLinkADRAnsSent(LoRaMac &l, uint8_t status) {
  printf("* LoRaWAN LinkADRAns sent with status 0x%02X.\n", status);
  printChannelInformation(l);
}

static void eventLoRaWANDutyCycleReqReceived(LoRaMac &lw, const uint8_t *payload) {
  printf("* LoRaWAN DutyCycleReq received: [");
  for (uint8_t i = 0; i < 1; i++) {
    printf(" %02X", payload[i]);
  }
  printf(" ]\n");
}

static void eventLoRaWANDutyCycleAnsSent(LoRaMac &lw) {
  printf("* LoRaWAN DutyCycleAns sent. Current MaxDCycle is %u.\n", lw.getMaxDutyCycle());
}

static void eventLoRaWANRxParamSetupReqReceived(LoRaMac &lw, const uint8_t *payload) {
  printf("* LoRaWAN RxParamSetupReq received: [");
  for (uint8_t i = 0; i < 4; i++) {
    printf(" %02X", payload[i]);
  }
  printf(" ]\n");
}

static void eventLoRaWANRxParamSetupAnsSent(LoRaMac &lw, uint8_t status) {
  printf("* LoRaWAN RxParamSetupAns sent with status 0x%02X. Current Rx1Offset is %u, and Rx2 channel is (DR%u, %lu Hz).\n", status, lw.getRx1DrOffset(), lw.getRx2Datarate(), lw.getRx2Frequency());
}

static void eventLoRaWANDevStatusReqReceived(LoRaMac &lw) {
  printf("* LoRaWAN DevStatusReq received.\n");
}

static void eventLoRaWANDevStatusAnsSent(LoRaMac &lw, uint8_t bat, uint8_t margin) {
  printf("* LoRaWAN DevStatusAns sent. (");
  if (bat == 0) {
    printf("Powered by external power source. ");
  } else if (bat == 255) {
    printf("Battery level cannot be measured. ");
  } else {
    printf("Battery: %lu %%. ", map(bat, 1, 254, 0, 100));
  }

  if (bitRead(margin, 5) == 1) {
    margin |= bit(7) | bit(6);
  }

  printf(" SNR: %d)\n", (int8_t) margin);
}

static void eventLoRaWANNewChannelReqReceived(LoRaMac &lw, const uint8_t *payload) {
  printf("* LoRaWAN NewChannelReq received [");
  for (uint8_t i = 0; i < 5; i++) {
    printf(" %02X", payload[i]);
  }
  printf(" ]\n");
}

static void eventLoRaWANNewChannelAnsSent(LoRaMac &lw, uint8_t status) {
  printf("* LoRaWAN NewChannelAns sent with (Datarate range %s and channel frequency %s).\n", (bitRead(status, 1) == 1) ? "OK" : "NOT OK", (bitRead(status, 0) == 1) ? "OK" : "NOT OK");

  for (uint8_t i = 0; i < lw.MaxNumChannels; i++) {
    const LoRaMac::ChannelParams_t *p = lw.getChannel(i);
    if (p) {
      printf(" - [%u] Frequency:%lu Hz\n", i, p->Frequency);
    } else {
      printf(" - [%u] disabled\n", i);
    }
  }
}

static void eventLoRaWANRxTimingSetupReqReceived(LoRaMac &lw, const uint8_t *payload) {
  printf("* LoRaWAN RxTimingSetupReq received:  [");
  for (uint8_t i = 0; i < 1; i++) {
    printf(" %02X", payload[i]);
  }
  printf(" ]\n");
}

static void eventLoRaWANRxTimingSetupAnsSent(LoRaMac &lw) {
  printf("* LoRaWAN RxTimingSetupAns sent. Current Rx1 delay is %u msec, and Rx2 delay is %u msec.\n", lw.getRx1Delay(), lw.getRx2Delay());
}

static void taskHello(void *) {
  Serial.println("Serial) Hello World!");
  Serial2.println("Serial2) Hello World!");

  digitalToggle(PB14);
}

static void moveCursor(int8_t x, int8_t y) {
  uint8_t buf[4];
  buf[0] = 0;
  buf[1] = x;
  buf[2] = y;
  buf[3] = 0;
  USBD_HID_SendReport(&usb, buf, 4);
}

static void changeClass() {
  LoRaMacFrame *f = new LoRaMacFrame(20);
  if (f == NULL) {
    printf("- Not enough memory\n");
    return;
  }

  if (LoRaWAN.getDeviceClass() == LoRaMac::CLASS_A) {
    printf("- Change class A to C\n");
    f->len = sprintf((char *) f->buf, "\"class\":\"C\"");
  } else {
    printf("- Change class C to A\n");
    f->len = sprintf((char *) f->buf, "\"class\":\"A\"");
  }

  f->port = 223;
  f->type = LoRaMacFrame::CONFIRMED;
  error_t err = LoRaWAN.send(f);
  printf(
    "* Sending class configuration message (%s (%u byte)): %d\n",
    f->buf, f->len, err
  );
  if (err == ERROR_SUCCESS) {
    if (LoRaWAN.getDeviceClass() == LoRaMac::CLASS_A) {
      LoRaWAN.setDeviceClass(LoRaMac::CLASS_C);
    } else {
      LoRaWAN.setDeviceClass(LoRaMac::CLASS_A);
    }
  } else {
    delete f;
  }
}

static void eventSerialRx(SerialPort &p) {
  while (p.available() > 0) {
    char c = p.read();
    if (isprint(c)) {
      p.write(c);
    }
    digitalToggle(PB7);

    switch(c) {
      case 'a':
      case 'A':
      // Left
      moveCursor(-5, 0);
      break;

      case 'w':
      case 'W':
      // Up
      moveCursor(0, -5);
      break;

      case 's':
      case 'S':
      // Down
      moveCursor(0, 5);
      break;

      case 'd':
      case 'D':
      // Right
      moveCursor(5, 0);
      break;

      case 'c':
      case 'C':
      changeClass();
      break;
    }
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

  timerHello.onFired(taskHello, NULL);
  timerHello.startPeriodic(1000);

  timerSend.onFired(taskPeriodicSend, NULL);

  LoRaWAN.begin();

  //! [How to set onSendDone callback]
  LoRaWAN.onSendDone(eventLoRaWANSendDone);
  //! [How to set onSendDone callback]

  //! [How to set onReceive callback]
  LoRaWAN.onReceive(eventLoRaWANReceive);
  //! [How to set onReceive callback]

  LoRaWAN.onJoin(eventLoRaWANJoin);

  //! [How to set onJoinRequested callback]
  LoRaWAN.onJoinRequested(eventLoRaWANJoinRequested);
  //! [How to set onJoinRequested callback]

  LoRaWAN.onLinkADRReqReceived(eventLoRaWANLinkADRReqReceived);
  LoRaWAN.onLinkADRAnsSent(eventLoRaWANLinkADRAnsSent);
  LoRaWAN.onDutyCycleReqReceived(eventLoRaWANDutyCycleReqReceived);
  LoRaWAN.onDutyCycleAnsSent(eventLoRaWANDutyCycleAnsSent);
  LoRaWAN.onRxParamSetupReqReceived(eventLoRaWANRxParamSetupReqReceived);
  LoRaWAN.onRxParamSetupAnsSent(eventLoRaWANRxParamSetupAnsSent);
  LoRaWAN.onDevStatusReqReceived(eventLoRaWANDevStatusReqReceived);
  LoRaWAN.onDevStatusAnsSent(eventLoRaWANDevStatusAnsSent);
  LoRaWAN.onNewChannelReqReceived(eventLoRaWANNewChannelReqReceived);
  LoRaWAN.onNewChannelAnsSent(eventLoRaWANNewChannelAnsSent);
  LoRaWAN.onRxTimingSetupReqReceived(eventLoRaWANRxTimingSetupReqReceived);
  LoRaWAN.onRxTimingSetupAnsSent(eventLoRaWANRxTimingSetupAnsSent);

  LoRaWAN.setPublicNetwork(false);

  printChannelInformation(LoRaWAN);

  #if (OVER_THE_AIR_ACTIVATION == 0)
  printf("ABP!\n");
  LoRaWAN.setABP(NwkSKey, AppSKey, DevAddr);
  LoRaWAN.setNetworkJoined(true);

  postTask(taskPeriodicSend, NULL);
  #else
  printf("Trying to join\n");

  #if 0
  //! [SKT PseudoAppKey joining]
  LoRaWAN.setNetworkJoined(false);
  LoRaWAN.beginJoining(devEui, appEui, appKey);
  //! [SKT PseudoAppKey joining]
  #else
  //! [SKT RealAppKey joining]
  LoRaWAN.setNetworkJoined(true);
  LoRaWAN.beginJoining(devEui, appEui, appKey);
  //! [SKT RealAppKey joining]
  #endif
  #endif
}
