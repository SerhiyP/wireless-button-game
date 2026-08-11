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
const char* MSG_PING_PREFIX = "PING_";  // buttons send "PING_<n>" to discover the display

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
bool gameActive = false;
bool systemReady = false;

// True once player n has been heard from (ping or press). Only these slots get
// GAME_RESET retries - an empty slot fails every send and would stretch the
// broadcast's deaf window for nothing.
bool connected[NUM_PLAYERS] = {false};
int lastWinner = -1;

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

// Parse a player number the strict way. atoi() cannot signal failure - it returns 0
// for anything unparseable, and 0 is a valid player number since numbering became
// 0-based, so a corrupted or foreign packet would be read as a press by player 0.
// Rejecting empty strings, non-digits and trailing junk keeps that from happening;
// bailing out as soon as the value reaches NUM_PLAYERS also avoids overflowing the
// 16-bit int on a long run of digits.
bool parsePlayer(const char* s, int* out) {
  if (*s == '\0') {
    return false;
  }

  // Buttons send BUTTON_ID with no padding, so "07" is not something our own
  // protocol can produce - only "0" itself may start with a zero.
  if (s[0] == '0' && s[1] != '\0') {
    return false;
  }

  int n = 0;
  for (const char* p = s; *p; p++) {
    if (*p < '0' || *p > '9') {
      return false;
    }
    n = n * 10 + (*p - '0');
    if (n >= NUM_PLAYERS) {
      return false;
    }
  }

  *out = n;
  return true;
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

  // No startup broadcast: buttons discover us by pinging, which works no matter
  // which device is powered up first.
  systemReady = true;

  showDashes();
  Serial.println("Display Unit Ready! Press reset to start game.");
}

void handleWinner(int n) {
  winnerChosen = true;
  lastWinner = n;
  connected[n] = true;
  showNumber(n);

  // Small delay to allow the button to switch back to listening mode
  // after it sent its button press message
  delay(500);

  char addr[6];
  char winMsg[16];
  addrFor(n, addr);
  winMsgFor(n, winMsg);
  sendMessage((const byte*)addr, winMsg);

  Serial.print("Player ");
  Serial.print(n);
  Serial.println(" WINS!");
}

void loop() {
  // Always drain the RX FIFO, even with no game running: it only holds three
  // payloads, and buttons ping while idle, so leaving messages unread would
  // fill it and block everything that follows.
  if (radio.available()) {
    memset(incoming, 0, sizeof(incoming));
    radio.read(&incoming, sizeof(incoming));
    handleMessage(incoming);
  }

  if (digitalRead(resetPin) == LOW && (millis() - lastResetTime > resetDebounceDelay)) {
    resetGame();
    lastResetTime = millis();
  }
}

// A round is live only between the reset press that starts it and the press that
// wins it. gameActive alone is not enough: it stays true once the first round has
// been started and is never cleared.
bool roundInProgress() {
  return gameActive && !winnerChosen;
}

// Buttons ping until the display answers. Adopt them only between rounds: the
// reply puts the display into TX, and a press landing in that window would fail
// its ACK and be locked out for the round by the button's one-shot latch. A late
// button keeps pinging and joins as soon as the round ends.
void handlePing(int n) {
  connected[n] = true;  // remember the button even when the reply is deferred

  if (!systemReady || roundInProgress()) {
    Serial.print("Ping from player ");
    Serial.print(n);
    Serial.println(" - deferred");
    return;
  }

  char addr[6];
  addrFor(n, addr);
  sendMessage((const byte*)addr, MSG_SYSTEM_READY);
}

void handleMessage(const char* msg) {
  size_t pingLen = strlen(MSG_PING_PREFIX);
  int n;

  if (strncmp(msg, MSG_PING_PREFIX, pingLen) == 0) {
    if (parsePlayer(msg + pingLen, &n)) {
      handlePing(n);
    } else {
      Serial.print("Ping from unknown player: ");
      Serial.println(msg);
    }
    return;
  }

  if (!roundInProgress()) {
    Serial.print("Ignored (no round in progress): ");
    Serial.println(msg);
    return;
  }

  if (parsePlayer(msg, &n)) {
    Serial.print("Received: ");
    Serial.println(msg);
    handleWinner(n);
  } else {
    // Decline anything that is not a player number, and do it without touching the
    // display. Showing an error code here used to cost 500 ms with the receiver
    // deaf, so a real press landing in that window failed its ACK and was locked
    // out for the round by the button's one-shot latch. A stray packet must not be
    // able to cost someone their press.
    Serial.print("Declined: ");
    Serial.println(msg);
  }
}

void resetGame() {
  // Clear any stale messages from the radio buffer
  radio.flush_rx();

  winnerChosen = false;
  gameActive = true;

  showDashes();

  // Send reset to all buttons, previous winner first: it is the button with
  // visible stale state (winner LED lit), and a fast new press racing the tail of
  // this broadcast can jam later sends - both sides retry on the same 1500us
  // schedule, so a collision tends to repeat until both give up.
  bool pending[NUM_PLAYERS];
  for (int i = 0; i < NUM_PLAYERS; i++) {
    int n = (lastWinner >= 0) ? (lastWinner + i) % NUM_PLAYERS : i;
    char addr[6];
    addrFor(n, addr);
    pending[n] = !sendMessage((const byte*)addr, MSG_GAME_RESET);
  }

  // A button that misses GAME_RESET keeps last round's state (winner LED lit,
  // press latched out), so failed sends must be retried, not dropped. Only
  // connected slots are retried: an empty slot fails every time anyway.
  for (int pass = 0; pass < 2; pass++) {
    bool anyPending = false;
    for (int n = 0; n < NUM_PLAYERS; n++) {
      if (pending[n] && connected[n]) {
        char addr[6];
        addrFor(n, addr);
        pending[n] = !sendMessage((const byte*)addr, MSG_GAME_RESET);
        anyPending |= pending[n];
      }
    }
    if (!anyPending) {
      break;
    }
  }

  for (int n = 0; n < NUM_PLAYERS; n++) {
    if (pending[n] && connected[n]) {
      Serial.print("WARNING: player ");
      Serial.print(n);
      Serial.println(" did not receive GAME_RESET - stale until next reset");
    }
  }

  Serial.println("=== NEW GAME STARTED! ===");
  Serial.println("Press any button!");
}

bool sendMessage(const byte* address, const char* message) {
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

  return result;
}
