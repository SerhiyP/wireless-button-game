// =================================================================================
// ==CONFIGURATION==
// Un-comment the block for the button you want to upload this sketch to.
// =================================================================================

// --- RED BUTTON ---
// #define BUTTON_COLOR "RED"
// #define BUTTON_ADDRESS "00002"
// #define WIN_CONFIRMATION "WIN_RED"

// --- BLUE BUTTON ---
// #define BUTTON_COLOR "BLUE"
// #define BUTTON_ADDRESS "00003"
// #define WIN_CONFIRMATION "WIN_BLUE"

// --- GREEN BUTTON ---
#define BUTTON_COLOR "GREEN"
#define BUTTON_ADDRESS "00004"
#define WIN_CONFIRMATION "WIN_GREEN"

// =================================================================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#ifndef BUTTON_COLOR
#error "Button configuration is not defined. Please un-comment one of the button blocks above."
#endif

RF24 radio(9, 10);

const byte displayAddress[6] = "00001";
const byte deviceAddress[6] = BUTTON_ADDRESS;

const int buttonPin = 2;
const int winnerLedPin = 4;
const int readyLedPin = 3;
const int connectionLedPin = 5;

bool sent = false;
bool buttonPressed = false;
bool systemReady = false;
bool winner = false;
char incoming[16];
char outgoing[16];
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long lastTransmissionTime = 0;
unsigned long transmissionTimeout = 5000;
unsigned long systemReadyTimeout = 10000;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(winnerLedPin, OUTPUT);
  pinMode(readyLedPin, OUTPUT);
  pinMode(connectionLedPin, OUTPUT);
  
  digitalWrite(winnerLedPin, LOW);
  digitalWrite(readyLedPin, LOW);
  digitalWrite(connectionLedPin, LOW);
  
  Serial.begin(9600);
  Serial.print(BUTTON_COLOR);
  Serial.println(" Button Transmitter Starting...");
  
  digitalWrite(connectionLedPin, HIGH);
  delay(100);
  digitalWrite(connectionLedPin, LOW);
  delay(100);
  
  if (!radio.begin()) {
    Serial.println("Radio initialization failed!");
    while(1) {
      digitalWrite(winnerLedPin, HIGH);
      digitalWrite(connectionLedPin, HIGH);
      delay(100);
      digitalWrite(winnerLedPin, LOW);
      digitalWrite(connectionLedPin, LOW);
      delay(100);
    }
  }
  
  radio.setPALevel(RF24_PA_HIGH);
  radio.openWritingPipe(displayAddress);
  radio.openReadingPipe(0, deviceAddress);
  radio.startListening();
  
  digitalWrite(connectionLedPin, HIGH);
  
  waitForSystemReady();
  
  Serial.print(BUTTON_COLOR);
  Serial.println(" Button Ready!");
}

void loop() {
  checkForMessages();
  
  if (systemReady && !winner) {
    int reading = digitalRead(buttonPin);
    
    if (reading != buttonPressed) {
      lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading == LOW && !sent) {
        sendButtonPress();
      }
    }
    
    buttonPressed = reading;
    
    if (sent && (millis() - lastTransmissionTime > transmissionTimeout)) {
      Serial.println("Resetting after timeout...");
      sent = false;
    }
  }
  
  updateLEDs();
}

void waitForSystemReady() {
  Serial.println("Waiting for SYSTEM_READY signal...");
  unsigned long startTime = millis();
  
  while (!systemReady && (millis() - startTime < systemReadyTimeout)) {
    if (radio.available()) {
      memset(incoming, 0, sizeof(incoming));
      radio.read(&incoming, sizeof(incoming));
      
      digitalWrite(connectionLedPin, LOW);
      delay(10);
      digitalWrite(connectionLedPin, HIGH);
      
      if (strcmp(incoming, "SYSTEM_READY") == 0) {
        systemReady = true;
        digitalWrite(readyLedPin, HIGH);
        Serial.println("SYSTEM_READY received! Button is now active.");
        break;
      }
    }
    
    if ((millis() / 500) % 2) {
      digitalWrite(readyLedPin, HIGH);
    } else {
      digitalWrite(readyLedPin, LOW);
    }
    
    delay(10);
  }
  
  if (!systemReady) {
    Serial.println("Timeout waiting for SYSTEM_READY - continuing anyway");
    systemReady = true;
    digitalWrite(readyLedPin, HIGH);
  }
}

void checkForMessages() {
  if (radio.available()) {
    memset(incoming, 0, sizeof(incoming));
    radio.read(&incoming, sizeof(incoming));
    
    digitalWrite(connectionLedPin, LOW);
    delay(10);
    digitalWrite(connectionLedPin, HIGH);
    
    if (strcmp(incoming, WIN_CONFIRMATION) == 0) {
      winner = true;
      Serial.print(WIN_CONFIRMATION);
      Serial.println(" received! This button won!");
    } else if (strcmp(incoming, "GAME_RESET") == 0) {
      resetForNewGame();
      Serial.println("GAME_RESET received! Ready for new game.");
    } else if (strcmp(incoming, "SYSTEM_READY") == 0) {
      if (!systemReady) {
        systemReady = true;
        digitalWrite(readyLedPin, HIGH);
        Serial.println("SYSTEM_READY received! Button is now active.");
      }
    }
  }
}

void resetForNewGame() {
  winner = false;
  sent = false;
  digitalWrite(winnerLedPin, LOW);
  digitalWrite(readyLedPin, HIGH);
}

void sendButtonPress() {
  radio.stopListening();
  strcpy(outgoing, BUTTON_COLOR);
  
  Serial.print("Sending ");
  Serial.print(BUTTON_COLOR);
  Serial.println(" signal...");
  
  digitalWrite(connectionLedPin, LOW);
  delay(10);
  
  bool result = radio.write(&outgoing, sizeof(outgoing));
  
  digitalWrite(connectionLedPin, HIGH);
  
  if (result) {
    Serial.print(BUTTON_COLOR);
    Serial.println(" signal sent successfully!");
    sent = true;
    lastTransmissionTime = millis();
  } else {
    Serial.print("Failed to send ");
    Serial.print(BUTTON_COLOR);
    Serial.println(" signal!");
  }
  
  radio.startListening();
}

void updateLEDs() {
  if (winner) {
    digitalWrite(winnerLedPin, HIGH);
    if ((millis() / 300) % 2) {
      digitalWrite(readyLedPin, HIGH);
    } else {
      digitalWrite(readyLedPin, LOW);
    }
  } else if (systemReady) {
    digitalWrite(readyLedPin, HIGH);
    digitalWrite(winnerLedPin, LOW);
  } else {
    if ((millis() / 500) % 2) {
      digitalWrite(readyLedPin, HIGH);
    } else {
      digitalWrite(readyLedPin, LOW);
    }
    digitalWrite(winnerLedPin, LOW);
  }
}
