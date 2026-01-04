# GEMINI.md - Wireless Button Game

## Project Overview

This project is a wireless button game system built using Arduino. It consists of four main components: three transmitter "buttons" (Red, Blue, and Green) and one receiver "display". The system is designed for competitive games where three players race to press their button first.

The core technology used is the nRF24L01 radio module for wireless communication between the Arduino devices. The system implements a robust communication protocol with features like winner confirmation, game state management (ready, active, winner), and automatic reset.

The project is divided into two main Arduino sketches:
-   `button.ino`: A single, configurable firmware for all three button transmitters (Red, Blue, and Green).
-   `display.ino`: The firmware for the central receiver and display unit.

## Building and Running

### Prerequisites

1.  **Arduino IDE:** You need the Arduino IDE installed to compile and upload the sketches.
2.  **RF24 Library:** The `RF24` library is required for nRF24L01 communication. It can be installed via the Arduino IDE's Library Manager (`Sketch` -> `Include Library` -> `Manage Libraries...` -> Search for "RF24").

### Hardware Components

-   4x Arduino boards (e.g., Uno, Nano, Pro Mini)
-   4x nRF24L01 radio modules
-   3x Push-buttons (for the transmitters)
-   1x Push-button (for the display's reset functionality)
-   Various LEDs for status indication (winner, ready, connection)
-   Resistors (220Ω for LEDs)
-   Connecting wires

### Setup and Flashing

1.  **Wiring:** Connect the components for each of the four devices (Red button, Blue button, Green button, Display) according to the schematics provided in `README.md`. **Crucially, ensure the nRF24L01 modules are connected to a 3.3V supply, not 5V.**

2.  **Upload Sketches:**
    *   Open `button.ino` in the Arduino IDE.
    *   **For the Red button:** In `button.ino`, uncomment the configuration block for the "RED BUTTON" and comment out the others. Select your board and port, then upload.
    *   **For the Blue button:** In `button.ino`, uncomment the configuration block for the "BLUE BUTTON" and comment out the others. Select your board and port, then upload.
    *   **For the Green button:** In `button.ino`, uncomment the configuration block for the "GREEN BUTTON" and comment out the others. Select your board and port, then upload.
    *   Open `display.ino`, select your board and port, and upload it to the fourth Arduino.

3.  **Verify:** Open the Serial Monitor (`Ctrl+Shift+M`) for each device with the baud rate set to `9600`. You should see diagnostic messages indicating that the devices are starting up and attempting to communicate.

### How to Play

1.  Power on all four devices.
2.  Press the reset button on the `display` unit. This starts a new game.
3.  The "ready" LEDs on all devices will light up, indicating the game is active.
4.  The first player to press their button wins. The corresponding winner LED will light up on the display and the winning button's device.
5.  Press the reset button on the display to start a new game.

## Development Conventions

-   **Communication Protocol:** The system uses a custom message-based protocol over the nRF24L01 radios. To improve maintainability and scalability, all protocol messages (e.g., "RED", "SYSTEM_READY", "WIN_RED") are now defined as string constants (e.g., `MSG_RED`, `MSG_SYSTEM_READY`) in `display.ino`.
    -   `display` unit broadcasts `MSG_SYSTEM_READY` to signal that the buttons can be activated.
    -   `button` units send `MSG_RED`, `MSG_BLUE`, or `MSG_GREEN` when pressed.
    -   `display` confirms the winner by sending `MSG_WIN_RED`, `MSG_WIN_BLUE`, or `MSG_WIN_GREEN` back to the corresponding button.
    -   `display` sends `MSG_GAME_RESET` to all buttons when a new game is started.
-   **Radio Addresses:** Each device has a unique 5-byte address for communication. While these addresses are still defined as constants (e.g., `redButtonAddress`), their management, along with associated LED pins and other button-specific data, is now centralized in `display.ino` within a `Button` struct and a `buttons` array. This structure significantly enhances scalability, making it easy to add or remove buttons by simply modifying the `buttons` array.
    -   Display: `"00001"`
    -   Red Button: `"00002"`
    -   Blue Button: `"00003"`
    -   Green Button: `"00004"`
-   **Debugging:** The code is heavily instrumented with `Serial.println()` statements. The Serial Monitor is the primary tool for debugging and verifying the state of each device.
-   **LED Indicators:** LEDs provide a visual representation of the game state (Connection status, System Ready, Winner). The blinking patterns are described in the `README.md`.
-   **Debouncing:** A software debouncing mechanism is implemented for the buttons to prevent multiple triggers from a single press.
