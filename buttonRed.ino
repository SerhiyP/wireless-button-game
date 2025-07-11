#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);  // CE, CSN

const byte address[6] = "00001";
const int buttonPin = 2;
const int ledPin = 4;

bool sent = false;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.openWritingPipe(address);
  radio.stopListening();  // Передавач

  digitalWrite(ledPin, LOW);  // LED вимкнений
}

void loop() {
  if (digitalRead(buttonPin) == LOW && !sent) {
    const char text[] = "RED";
    radio.write(&text, sizeof(text));
    sent = true;
  }
}