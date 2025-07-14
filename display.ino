#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);

const byte address[6] = "00001";
const int redLedPin = 4;
const int blueLedPin = 5;
const int resetPin = 6;
const int statusLedPin = 7;
const int readyLedPin = 3;
const int connectionLedPin = 8;

bool winnerChosen = false;
char incoming[32];
char outgoing[32];
unsigned long lastResetTime = 0;
unsigned long resetDebounceDelay = 300;
unsigned long gameStartTime = 0;
bool gameActive = false;
bool systemReady = false;
unsigned long lastReadyBroadcast = 0;
unsigned long readyBroadcastInterval = 1000;

void setup() {
  pinMode(redLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(statusLedPin, OUTPUT);
  pinMode(readyLedPin, OUTPUT);
  pinMode(connectionLedPin, OUTPUT);

  digitalWrite(redLedPin, LOW);
  digitalWrite(blueLedPin, LOW);
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
  radio.openReadingPipe(0, address);
  radio.openWritingPipe(address);
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
      sendWinConfirmation("WIN_RED");
      unsigned long gameTime = millis() - gameStartTime;
      Serial.print("ЧЕРВОНИЙ ПЕРЕМІГ! Час гри: ");
      Serial.print(gameTime);
      Serial.println(" мс");
    } else if (strcmp(incoming, "BLUE") == 0) {
      digitalWrite(blueLedPin, HIGH);
      winnerChosen = true;
      sendWinConfirmation("WIN_BLUE");
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
  
  updateLEDs();
}

void resetGame() {
  winnerChosen = false;
  gameActive = true;
  gameStartTime = millis();
  
  digitalWrite(redLedPin, LOW);
  digitalWrite(blueLedPin, LOW);
  digitalWrite(statusLedPin, LOW);
  digitalWrite(readyLedPin, HIGH);
  
  Serial.println("=== ГРА ПОЧАЛАСЯ! ===");
  Serial.println("Натисніть червону або синю кнопку!");
  
  delay(100);
  digitalWrite(statusLedPin, HIGH);
}

void broadcastSystemReady() {
  radio.stopListening();
  strcpy(outgoing, "SYSTEM_READY");
  
  digitalWrite(connectionLedPin, LOW);
  delay(10);
  
  bool result = radio.write(&outgoing, sizeof(outgoing));
  
  digitalWrite(connectionLedPin, HIGH);
  
  if (result) {
    Serial.println("SYSTEM_READY signal broadcasted");
  } else {
    Serial.println("Failed to broadcast SYSTEM_READY");
  }
  
  radio.startListening();
}

void sendWinConfirmation(const char* message) {
  radio.stopListening();
  strcpy(outgoing, message);
  
  digitalWrite(connectionLedPin, LOW);
  delay(10);
  
  bool result = radio.write(&outgoing, sizeof(outgoing));
  
  digitalWrite(connectionLedPin, HIGH);
  
  if (result) {
    Serial.print("Win confirmation sent: ");
    Serial.println(message);
  } else {
    Serial.print("Failed to send win confirmation: ");
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
