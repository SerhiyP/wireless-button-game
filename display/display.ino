#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(9, 10);

const byte displayAddress[6] = "00001";  // Display receives on this address

// Number of players supported. Player numbers run 0 .. (NUM_PLAYERS - 1);
// button addresses run 00002 .. (00002 + NUM_PLAYERS - 1).
const int NUM_PLAYERS = 10;

// Protocol Messages
const char* MSG_GAME_RESET = "GAME_RESET";
const char* MSG_SYSTEM_READY = "SYSTEM_READY";

// =================================================================================
// Single 7-segment digit via 1x 74HC595. writeDigits() still takes two bytes for
// compatibility with showNumber()/showError(), but only the `right` byte (shifted
// in last) actually reaches the one physical register — see writeDigits() below.
// =================================================================================
const int dataPin = 4;   // DS
const int latchPin = 2;   // ST_CP
const int clockPin = 5;   // SH_CP

// Segment table A-G (no DP)
const byte numberTable[10] = {
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111  // 9
};

const byte SEG_BLANK = 0x00;  // all segments off
const byte SEG_DASH  = 0x40;  // segment G only
const byte SEG_E     = 0x79;  // letter E

// Pin Definitions
const int resetPin = 6;

bool winnerChosen = false;
char incoming[16];
char outgoing[16];
unsigned long lastResetTime = 0;
unsigned long resetDebounceDelay = 300;
unsigned long gameStartTime = 0;
unsigned long gameEndTime = 0;
bool gameActive = false;
bool systemReady = false;
bool ignoredAvailableLogged = false;

// --- Display helpers ----------------------------------------------------------
// Writes the two digit segment patterns. Only one 74HC595 is physically wired,
// so only the last byte shifted in (right) actually ends up latched and shown;
// `left` is sent first so it gets pushed out and discarded. showNumber()/showError()
// rely on this: the meaningful digit must always be passed as `right`.
void writeDigits(byte left, byte right) {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, left);
  shiftOut(dataPin, clockPin, MSBFIRST, right);
  digitalWrite(latchPin, HIGH);
}

void showBlank() {
  writeDigits(SEG_BLANK, SEG_BLANK);
}

void showDashes() {
  writeDigits(SEG_DASH, SEG_DASH);
}

void showNumber(int n) {
  writeDigits(numberTable[(n / 10) % 10], numberTable[n % 10]);
}

void showError(int code) {
  writeDigits(SEG_E, numberTable[code % 10]);
}

void runDisplayTest() {
  Serial.println("Running display self-test...");
  for (int d = 0; d <= 9; d++) {
    writeDigits(numberTable[d], numberTable[d]);
    delay(150);
  }
  showDashes();
  delay(150);
  showBlank();
}

// --- Identity derivation from player number -----------------------------------
void addrFor(int n, char* out) {
  snprintf(out, 6, "%05d", n + 2);  // player 0 -> "00002"
}

void winMsgFor(int n, char* out) {
  snprintf(out, 16, "WIN_%d", n);   // player 1 -> "WIN_1"
}

void setup() {
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(resetPin, INPUT_PULLUP);

  showBlank();

  Serial.begin(9600);
  Serial.println("Display Unit Starting...");

  runDisplayTest();

  if (!radio.begin()) {
    Serial.println("Radio initialization failed!");
    while (1) {
      showError(1);
      delay(300);
      showBlank();
      delay(300);
    }
  }

  radio.setPALevel(RF24_PA_LOW);
  radio.setRetries(5, 15);  // 1500us delay, 15 retries for reliable communication
  radio.openReadingPipe(0, displayAddress);
  radio.startListening();

  Serial.print("Radio initialized, listening on ");
  Serial.print((const char*)displayAddress);
  Serial.print(" for ");
  Serial.print(NUM_PLAYERS);
  Serial.println(" players");

  broadcastSystemReady();
  systemReady = true;

  showDashes();
  Serial.println("Display Unit Ready! Press reset to start game.");
}

void handleWinner(int n) {
  winnerChosen = true;
  gameEndTime = millis();
  showNumber(n);

  // Small delay to allow the button to switch back to listening mode
  // after it sent its button press message
  delay(500);

  char addr[6];
  char winMsg[16];
  addrFor(n, addr);
  winMsgFor(n, winMsg);
  sendMessage((const byte*)addr, winMsg);

  unsigned long gameTime = gameEndTime - gameStartTime;
  Serial.print("Player ");
  Serial.print(n);
  Serial.print(" WINS! Game time: ");
  Serial.print(gameTime);
  Serial.println(" ms");
}

void loop() {
  if (radio.available() && (winnerChosen || !gameActive) && !ignoredAvailableLogged) {
    Serial.print("Radio data available but ignored (winnerChosen=");
    Serial.print(winnerChosen);
    Serial.print(", gameActive=");
    Serial.print(gameActive);
    Serial.println(") - left in RX buffer");
    ignoredAvailableLogged = true;
  }

  if (radio.available() && !winnerChosen && gameActive) {
    memset(incoming, 0, sizeof(incoming));
    radio.read(&incoming, sizeof(incoming));
    ignoredAvailableLogged = false;

    Serial.print("Received: ");
    Serial.println(incoming);

    int n = atoi(incoming);
    if (n >= 0 && n < NUM_PLAYERS) {
      handleWinner(n);
    } else {
      Serial.print("Invalid/unknown message: ");
      Serial.println(incoming);
      showError(2);
      delay(500);
      showDashes();
    }
  }

  if (digitalRead(resetPin) == LOW && (millis() - lastResetTime > resetDebounceDelay)) {
    resetGame();
    lastResetTime = millis();
  }
}

void resetGame() {
  // Clear any stale messages from the radio buffer
  radio.flush_rx();

  winnerChosen = false;
  gameActive = true;
  gameStartTime = millis();

  showDashes();

  // Send reset signal to all buttons
  for (int n = 0; n < NUM_PLAYERS; n++) {
    char addr[6];
    addrFor(n, addr);
    sendMessage((const byte*)addr, MSG_GAME_RESET);
  }

  Serial.println("=== NEW GAME STARTED! ===");
  Serial.println("Press any button!");
}

void broadcastSystemReady() {
  // Send SYSTEM_READY to all buttons individually
  for (int n = 0; n < NUM_PLAYERS; n++) {
    char addr[6];
    addrFor(n, addr);
    sendMessage((const byte*)addr, MSG_SYSTEM_READY);
    delay(50);
  }
  Serial.println("SYSTEM_READY signals sent to all buttons");
}

void sendMessage(const byte* address, const char* message) {
  radio.stopListening();
  radio.openWritingPipe(address);
  strcpy(outgoing, message);

  bool result = radio.write(&outgoing, sizeof(outgoing));

  if (result) {
    Serial.print("Message sent to ");
    Serial.print((const char*)address);
    Serial.print(": ");
    Serial.println(message);
  } else {
    Serial.print("Failed to send message to ");
    Serial.print((const char*)address);
    Serial.print(": ");
    Serial.println(message);
  }

  radio.startListening();
  delay(10);  // Allow radio to reset before next transmission
}
