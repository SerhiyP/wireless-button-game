# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A wireless button game system for 5 players built on Arduino + nRF24L01 2.4GHz radio. Five button devices (Red, Blue, Green, Yellow, White) race to transmit a press to a central display receiver. The first press wins; the display confirms the winner and controls game reset.

The codebase is two Arduino sketches:
- **`button.ino`** — single configurable firmware uploaded to all five button Arduinos
- **`display.ino`** — firmware for the central receiver/display Arduino

## Building & Uploading

This is a pure Arduino IDE project with no build system. There are no CLI compile/upload commands.

**Prerequisites:**
- Arduino IDE
- RF24 library (install via Library Manager → search "RF24")

**Uploading button.ino:**  
At the top of `button.ino`, exactly one color block must be uncommented before uploading:

```cpp
// Un-comment ONE of these blocks:
// #define BUTTON_COLOR "RED"
// #define BUTTON_ADDRESS "00002"
// #define WIN_CONFIRMATION "WIN_RED"
```

A `#error` directive prevents compilation if none is selected.

**Debugging:** Open Serial Monitor at **9600 baud** (`Ctrl+Shift+M`). All devices emit verbose diagnostic output — this is the primary debugging tool.

## Architecture

### Communication Protocol

All messages are fixed `char[16]` buffers transmitted over nRF24L01.

| Message | Direction | Meaning |
|---|---|---|
| `SYSTEM_READY` | Display → Buttons | Broadcast on startup to each button address |
| `RED` / `BLUE` / `GREEN` / `YELLOW` / `WHITE` | Button → Display | Button press signal |
| `WIN_RED` / `WIN_BLUE` / `WIN_GREEN` / `WIN_YELLOW` / `WIN_WHITE` | Display → Button | Winner confirmation sent to winning button |
| `GAME_RESET` | Display → All Buttons | New game started |

String constants are defined in `display.ino` as `MSG_*` variables (e.g., `MSG_SYSTEM_READY`). `button.ino` uses raw string literals for the subset it needs.

### Radio Addresses

| Device | Address |
|---|---|
| Display | `"00001"` |
| Red Button | `"00002"` |
| Blue Button | `"00003"` |
| Green Button | `"00004"` |
| Yellow Button | `"00005"` |
| White Button | `"00006"` |

Radio settings: `RF24_PA_LOW`, retries `(5, 15)` = 1500µs delay × 15 attempts.

### Adding a New Button Color

All button configuration in `display.ino` is centralized in the `Button` struct array:

```cpp
Button buttons[] = {
  {MSG_RED,    redButtonAddress,    4,  MSG_WIN_RED,    "ЧЕРВОНИЙ"},
  {MSG_BLUE,   blueButtonAddress,   5,  MSG_WIN_BLUE,   "СИНІЙ"},
  {MSG_GREEN,  greenButtonAddress,  2,  MSG_WIN_GREEN,  "ЗЕЛЕНИЙ"},
  {MSG_YELLOW, yellowButtonAddress, A0, MSG_WIN_YELLOW, "ЖОВТИЙ"},
  {MSG_WHITE,  whiteButtonAddress,  A1, MSG_WIN_WHITE,  "БІЛИЙ"}
};
```

Add a new entry here (name, address, LED pin, win message, display name), define the address constant, add the corresponding `MSG_WIN_*` constant, then add the matching `#define` block option to `button.ino`.

**Note on analog pins:** A0 and A1 are analog pins used as digital outputs for Yellow and White winner LEDs. The `setup()` loop initializes them automatically via `buttons[]`. Next available addresses would start at `"00007"`; next available LED pins would be A2, A3, etc.

### Pin Assignments

**button.ino:**
- Pin 2: Button (INPUT_PULLUP, active-LOW)
- Pin 3: Ready LED
- Pin 4: Winner LED
- Pin 5: Connection LED
- Pins 9, 10: nRF24 CE, CSN (SPI)

**display.ino:**
- Pin 2: Green Winner LED
- Pin 3: Ready LED
- Pin 4: Red Winner LED
- Pin 5: Blue Winner LED
- Pin 6: Reset Button (INPUT_PULLUP, active-LOW)
- Pin 7: Status LED
- Pin 8: Connection LED
- Pins 9, 10: nRF24 CE, CSN (SPI)
- A0: Yellow Winner LED (analog pin used as digital output)
- A1: White Winner LED (analog pin used as digital output)

**Critical hardware note:** nRF24L01 modules require a **3.3V** supply, not 5V.