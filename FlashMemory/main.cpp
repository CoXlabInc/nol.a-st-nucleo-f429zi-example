#include <cox.h>

//! [How to declare]
#include <stm32f4/STM32F4xxFlashSector.hpp>

STM32F4xxFlashSector sector(0x081E0000, 128 * 1024);
//! [How to declare]

char keyBuf[256];

static void printMenu() {
  printf("Usage:\n");
  printf("* read {addr} {len}\n");
  printf("* write {addr} {single byte of data}\n");
  printf("* length\n");
  printf("* writestring {addr} {string}\n");
  printf("* write1k {addr} {single byte of data}\n");
}

static uint8_t copyUntil(char *dst, char *src, uint8_t len, char terminator) {
  uint8_t i, j = 0;

  for (i = 0; i < len; i++) {
    if (src[i] == terminator) {
      i++;
      dst[j++] = '\0';
      break;
    } else {
      dst[j++] = src[i];
    }
  }

  return i;
}

static void eventSerialReceived(SerialPort &) {
  char tmpBuf[256];
  uint8_t i;

  if (strcmp(keyBuf, "length") == 0) {
    printf("* length: 0x%lX\n", (uint32_t) sector.length());
  } else if (strncmp(keyBuf, "read ", 5) == 0) {
    uint32_t addr, len;

    i = 5;
    i += copyUntil(tmpBuf, &keyBuf[i], 255 - i, ' ');
    addr = strtoul(tmpBuf, NULL, 0);

    i += copyUntil(tmpBuf, &keyBuf[i], 255 - i, '\0');
    len = strtoul(tmpBuf, NULL, 0);

    printf("* read %lu bytes from address 0x%lX:\n", len, addr);

    if (len > 0) {
      uint8_t *dump = new uint8_t[len];
      if (dump) {
        sector.read(dump, addr, len);

        for (uint32_t x = 0; x < len; x++) {
          uint8_t index = x % 16;

          if (index == 0) {
            printf("0x%08lX |", addr + x);
          }
          printf(" %02X", dump[x]);
          if (index == 7) {
            printf(" ");
          } else if (x % 16 == 15) {
            printf("\n");
          }
        }
        printf("\n");
        delete[] dump;
      }
    }
  } else if (strncmp(keyBuf, "write ", 6) == 0) {
    uint32_t addr;
    uint8_t data;

    i = 6;
    i += copyUntil(tmpBuf, &keyBuf[i], 255 - i, ' ');
    addr = strtoul(tmpBuf, NULL, 0);

    i += copyUntil(tmpBuf, &keyBuf[i], 255 - i, '\0');
    data = (uint8_t) strtoul(tmpBuf, NULL, 0);

    printf("* write 0x%02X to address 0x%lX:\n", data, addr);

    sector.write(addr, data);
  } else if (strncmp(keyBuf, "write1k ", 8) == 0) {
    uint32_t addr;
    uint8_t data;
    uint8_t dataBlock[1024];

    i = 8;
    i += copyUntil(tmpBuf, &keyBuf[i], 255 - i, ' ');
    addr = strtoul(tmpBuf, NULL, 0);

    i += copyUntil(tmpBuf, &keyBuf[i], 255 - i, '\0');
    data = (uint8_t) strtoul(tmpBuf, NULL, 0);

    printf("* write1k [0x%02X 0x%02X ...] to address 0x%lX~0x%lX:\n", data, data + 1, addr, addr + 1024);

    for (uint32_t x = 0; x < 1024; x++) {
      dataBlock[x] = data + x;
    }

    sector.write(dataBlock, addr, 1024);

  } else if (strncmp(keyBuf, "writestring ", 12) == 0) {
    uint32_t addr;
    const char *data;

    i = 12;
    i += copyUntil(tmpBuf, &keyBuf[i], 255 - i, ' ');
    addr = strtoul(tmpBuf, NULL, 0);

    data = keyBuf + i;
    printf("* writestring \"%s\" to address 0x%lX:\n", data, addr);

    sector.write((const uint8_t *) data, addr, strlen(data));
  } else {
    printf("* Unknown command\n");
  }

  printf("\n");
  printMenu();
  Serial.inputKeyboard(keyBuf, sizeof(keyBuf) - 1);
}

void setup() {
  Serial.begin(115200);
  printf("\n*** [ST Nucleo-F429ZI] Internal Flash Memory (sector) Test ***\n");

  Serial.onReceive(eventSerialReceived);
  Serial.inputKeyboard(keyBuf, sizeof(keyBuf) - 1);
  Serial.listen();
  printMenu();
}
