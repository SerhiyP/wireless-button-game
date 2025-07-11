#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);  // CE, CSN

const byte address[6] = "00001";
const int redLedPin = 4;
const int blueLedPin = 5;
const int resetPin = 6;

bool winnerChosen = false;
char incoming[32];

void setup() {
  pinMode(redLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);
  pinMode(resetPin, INPUT_PULLUP);

  digitalWrite(redLedPin, LOW);
  digitalWrite(blueLedPin, LOW);

  Serial.begin(9600);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.openReadingPipe(0, address);
  radio.startListening();  // Приймач
}

void loop() {
  if (radio.available() && !winnerChosen) {
    radio.read(&incoming, sizeof(incoming));
    if (strcmp(incoming, "RED") == 0) {
      digitalWrite(redLedPin, HIGH);
      winnerChosen = true;
      Serial.println("Переміг ЧЕРВОНИЙ");
    } else if (strcmp(incoming, "BLUE") == 0) {
      digitalWrite(blueLedPin, HIGH);
      winnerChosen = true;
      Serial.println("Переміг СИНІЙ");
    }
  }

  // Скидання гри кнопкою
  if (digitalRead(resetPin) == LOW) {
    winnerChosen = false;
    digitalWrite(redLedPin, LOW);
    digitalWrite(blueLedPin, LOW);
    Serial.println("Скидання");
    delay(300);
  }
}