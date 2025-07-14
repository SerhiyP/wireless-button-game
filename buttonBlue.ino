#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);

const byte address[6] = "00001";
const int buttonPin = 2;
const int winnerLedPin = 4;
const int readyLedPin = 3;
const int connectionLedPin = 5;

bool sent = false;
bool buttonPressed = false;
bool systemReady = false;
bool winner = false;
char incoming[32];
char outgoing[32];
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long lastTransmissionTime = 0;
unsigned long transmissionTimeout = 5000;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(winnerLedPin, OUTPUT);
  pinMode(readyLedPin, OUTPUT);
  pinMode(connectionLedPin, OUTPUT);
  
  digitalWrite(winnerLedPin, LOW);
  digitalWrite(readyLedPin, LOW);
  digitalWrite(connectionLedPin, LOW);
  
  Serial.begin(9600);
  Serial.println("Blue Button Transmitter Starting...");
  
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
  radio.openWritingPipe(address);
  radio.openReadingPipe(0, address);
  radio.startListening();
  
  digitalWrite(connectionLedPin, HIGH);
  
  waitForSystemReady();
  
  Serial.println("Blue Button Ready!");
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
  
  while (!systemReady) {
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
      }
    }
    
    if ((millis() / 500) % 2) {
      digitalWrite(readyLedPin, HIGH);
    } else {
      digitalWrite(readyLedPin, LOW);
    }
    
    delay(10);
  }
}

void checkForMessages() {
  if (radio.available()) {
    memset(incoming, 0, sizeof(incoming));
    radio.read(&incoming, sizeof(incoming));
    
    digitalWrite(connectionLedPin, LOW);
    delay(10);
    digitalWrite(connectionLedPin, HIGH);
    
    if (strcmp(incoming, "WIN_BLUE") == 0) {
      winner = true;
      Serial.println("WIN_BLUE received! This button won!");
    } else if (strcmp(incoming, "SYSTEM_READY") == 0) {
      if (!systemReady) {
        systemReady = true;
        digitalWrite(readyLedPin, HIGH);
        Serial.println("SYSTEM_READY received! Button is now active.");
      }
    }
  }
}

void sendButtonPress() {
  radio.stopListening();
  strcpy(outgoing, "BLUE");
  
  Serial.println("Sending BLUE signal...");
  
  digitalWrite(connectionLedPin, LOW);
  delay(10);
  
  bool result = radio.write(&outgoing, sizeof(outgoing));
  
  digitalWrite(connectionLedPin, HIGH);
  
  if (result) {
    Serial.println("BLUE signal sent successfully!");
    sent = true;
    lastTransmissionTime = millis();
  } else {
    Serial.println("Failed to send BLUE signal!");
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
