#include <Arduino.h>
#include <stdlib.h>

const int MAX_CAPACITY = 5;
const unsigned long DEBOUNCE_MS = 30;

struct Occupancy {
  int inside;
  int capacity;
};

struct Button {
  int pin;
  int lastReading;
  int stableState;
  unsigned long changedAt;
};

Occupancy *occupancy = NULL;
Button entryButton = {2, HIGH, HIGH, 0};
Button exitButton = {3, HIGH, HIGH, 0};

// Report one press when the input settles low. A release rearms the button.
bool buttonPressed(Button *button, unsigned long now) {
  int reading = digitalRead(button->pin);

  if (reading != button->lastReading) {
    button->lastReading = reading;
    button->changedAt = now;
  }

  if (reading != button->stableState && now - button->changedAt >= DEBOUNCE_MS) {
    button->stableState = reading;
    return reading == LOW;
  }

  return false;
}

void logEvent(const char *message, const Occupancy *record) {
  Serial.print(message);
  Serial.print(" | Inside: ");
  Serial.print(record->inside);
  Serial.print(" | Remaining: ");
  Serial.println(record->capacity - record->inside);
}

void handleEntry(Occupancy *record) {
  if (record->inside == record->capacity) {
    logEvent("Entry rejected: facility is full", record);
    return;
  }

  record->inside++;
  logEvent("Entry accepted", record);
}

void handleExit(Occupancy *record) {
  if (record->inside == 0) {
    logEvent("Exit rejected: facility is empty", record);
    return;
  }

  record->inside--;
  logEvent("Exit accepted", record);
}

void setup() {
  Serial.begin(9600);
  pinMode(entryButton.pin, INPUT_PULLUP);
  pinMode(exitButton.pin, INPUT_PULLUP);

  occupancy = (Occupancy *)malloc(sizeof(Occupancy));
  if (occupancy == NULL) {
    Serial.println("Error: could not allocate occupancy record");
    return;
  }

  occupancy->inside = 0;
  occupancy->capacity = MAX_CAPACITY;
  logEvent("System ready", occupancy);
}

void loop() {
  if (occupancy == NULL) {
    return;
  }

  unsigned long now = millis();
  if (buttonPressed(&exitButton, now)) {
    handleExit(occupancy);
  }
  if (buttonPressed(&entryButton, now)) {
    handleEntry(occupancy);
  }
}
