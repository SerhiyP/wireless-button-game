#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);

const byte displayAddress[6] = "00001";    // Display receives on this address
const byte redButtonAddress[6] = "00002";  // Red button receives on this address
const byte blueButtonAddress[6] = "00003"; // Blue button receives on this address
const byte greenButtonAddress[6] = "00004"; // Green button receives on this address

const int redLedPin = 4;
const int blueLedPin = 5;
const int greenLedPin = 2;
const int resetPin = 6;
const int statusLedPin = 7;
const int readyLedPin = 3;
const int connectionLedPin = 8;

bool winnerChosen = false;
char incoming[16];
char outgoing[16];
unsigned long lastResetTime = 0;
unsigned long resetDebounceDelay = 300;
unsigned long gameStartTime = 0;
unsigned long gameEndTime = 0;
bool gameActive = false;
bool systemReady = false;

void setup() {
  pinMode(redLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(statusLedPin, OUTPUT);
  pinMode(readyLedPin, OUTPUT);
  pinMode(connectionLedPin, OUTPUT);

  digitalWrite(redLedPin, LOW);
  digitalWrite(blueLedPin, LOW);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(statusLedPin, LOW);
  digitalWrite(readyLedPin, LOW);
  digitalWrite(connectionLedPin, LOW);

  Serial.begin(9600);
  Serial.println("Display Unit Starting...");

  digitalWrite(connectionLedPin, HIGH);
  delay(100);
  digitalWrite(connectionLedPin, LOW);
  delay(100);

  if (!radio.begin()) {
    Serial.println("Radio initialization failed!");
    while(1) {
      digitalWrite(statusLedPin, HIGH);
      digitalWrite(connectionLedPin, HIGH);
      delay(100);
      digitalWrite(statusLedPin, LOW);
      digitalWrite(connectionLedPin, LOW);
      delay(100);
    }
  }

  radio.setPALevel(RF24_PA_HIGH);
  radio.openReadingPipe(0, displayAddress);
  radio.startListening();
  
  digitalWrite(connectionLedPin, HIGH);
  digitalWrite(statusLedPin, HIGH);
  
  broadcastSystemReady();
  systemReady = true;
  
  Serial.println("Display Unit Ready! Press reset to start game.");
}

void loop() {
  if (radio.available() && !winnerChosen && gameActive) {
    memset(incoming, 0, sizeof(incoming));
    radio.read(&incoming, sizeof(incoming));
    
    digitalWrite(connectionLedPin, LOW);
    delay(10);
    digitalWrite(connectionLedPin, HIGH);
    
    Serial.print("Received: ");
    Serial.println(incoming);
    
    if (strcmp(incoming, "RED") == 0) {
      digitalWrite(redLedPin, HIGH);
      winnerChosen = true;
      gameEndTime = millis();
      sendToRedButton("WIN_RED");
      unsigned long gameTime = gameEndTime - gameStartTime;
      Serial.print("ЧЕРВОНИЙ ПЕРЕМІГ! Час гри: ");
      Serial.print(gameTime);
      Serial.println(" мс");
    } else if (strcmp(incoming, "BLUE") == 0) {
      digitalWrite(blueLedPin, HIGH);
      winnerChosen = true;
      gameEndTime = millis();
      sendToBlueButton("WIN_BLUE");
      unsigned long gameTime = gameEndTime - gameStartTime;
      Serial.print("СИНІЙ ПЕРЕМІГ! Час гри: ");
      Serial.print(gameTime);
      Serial.println(" мс");
    } else if (strcmp(incoming, "GREEN") == 0) {
      digitalWrite(greenLedPin, HIGH);
      winnerChosen = true;
      gameEndTime = millis();
      sendToGreenButton("WIN_GREEN");
      unsigned long gameTime = gameEndTime - gameStartTime;
      Serial.print("ЗЕЛЕНИЙ ПЕРЕМІГ! Час гри: ");
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
  
  updateLEDs();
}

void resetGame() {
  winnerChosen = false;
  gameActive = true;
  gameStartTime = millis();
  
  digitalWrite(redLedPin, LOW);
  digitalWrite(blueLedPin, LOW);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(statusLedPin, LOW);
  digitalWrite(readyLedPin, HIGH);
  
  // Send reset signal to all buttons
  sendToRedButton("GAME_RESET");
  sendToBlueButton("GAME_RESET");
  sendToGreenButton("GAME_RESET");
  
  Serial.println("=== НОВА ГРА ПОЧАЛАСЯ! ===");
  Serial.println("Натисніть червону, синю або зелену кнопку!");
  
  delay(100);
  digitalWrite(statusLedPin, HIGH);
}

void broadcastSystemReady() {
  // Send SYSTEM_READY to all buttons individually
  sendToRedButton("SYSTEM_READY");
  delay(50);
  sendToBlueButton("SYSTEM_READY");
  delay(50);
  sendToGreenButton("SYSTEM_READY");
  Serial.println("SYSTEM_READY signals sent to all buttons");
}

void sendToRedButton(const char* message) {
  radio.stopListening();
  radio.openWritingPipe(redButtonAddress);
  strcpy(outgoing, message);
  
  digitalWrite(connectionLedPin, LOW);
  delay(10);
  
  bool result = radio.write(&outgoing, sizeof(outgoing));
  
  digitalWrite(connectionLedPin, HIGH);
  
  if (result) {
    Serial.print("Message sent to RED button: ");
    Serial.println(message);
  } else {
    Serial.print("Failed to send to RED button: ");
    Serial.println(message);
  }
  
  radio.startListening();
}

void sendToBlueButton(const char* message) {
  radio.stopListening();
  radio.openWritingPipe(blueButtonAddress);
  strcpy(outgoing, message);
  
  digitalWrite(connectionLedPin, LOW);
  delay(10);
  
  bool result = radio.write(&outgoing, sizeof(outgoing));
  
  digitalWrite(connectionLedPin, HIGH);
  
  if (result) {
    Serial.print("Message sent to BLUE button: ");
    Serial.println(message);
  } else {
    Serial.print("Failed to send to BLUE button: ");
    Serial.println(message);
  }
  
  radio.startListening();
}

void sendToGreenButton(const char* message) {
  radio.stopListening();
  radio.openWritingPipe(greenButtonAddress);
  strcpy(outgoing, message);
  
  digitalWrite(connectionLedPin, LOW);
  delay(10);
  
  bool result = radio.write(&outgoing, sizeof(outgoing));
  
  digitalWrite(connectionLedPin, HIGH);
  
  if (result) {
    Serial.print("Message sent to GREEN button: ");
    Serial.println(message);
  } else {
    Serial.print("Failed to send to GREEN button: ");
    Serial.println(message);
  }
  
  radio.startListening();
}

void updateLEDs() {
  if (gameActive && !winnerChosen) {
    digitalWrite(readyLedPin, HIGH);
    if ((millis() / 200) % 2) {
      digitalWrite(statusLedPin, HIGH);
    } else {
      digitalWrite(statusLedPin, LOW);
    }
  } else if (winnerChosen) {
    digitalWrite(statusLedPin, HIGH);
    if ((millis() / 300) % 2) {
      digitalWrite(readyLedPin, HIGH);
    } else {
      digitalWrite(readyLedPin, LOW);
    }
  } else {
    digitalWrite(readyLedPin, LOW);
    digitalWrite(statusLedPin, HIGH);
  }
}
