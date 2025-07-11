#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);

const byte address[6] = "00001";
const int redLedPin = 4;
const int blueLedPin = 5;
const int resetPin = 6;
const int statusLedPin = 7;

bool winnerChosen = false;
char incoming[32];
unsigned long lastResetTime = 0;
unsigned long resetDebounceDelay = 300;
unsigned long gameStartTime = 0;
bool gameActive = false;

void setup() {
  pinMode(redLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(statusLedPin, OUTPUT);

  digitalWrite(redLedPin, LOW);
  digitalWrite(blueLedPin, LOW);
  digitalWrite(statusLedPin, LOW);

  Serial.begin(9600);
  Serial.println("Display Unit Starting...");

  if (!radio.begin()) {
    Serial.println("Radio initialization failed!");
    while(1) {
      digitalWrite(statusLedPin, HIGH);
      delay(100);
      digitalWrite(statusLedPin, LOW);
      delay(100);
    }
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.openReadingPipe(0, address);
  radio.startListening();
  
  digitalWrite(statusLedPin, HIGH);
  Serial.println("Display Unit Ready! Press reset to start game.");
}

void loop() {
  if (radio.available() && !winnerChosen && gameActive) {
    memset(incoming, 0, sizeof(incoming));
    radio.read(&incoming, sizeof(incoming));
    
    Serial.print("Received: ");
    Serial.println(incoming);
    
    if (strcmp(incoming, "RED") == 0) {
      digitalWrite(redLedPin, HIGH);
      winnerChosen = true;
      unsigned long gameTime = millis() - gameStartTime;
      Serial.print("ЧЕРВОНИЙ ПЕРЕМІГ! Час гри: ");
      Serial.print(gameTime);
      Serial.println(" мс");
    } else if (strcmp(incoming, "BLUE") == 0) {
      digitalWrite(blueLedPin, HIGH);
      winnerChosen = true;
      unsigned long gameTime = millis() - gameStartTime;
      Serial.print("СИНІЙ ПЕРЕМІГ! Час гри: ");
      Serial.print(gameTime);
      Serial.println(" мс");
    } else {
      Serial.print("Невідоме повідомлення: ");
      Serial.println(incoming);
    }
  }

  if (digitalRead(resetPin) == LOW && (millis() - lastResetTime > resetDebounceDelay)) {
    resetGame();
    lastResetTime = millis();
  }
  
  if (gameActive && !winnerChosen) {
    if ((millis() / 200) % 2) {
      digitalWrite(statusLedPin, HIGH);
    } else {
      digitalWrite(statusLedPin, LOW);
    }
  } else if (winnerChosen) {
    digitalWrite(statusLedPin, HIGH);
  }
}

void resetGame() {
  winnerChosen = false;
  gameActive = true;
  gameStartTime = millis();
  
  digitalWrite(redLedPin, LOW);
  digitalWrite(blueLedPin, LOW);
  digitalWrite(statusLedPin, LOW);
  
  Serial.println("=== ГРА ПОЧАЛАСЯ! ===");
  Serial.println("Натисніть червону або синю кнопку!");
  
  delay(100);
  digitalWrite(statusLedPin, HIGH);
}
