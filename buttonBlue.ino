#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);

const byte address[6] = "00001";
const int buttonPin = 2;
const int ledPin = 4;

bool sent = false;
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long lastTransmissionTime = 0;
unsigned long transmissionTimeout = 5000;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  
  Serial.begin(9600);
  Serial.println("Blue Button Transmitter Starting...");
  
  if (!radio.begin()) {
    Serial.println("Radio initialization failed!");
    while(1) {
      digitalWrite(ledPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
      delay(100);
    }
  }
  
  radio.setPALevel(RF24_PA_LOW);
  radio.openWritingPipe(address);
  radio.stopListening();
  
  digitalWrite(ledPin, LOW);
  Serial.println("Blue Button Ready!");
}

void loop() {
  int reading = digitalRead(buttonPin);
  
  if (reading != buttonPressed) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && !sent) {
      const char text[] = "BLUE";
      
      Serial.println("Sending BLUE signal...");
      digitalWrite(ledPin, HIGH);
      
      bool result = radio.write(&text, sizeof(text));
      
      if (result) {
        Serial.println("BLUE signal sent successfully!");
        sent = true;
        lastTransmissionTime = millis();
      } else {
        Serial.println("Failed to send BLUE signal!");
        digitalWrite(ledPin, LOW);
      }
    }
  }
  
  buttonPressed = reading;
  
  if (sent && (millis() - lastTransmissionTime > transmissionTimeout)) {
    Serial.println("Resetting after timeout...");
    sent = false;
    digitalWrite(ledPin, LOW);
  }
  
  if (sent) {
    if ((millis() / 500) % 2) {
      digitalWrite(ledPin, HIGH);
    } else {
      digitalWrite(ledPin, LOW);
    }
  }
}
