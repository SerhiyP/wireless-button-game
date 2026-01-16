#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);

const byte displayAddress[6] = "00001";    // Display receives on this address

// Button Radio Addresses
const byte redButtonAddress[6] = "00002";
const byte blueButtonAddress[6] = "00003";
const byte greenButtonAddress[6] = "00004";

// Protocol Messages
const char* MSG_RED = "RED";
const char* MSG_BLUE = "BLUE";
const char* MSG_GREEN = "GREEN";
const char* MSG_WIN_RED = "WIN_RED";
const char* MSG_WIN_BLUE = "WIN_BLUE";
const char* MSG_WIN_GREEN = "WIN_GREEN";
const char* MSG_GAME_RESET = "GAME_RESET";
const char* MSG_SYSTEM_READY = "SYSTEM_READY";

// Struct to hold all info for a button
struct Button {
  const char* name;
  const byte* address;
  int ledPin;
  const char* winMessage;
  const char* displayName;
};

// Array of all buttons. Add new buttons here.
Button buttons[] = {
  {MSG_RED,   redButtonAddress,   4, MSG_WIN_RED,   "ЧЕРВОНИЙ"},
  {MSG_BLUE,  blueButtonAddress,  5, MSG_WIN_BLUE,  "СИНІЙ"},
  {MSG_GREEN, greenButtonAddress, 2, MSG_WIN_GREEN, "ЗЕЛЕНИЙ"}
};

const int numButtons = sizeof(buttons) / sizeof(buttons[0]);

// Pin Definitions
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
  for (int i = 0; i < numButtons; i++) {
    pinMode(buttons[i].ledPin, OUTPUT);
  }
  
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(statusLedPin, OUTPUT);
  pinMode(readyLedPin, OUTPUT);
  pinMode(connectionLedPin, OUTPUT);

  for (int i = 0; i < numButtons; i++) {
    digitalWrite(buttons[i].ledPin, LOW);
  }
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

  radio.setPALevel(RF24_PA_LOW);
  radio.openReadingPipe(0, displayAddress);
  radio.startListening();
  
  digitalWrite(connectionLedPin, HIGH);
  digitalWrite(statusLedPin, HIGH);
  
  broadcastSystemReady();
  systemReady = true;
  
  Serial.println("Display Unit Ready! Press reset to start game.");
}

void handleWinner(const Button& winner) {
    digitalWrite(winner.ledPin, HIGH);
    winnerChosen = true;
    gameEndTime = millis();
    sendMessage(winner.address, winner.winMessage);
    unsigned long gameTime = gameEndTime - gameStartTime;
    Serial.print(winner.displayName);
    Serial.print(" ПЕРЕМІГ! Час гри: ");
    Serial.print(gameTime);
    Serial.println(" мс");
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

    bool knownMessage = false;
    for (int i = 0; i < numButtons; i++) {
      if (strcmp(incoming, buttons[i].name) == 0) {
        handleWinner(buttons[i]);
        knownMessage = true;
        break;
      }
    }

    if (!knownMessage) {
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
  
  for (int i = 0; i < numButtons; i++) {
    digitalWrite(buttons[i].ledPin, LOW);
  }
  digitalWrite(statusLedPin, LOW);
  digitalWrite(readyLedPin, HIGH);
  
  // Send reset signal to all buttons
  for (int i = 0; i < numButtons; i++) {
    sendMessage(buttons[i].address, MSG_GAME_RESET);
  }
  
  Serial.println("=== НОВА ГРА ПОЧАЛАСЯ! ===");
  Serial.println("Натисніть червону, синю або зелену кнопку!");
  
  delay(100);
  digitalWrite(statusLedPin, HIGH);
}

void broadcastSystemReady() {
  // Send SYSTEM_READY to all buttons individually
  for (int i = 0; i < numButtons; i++) {
    sendMessage(buttons[i].address, MSG_SYSTEM_READY);
    delay(50);
  }
  Serial.println("SYSTEM_READY signals sent to all buttons");
}

void sendMessage(const byte* address, const char* message) {
  radio.stopListening();
  radio.openWritingPipe(address);
  strcpy(outgoing, message);

  digitalWrite(connectionLedPin, LOW);
  delay(10);

  bool result = radio.write(&outgoing, sizeof(outgoing));

  digitalWrite(connectionLedPin, HIGH);

  if (result) {
    Serial.print("Message sent to a button: ");
    Serial.println(message);
  } else {
    Serial.print("Failed to send message to a button: ");
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
