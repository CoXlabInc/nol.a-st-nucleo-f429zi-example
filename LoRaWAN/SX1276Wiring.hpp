#include "cox.h"
#include "SX1276Chip.hpp"
#include "SPI.hpp"

#ifndef SX1276WIRING_HPP
#define SX1276WIRING_HPP

class SX1276Wiring : public SX1276Chip {
public:
  SX1276Wiring(
    SPI &,
    int8_t pinReset,
    int8_t pinCS,
    int8_t pinRxTx,
    int8_t pinDIO0,
    int8_t pinDIO1,
    int8_t pinDIO2,
    int8_t pinDIO3,
    int8_t pinDIO4
  );

protected:
  bool usingPaBoost(uint32_t channel);
};

#endif //SX1276WIRING_HPP
