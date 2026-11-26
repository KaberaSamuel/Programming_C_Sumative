#include <LiquidCrystal.h>
#include <stdio.h>
#include <stdlib.h>

const int MAX_CAPACITY = 5;
const int GREEN_LED_PIN = 8;
const int RED_LED_PIN = 9;
const unsigned long DEBOUNCE_MS = 30;
const unsigned long LCD_REFRESH_MS = 250;

struct Occupancy
{
  int inside;
  int capacity;
};

struct Button
{
  int pin;
  int lastReading;
  int stableState;
  unsigned long changedAt;
};

// Tinkercad needs these declarations after the struct definitions.
bool buttonPressed(Button *button, unsigned long now);
void logEvent(const char *message, const Occupancy *record);
void updateStatusLights(const Occupancy *record);
void updateDisplay(const Occupancy *record);
void handleEntry(Occupancy *record);
void handleExit(Occupancy *record);
bool validRecord(const Occupancy *record);
void releaseOccupancy();

Occupancy *occupancy = NULL;
Button entryButton = {2, HIGH, HIGH, 0};
Button exitButton = {3, HIGH, HIGH, 0};
LiquidCrystal lcd(12, 11, 7, 6, 5, 4); // RS, E, D4, D5, D6, D7
unsigned long lastLcdUpdate = 0;

// Report one press when the input settles low. A release rearms the button.
bool buttonPressed(Button *button, unsigned long now)
{
  int reading = digitalRead(button->pin);

  if (reading != button->lastReading)
  {
    button->lastReading = reading;
    button->changedAt = now;
  }

  if (reading != button->stableState && now - button->changedAt >= DEBOUNCE_MS)
  {
    button->stableState = reading;
    return reading == LOW;
  }

  return false;
}

void logEvent(const char *message, const Occupancy *record)
{
  Serial.print(message);
  Serial.print(" | Inside: ");
  Serial.print(record->inside);
  Serial.print(" | Remaining: ");
  Serial.println(record->capacity - record->inside);
}

void updateStatusLights(const Occupancy *record)
{
  bool full = record->inside == record->capacity;
  digitalWrite(GREEN_LED_PIN, full ? LOW : HIGH);
  digitalWrite(RED_LED_PIN, full ? HIGH : LOW);
}

void updateDisplay(const Occupancy *record)
{
  // Pad each row to 16 characters so old digits are overwritten.
  char line[17];
  snprintf(line, sizeof(line), "Inside: %-8d", record->inside);
  lcd.setCursor(0, 0);
  lcd.print(line);
  snprintf(line, sizeof(line), "Remaining: %-5d", record->capacity - record->inside);
  lcd.setCursor(0, 1);
  lcd.print(line);
}

void handleEntry(Occupancy *record)
{
  if (record->inside == record->capacity)
  {
    logEvent("Entry rejected: facility is full", record);
    return;
  }

  record->inside++;
  updateStatusLights(record);
  logEvent("Entry accepted", record);
}

void handleExit(Occupancy *record)
{
  if (record->inside == 0)
  {
    logEvent("Exit rejected: facility is empty", record);
    return;
  }

  record->inside--;
  updateStatusLights(record);
  logEvent("Exit accepted", record);
}

// Check that the count is still within the room limits.
bool validRecord(const Occupancy *record)
{
  return record->inside >= 0 && record->inside <= record->capacity;
}

// Free a damaged record and stop counting. The red LED signals the fault.
void releaseOccupancy()
{
  free(occupancy);
  occupancy = NULL;
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, HIGH);
  lcd.clear();
  lcd.print("System error");
  Serial.println("Error: invalid occupancy record, memory released");
}

void setup()
{
  Serial.begin(9600);
  pinMode(entryButton.pin, INPUT_PULLUP);
  pinMode(exitButton.pin, INPUT_PULLUP);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  occupancy = (Occupancy *)malloc(sizeof(Occupancy));
  if (occupancy == NULL)
  {
    Serial.println("Error: could not allocate occupancy record");
    return;
  }

  occupancy->inside = 0;
  occupancy->capacity = MAX_CAPACITY;
  lcd.begin(16, 2);
  updateDisplay(occupancy);
  lastLcdUpdate = millis();
  updateStatusLights(occupancy);
  logEvent("System ready", occupancy);
}

void loop()
{
  if (occupancy == NULL)
  {
    return;
  }

  if (!validRecord(occupancy))
  {
    releaseOccupancy();
    return;
  }

  unsigned long now = millis();
  if (buttonPressed(&exitButton, now))
  {
    handleExit(occupancy);
  }
  if (buttonPressed(&entryButton, now))
  {
    handleEntry(occupancy);
  }
  if (now - lastLcdUpdate >= LCD_REFRESH_MS)
  {
    lastLcdUpdate = now;
    updateDisplay(occupancy);
  }
}
