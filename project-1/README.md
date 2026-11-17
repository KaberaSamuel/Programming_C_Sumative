# Project 1: Smart Occupancy Monitoring and Access Control

In this project, I use an Arduino Uno to count how many people are inside a room that holds up to five people. One button is for entry and the other is for exit. The LCD shows how many people are inside and how many more can enter. The green LED means there is space. The red LED means the room is full.

## Wiring

My circuit uses an Arduino Uno, a breadboard, two push buttons, a green LED, a red LED, three 220 Ω resistors, a 16×2 LCD, a 10 kΩ potentiometer, and jumper wires. I connect 5V to the breadboard's power rail and GND to its ground rail.

| Part | Connection |
| --- | --- |
| Entry button | D2 to one switch contact, GND to the opposite contact |
| Exit button | D3 to one switch contact, GND to the opposite contact |
| Green LED | D8 through a 220 Ω resistor to the anode, cathode to GND |
| Red LED | D9 through a 220 Ω resistor to the anode, cathode to GND |
| LCD power | VSS to GND and VDD to 5V |
| LCD contrast | VO to the potentiometer's middle pin, outer pins to 5V and GND |
| LCD control | RS to D12, R/W to GND, and E to D11 |
| LCD data | DB4 to D7, DB5 to D6, DB6 to D5, and DB7 to D4 |
| LCD backlight | Anode (A) to 5V through a 220 Ω resistor, cathode (K) to GND |

I leave LCD pins DB0 to DB3 disconnected and place both buttons across the breadboard's center gap. I use the Uno's built-in pull-up resistors to keep each input HIGH. Pressing a button connects its input to ground and makes it LOW.

## Run in Tinkercad

1. Open the circuit while signed in to the account that owns it. [Saved Tinkercad circuit](https://www.tinkercad.com/things/eUmNP0nMBtU/editel?returnTo=%2Fdashboard)
2. Click **Start Simulation**. The left button is Entry and the right button is Exit.
3. Open **Code** and expand **Serial Monitor** to see the event messages. The sketch uses 9600 baud.

To rebuild the circuit, follow the wiring table. Open **Code**, switch to **Text**, and paste the sketch. Adjust the potentiometer if the LCD text is hard to see. [Arduino sketch](project-1.ino)

## How the sketch works

- I use an `Occupancy` struct to store the count and the limit. In `setup()`, I use `malloc()` to make space for one record. If that fails, the sketch prints an error and stops counting. I pass a pointer to the functions that use the record and keep it until the Uno resets.
- I give each button its own timer to handle bounce. The sketch counts a press after the input stays LOW for 30 ms. It waits for a stable release before counting another press. If both presses register in the same loop, exit goes first.
- I use `millis()` to time the buttons and refresh the LCD. The LEDs update when the count changes. The Serial Monitor shows each attempt, the count inside, and how many spaces are left.
