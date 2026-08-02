// =================================================================================
// Nano self-test: are the radio pins and the SPI peripheral sound?
//
// Tests the microcontroller and its wiring WITHOUT a radio module attached, so a
// failure here is unambiguously the board rather than the nRF24. Run this before
// radio_check when it is unclear which side of the SPI bus is at fault.
//
// DISCONNECT THE RADIO MODULE FIRST. Every wire from the module must come off --
// with it attached the module drives MISO and the results are meaningless.
//
// An attached module fakes shorts, so read the results with that in mind. Even
// unpowered, an nRF24 ties its I/O pins together through the ESD diodes into its
// dead VDD rail: drive CSN low, the parasitic supply sags, and MISO follows it
// down. That reports as a D10 <-> D12 short on a perfectly good board. The tell
// is that the baseline contradicts it -- a real copper short between two pins
// forces BOTH low at baseline, while these paths only conduct while a pin is
// being driven. If the module cannot easily be unsoldered, run this on a board
// known to work and compare: whatever appears on both is the module, not a fault.
//
// Serial Monitor at 9600 baud.
// =================================================================================

#include <SPI.h>

const uint8_t PIN_COUNT = 5;
const uint8_t RADIO_PINS[PIN_COUNT] = {9, 10, 11, 12, 13};
const char* RADIO_NAMES[PIN_COUNT] = {
  "D9  (CE)  ", "D10 (CSN) ", "D11 (MOSI)", "D12 (MISO)", "D13 (SCK) "};

uint8_t baseline[PIN_COUNT];

void allPinsToPullup() {
  for (uint8_t i = 0; i < PIN_COUNT; i++) {
    pinMode(RADIO_PINS[i], INPUT_PULLUP);
  }
  delay(5);  // let the ~30k pull-ups charge the pin capacitance
}

// A healthy, unconnected pin reads HIGH through its internal pull-up. D13 is the
// exception on most Nano clones: the onboard L LED sits directly on it through a
// ~1k resistor, which drags the pin below the logic threshold. D13 reading LOW
// here is normal and not a fault.
void readBaseline() {
  Serial.println("--- pin baseline (internal pull-ups, nothing driving) ---");

  allPinsToPullup();
  for (uint8_t i = 0; i < PIN_COUNT; i++) {
    baseline[i] = digitalRead(RADIO_PINS[i]);
    Serial.print("  ");
    Serial.print(RADIO_NAMES[i]);
    Serial.print(" = ");
    Serial.print(baseline[i] ? "HIGH" : "LOW ");
    if (!baseline[i]) {
      Serial.print(RADIO_PINS[i] == 13 ? "   (normal: onboard L LED loads D13)"
                                       : "   <-- SUSPECT: shorted to GND?");
    }
    Serial.println();
  }
  Serial.println();
}

// Drive one pin LOW and see whether any other pin follows it down. Two pins that
// move together are shorted -- a solder bridge between adjacent header pins, or
// two wires sharing a breadboard row. D11/D12 are neighbours on the header and
// are the pair most worth knowing about.
void shortTest() {
  Serial.println("--- pin-to-pin short test ---");
  bool anyFound = false;

  for (uint8_t driven = 0; driven < PIN_COUNT; driven++) {
    allPinsToPullup();
    pinMode(RADIO_PINS[driven], OUTPUT);
    digitalWrite(RADIO_PINS[driven], LOW);
    delay(5);

    for (uint8_t other = 0; other < PIN_COUNT; other++) {
      if (other == driven) continue;
      // Only a pin that STARTED high can tell us anything by going low.
      if (baseline[other] && digitalRead(RADIO_PINS[other]) == LOW) {
        Serial.print("  SHORT: ");
        Serial.print(RADIO_NAMES[driven]);
        Serial.print(" <-> ");
        Serial.println(RADIO_NAMES[other]);
        anyFound = true;
      }
    }
  }

  allPinsToPullup();
  if (anyFound) {
    Serial.println("  ^^ real ONLY if the module is fully disconnected.");
    Serial.println("     An attached nRF24 fakes D10 <-> D12 through its ESD diodes.");
    Serial.println("     Cross-check: a copper short forces both pins LOW at baseline.");
  } else {
    Serial.println("  no shorts between D9-D13");
  }
  Serial.println();
}

// Confirms D13 can actually drive. Watch the onboard L LED.
void blinkTest() {
  Serial.println("--- D13 drive test: L LED should blink 5 times ---");
  pinMode(13, OUTPUT);
  for (uint8_t i = 0; i < 5; i++) {
    digitalWrite(13, HIGH);
    delay(200);
    digitalWrite(13, LOW);
    delay(200);
  }
  Serial.println("  (if L never lit, D13 or the LED is dead)");
  Serial.println();
}

// Can each pin actually drive? On AVR, reading a pin configured as OUTPUT returns
// the real electrical level on the pad, not the value we wrote -- so this catches
// a damaged output stage, or a pin loaded down hard enough that it cannot reach
// its rail. It also separates a dead pull-up (pin reads LOW as an input but still
// drives fine) from a dead pin (cannot drive either).
void driveTest() {
  Serial.println("--- output drive test ---");

  for (uint8_t i = 0; i < PIN_COUNT; i++) {
    pinMode(RADIO_PINS[i], OUTPUT);
    digitalWrite(RADIO_PINS[i], HIGH);
    delayMicroseconds(100);
    uint8_t drivenHigh = digitalRead(RADIO_PINS[i]);

    digitalWrite(RADIO_PINS[i], LOW);
    delayMicroseconds(100);
    uint8_t drivenLow = digitalRead(RADIO_PINS[i]);

    pinMode(RADIO_PINS[i], INPUT);

    Serial.print("  ");
    Serial.print(RADIO_NAMES[i]);
    if (drivenHigh && !drivenLow) {
      Serial.println(" drives HIGH and LOW - OK");
    } else if (!drivenHigh && !drivenLow) {
      Serial.println(" cannot reach HIGH  <-- pin damaged, or shorted to GND");
    } else if (drivenHigh && drivenLow) {
      Serial.println(" cannot reach LOW   <-- pin damaged, or shorted to VCC");
    } else {
      Serial.println(" inverted reading   <-- unexpected, retest");
    }
  }
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {}
  delay(200);

  Serial.println();
  Serial.println("Nano self-test (radio module must be DISCONNECTED)");
  Serial.println("=================================================");

  readBaseline();
  shortTest();
  driveTest();
  blinkTest();

  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  // Hold MISO up through its internal pull-up. Without this an unconnected D12 is
  // high-impedance and capacitively follows D11 right next to it, faking a
  // successful loopback -- the exact effect that made a loose MISO look like a
  // hard short earlier. The jumper drives far harder than a ~30k pull-up, so a
  // real loopback still wins.
  digitalWrite(MISO, HIGH);

  Serial.println("--- SPI loopback, running continuously ---");
  Serial.println("Jumper D11 (MOSI) to D12 (MISO) to test the SPI peripheral.");
  Serial.println("Expect LOOPBACK OK with the jumper, NO ECHO without it.");
  Serial.println("Add and remove it while this runs -- the reading must follow.");
  Serial.println();
}

void loop() {
  // Two complementary patterns: a line stuck high or low matches one of them by
  // accident, but cannot match both.
  uint8_t a = SPI.transfer(0xA5);
  uint8_t b = SPI.transfer(0x5A);

  Serial.print("  sent 0xA5,0x5A  got 0x");
  if (a < 0x10) Serial.print('0');
  Serial.print(a, HEX);
  Serial.print(",0x");
  if (b < 0x10) Serial.print('0');
  Serial.print(b, HEX);

  if (a == 0xA5 && b == 0x5A) {
    Serial.println("   LOOPBACK OK - SPI, D11 and D12 all good");
  } else if (a == 0xFF && b == 0xFF) {
    Serial.println("   no jumper (pull-up holding MISO) - expected with it off");
  } else if (a == 0x00 && b == 0x00) {
    Serial.println("   MISO stuck LOW - shorted to GND");
  } else {
    Serial.println("   partial/garbage - marginal contact on D11, D12 or SCK");
  }

  delay(1000);
}