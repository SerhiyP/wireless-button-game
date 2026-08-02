// =================================================================================
// nRF24L01 wiring / SPI diagnostic
//
// Upload to any button or display Nano when radio.begin() returns false
// ("Radio initialization failed!"). Serial Monitor at 9600 baud.
//
// begin() fails in exactly one way (RF24.cpp, _init_radio): it writes the CONFIG
// register, reads it back, and returns false if the value does not match. So a
// failure always means the MCU cannot reliably talk to the chip over SPI -- it is
// never an RF, antenna, PA-level or range problem. This sketch shows the raw
// bytes so the fault can be classified instead of guessed at.
//
// Uses the same CE/CSN pins as button.ino and display.ino.
// =================================================================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

const int CE_PIN = 9;
const int CSN_PIN = 10;

// ---- raw SPI access, independent of the RF24 library ----------------------------

// The filler byte is what MOSI drives while the chip clocks its answer back on
// MISO. Its value should be irrelevant -- if the reading changes with it, MISO is
// echoing MOSI (the two are shorted or swapped) rather than being driven by the chip.
uint8_t rawReadFilled(uint8_t reg, uint8_t filler) {
  digitalWrite(CSN_PIN, LOW);
  SPI.transfer(0x00 | (reg & 0x1F));  // R_REGISTER
  uint8_t value = SPI.transfer(filler);
  digitalWrite(CSN_PIN, HIGH);
  return value;
}

uint8_t rawRead(uint8_t reg) {
  return rawReadFilled(reg, 0xFF);
}

void rawWrite(uint8_t reg, uint8_t value) {
  digitalWrite(CSN_PIN, LOW);
  SPI.transfer(0x20 | (reg & 0x1F));  // W_REGISTER
  SPI.transfer(value);
  digitalWrite(CSN_PIN, HIGH);
}

void printHex(uint8_t v) {
  Serial.print("0x");
  if (v < 0x10) Serial.print('0');
  Serial.print(v, HEX);
}

void dumpRegisters() {
  Serial.println("--- raw registers, 1 MHz, before any RF24 call ---");

  const uint8_t regs[] = {0x00, 0x01, 0x03, 0x05, 0x06, 0x07};
  const char* names[] = {"CONFIG  ", "EN_AA   ", "SETUP_AW", "RF_CH   ", "RF_SETUP", "STATUS  "};

  for (uint8_t i = 0; i < 6; i++) {
    Serial.print("  ");
    Serial.print(names[i]);
    Serial.print(" = ");
    printHex(rawRead(regs[i]));
    Serial.println();
  }

  // Write-then-read-back on SETUP_AW: the same class of check begin() performs.
  rawWrite(0x03, 0x03);
  uint8_t back = rawRead(0x03);
  Serial.print("  write 0x03 to SETUP_AW -> read back ");
  printHex(back);
  Serial.println(back == 0x03 ? "  (SPI OK)" : "  (SPI BROKEN)");

  // Does the reading depend on what MOSI is driving? It must not.
  uint8_t withHigh = rawReadFilled(0x03, 0xFF);
  uint8_t withLow = rawReadFilled(0x03, 0x00);
  Serial.print("  SETUP_AW with MOSI filler 0xFF = ");
  printHex(withHigh);
  Serial.print(", with 0x00 = ");
  printHex(withLow);
  Serial.println();
  if (withHigh != withLow) {
    Serial.println("  -> MISO is echoing MOSI: the two lines are shorted or swapped");
  } else if (withHigh == 0xFF) {
    Serial.println("  -> MISO stuck HIGH independently of MOSI: line open, or module unpowered/dead");
  } else if (withHigh == 0x00) {
    Serial.println("  -> MISO stuck LOW: shorted to GND, or module held in reset");
  }
  Serial.println();
}

// Retry begin() at descending bus speeds. RF24 clocks the bus at 10 MHz by
// default; long or loose jumpers corrupt reads at that rate but survive 1 MHz.
void trySpeed(uint32_t hz) {
  Serial.print("=== RF24 begin() at ");
  Serial.print(hz);
  Serial.println(" Hz ===");

  RF24 radio(CE_PIN, CSN_PIN, hz);
  bool ok = radio.begin();

  Serial.print("  begin()           -> ");
  Serial.println(ok ? "TRUE" : "FALSE");
  Serial.print("  isChipConnected() -> ");
  Serial.println(radio.isChipConnected() ? "TRUE" : "FALSE");
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {}
  delay(200);

  Serial.println();
  Serial.println("nRF24 wiring / SPI diagnostic");
  Serial.println("=============================");

  pinMode(CE_PIN, OUTPUT);
  pinMode(CSN_PIN, OUTPUT);
  digitalWrite(CE_PIN, LOW);
  digitalWrite(CSN_PIN, HIGH);

  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  delay(100);  // nRF24 needs time to settle after power-up before it answers
  dumpRegisters();
  SPI.endTransaction();

  trySpeed(10000000);  // RF24_SPI_SPEED, what button.ino and display.ino use
  trySpeed(4000000);
  trySpeed(1000000);

  Serial.println("Interpretation:");
  Serial.println("  all 0xFF      -> MISO open, OR module unpowered/dead. Both look the");
  Serial.println("                   same here: check 3.3V at the module's own pins, and");
  Serial.println("                   re-run with the module unplugged -- identical output");
  Serial.println("                   means it was never on the bus.");
  Serial.println("  all 0x00      -> MISO shorted to GND, or CSN stuck LOW");
  Serial.println("  sane at 1 MHz -> SPI signal integrity: wires too long or loose");
  Serial.println("  random junk   -> bad contact, or supply brownout on the module");
}

void loop() {}