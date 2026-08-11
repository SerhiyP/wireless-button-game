// =================================================================================
// ==CONFIGURATION==
// Set BUTTON_ID to this button's player number (0..9) before uploading.
// Everything else (radio address, sent value, win message) is derived from it.
// =================================================================================

#define BUTTON_ID 1

// =================================================================================

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#ifndef BUTTON_ID
#error "Button configuration is not defined. Please set BUTTON_ID to a number 0..9."
#endif

#define STR_(x) #x
#define STR(x) STR_(x)
#define BUTTON_NUMBER STR(BUTTON_ID)            // "0" .. "9"   (value sent to display)
#define WIN_CONFIRMATION "WIN_" STR(BUTTON_ID)  // "WIN_0" .. "WIN_9"
#define PING_MESSAGE "PING_" STR(BUTTON_ID)     // "PING_0" .. "PING_9"

RF24 radio(9, 10);

const byte displayAddress[6] = "00001";
byte deviceAddress[6];  // derived from BUTTON_ID in setup(): "00002" .. "00011"

const int buttonPin = 2;
const int winnerLedPin = 4;
const int readyLedPin = 3;
const int connectionLedPin = 5;

bool sent = false;
bool systemReady = false;
bool winner = false;
char incoming[16];
char outgoing[16];
volatile bool pressLatched = false;
volatile unsigned long pressEdgeTime = 0;
const unsigned long glitchConfirmDelay = 2;  // ms the pin must still be LOW after the edge
unsigned long lastPingTime = 0;
unsigned long pingInterval = 1000;

// INT0 ISR (button is on pin 2): latch the press the moment the pin falls, even
// while loop() is stuck in a radio call or a serial write. Confirmation, sending
// and clearing all happen in loop(). Only the first edge is recorded - the later
// edges of the same press are contact bounce. pressEdgeTime is written before
// pressLatched so loop() can never see the flag without a valid timestamp; both
// are volatile, which keeps that order.
void onButtonFall() {
  if (!pressLatched) {
    pressEdgeTime = millis();
    pressLatched = true;
  }
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), onButtonFall, FALLING);
  pinMode(winnerLedPin, OUTPUT);
  pinMode(readyLedPin, OUTPUT);
  pinMode(connectionLedPin, OUTPUT);

  digitalWrite(winnerLedPin, LOW);
  digitalWrite(readyLedPin, LOW);
  digitalWrite(connectionLedPin, LOW);

  Serial.begin(9600);

  // Derive this button's radio address from its ID: player 0 -> "00002", etc.
  snprintf((char*)deviceAddress, sizeof(deviceAddress), "%05d", BUTTON_ID + 2);

  Serial.print("Button ");
  Serial.print(BUTTON_NUMBER);
  Serial.println(" Transmitter Starting...");
  Serial.print("Device address: ");
  Serial.println((char*)deviceAddress);
  Serial.print("Win confirmation message: ");
  Serial.println(WIN_CONFIRMATION);

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

  radio.setPALevel(RF24_PA_LOW);
  radio.setRetries(5, 15);  // 1500µs delay, 15 retries for reliable communication
  radio.openWritingPipe(displayAddress);
  radio.openReadingPipe(0, deviceAddress);
  radio.startListening();

  Serial.println("Radio initialized, pinging display until it answers...");

  digitalWrite(connectionLedPin, HIGH);
}

void loop() {
  checkForMessages();

  if (!systemReady && (lastPingTime == 0 || millis() - lastPingTime >= pingInterval)) {
    pingDisplay();
  }

  if (pressLatched) {
    handleLatchedPress();
  }

  updateLEDs();
}

// A press is latched by the INT0 ISR on the falling edge and confirmed here: the
// pin must still be LOW glitchConfirmDelay ms after the edge. A real press holds
// the pin low for tens of milliseconds; a noise spike lasts microseconds and fails
// the re-read. No further debounce is needed - the `sent` latch already limits the
// round to one transmission, so bounce cannot double-send.
void handleLatchedPress() {
  if (systemReady && !winner && !sent) {
    if (millis() - pressEdgeTime < glitchConfirmDelay) {
      return;  // too early to tell press from glitch; check again next loop
    }
    if (digitalRead(buttonPin) == LOW) {
      sendButtonPress();
    } else {
      Serial.println("Discarded noise glitch on button pin");
    }
  } else {
    Serial.print("Button press ignored (systemReady=");
    Serial.print(systemReady);
    Serial.print(", winner=");
    Serial.print(winner);
    Serial.print(", sent=");
    Serial.print(sent);
    Serial.println(")");
  }
  pressLatched = false;
}

// Actively probe the display instead of waiting to be told the system is up: the
// display cannot know when a button powers on (or gets reset by opening the Serial
// Monitor), but a button can always ask.
//
// A successful write only means the display's radio auto-ACKed in hardware, which
// says nothing about whether its firmware is running. So readiness waits for the
// SYSTEM_READY reply instead - only the display's main loop can send that, and only
// between rounds. checkForMessages() picks it up and calls markReady().
void pingDisplay() {
  lastPingTime = millis();

  radio.stopListening();
  radio.openWritingPipe(displayAddress);  // re-assert TX address so the ACK returns on pipe 0
  strcpy(outgoing, PING_MESSAGE);

  bool result = radio.write(&outgoing, sizeof(outgoing));

  radio.openReadingPipe(0, deviceAddress);  // restore reading address before listening again
  radio.startListening();

  if (result) {
    Serial.println("Ping delivered, waiting for SYSTEM_READY...");
  } else {
    Serial.println("No answer from display, retrying...");
  }
}

void markReady() {
  systemReady = true;
  digitalWrite(readyLedPin, HIGH);
  Serial.print("Button ");
  Serial.print(BUTTON_NUMBER);
  Serial.println(" Ready!");
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
        Serial.println("SYSTEM_READY received!");
        markReady();
      }
    } else {
      Serial.print("Unknown message received: ");
      Serial.println(incoming);
    }
  }
}

void resetForNewGame() {
  winner = false;
  sent = false;
  pressLatched = false;  // a press latched while locked out must not fire into the new round
  digitalWrite(winnerLedPin, LOW);
  digitalWrite(readyLedPin, HIGH);

  // GAME_RESET is broadcast from the display's main loop to every button as a round
  // starts, so it also adopts a button that had not connected yet. Without this a
  // button powered up during a round could never join: pings are deferred while a
  // round is live, and the round only ends when someone presses.
  if (!systemReady) {
    markReady();
  }
}

void sendButtonPress() {
  radio.stopListening();
  radio.openWritingPipe(displayAddress);  // re-assert TX address so the ACK returns on pipe 0
  strcpy(outgoing, BUTTON_NUMBER);

  Serial.print("Sending ");
  Serial.print(BUTTON_NUMBER);
  Serial.println(" signal...");

  // Transmit immediately - no LED blink delay before the write, this is a race.
  // The write itself (~1-22ms with retries) keeps the LED visibly off.
  digitalWrite(connectionLedPin, LOW);

  bool result = radio.write(&outgoing, sizeof(outgoing));

  digitalWrite(connectionLedPin, HIGH);

  // Latch on attempt: one press = one transmission, locked until GAME_RESET.
  // The display is the authority on who won; a missed ACK should still lock us out.
  sent = true;

  if (result) {
    Serial.print(BUTTON_NUMBER);
    Serial.println(" signal sent successfully!");
  } else {
    Serial.print("Failed to send ");
    Serial.print(BUTTON_NUMBER);
    Serial.println(" signal!");
  }

  radio.openReadingPipe(0, deviceAddress);  // restore reading address before listening again
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
