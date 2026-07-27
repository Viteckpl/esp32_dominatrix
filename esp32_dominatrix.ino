
#include <TM1637Display.h>

#define TRIG_PIN 4
#define ECHO_PIN 5
#define CLK 18
#define DIO 19
#define BUTTON_PIN 21
#define LED_PIN 22
#define BUZZER_PIN 23

// ===== CONFIG =====
const unsigned long WAIT_MIN_MS = 600000;       // 10 min
const unsigned long WAIT_MAX_MS = 10800000;     // 3 h
const unsigned long CALL_TIMEOUT_MS = 30000;    // 30 s to press the button after the call
const unsigned long PRESENCE_MIN_MS = 10000;    // 10 s
const unsigned long PRESENCE_MAX_MS = 600000;   // 10 min
const unsigned long SLEEP_HOLD_MS = 2000;       // 2 s button hold
const unsigned long PUNISH_BLINK_MS = 400;      // punishment LED blink tempo
const unsigned long CALL_BEEP_INTERVAL_MS = 2000;
const unsigned long CALL_BEEP_DURATION_MS = 200;
const int CALL_BEEP_FREQ = 2000;
const unsigned long SUCCESS_BEEP_DURATION_MS = 200;
const int SUCCESS_BEEP_FREQ = 2500;
const int DISTANCE_THRESHOLD_CM = 60;
const int MAX_MISSES = 5;
const int SLEEP_ON_BLINKS = 3;
const int SLEEP_OFF_BLINKS = 5;
const unsigned long SLEEP_SIGNAL_ON_MS = 180;
const unsigned long SLEEP_SIGNAL_OFF_MS = 180;
// ==================

TM1637Display display(CLK, DIO);

enum State {
  IDLE,
  CALLING,
  PRESENCE,
  PUNISH,
  SLEEP_MODE
};

State state = IDLE;

unsigned long stateStart = 0;
unsigned long nextCallAt = 0;
unsigned long currentWaitDuration = 0;
unsigned long presenceDuration = 0;
unsigned long punishBlinkAt = 0;
unsigned long lastBeepAt = 0;
unsigned long buttonPressStart = 0;
bool ledState = false;
bool buttonWasPressed = false;
int distanceMisses = 0;
float lastGoodDistance = -1.0;

bool buttonPressed() {
  return digitalRead(BUTTON_PIN) == LOW;
}

bool buttonClicked() {
  static bool lastState = HIGH;
  bool current = digitalRead(BUTTON_PIN);
  bool clicked = (lastState == HIGH && current == LOW);
  lastState = current;
  return clicked;
}

void blinkLedTimes(int times, unsigned long onMs, unsigned long offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) {
      delay(offMs);
    }
  }
}

float readDistanceCm() {
  const int samples = 5;
  long vals[samples];
  int count = 0;

  for (int i = 0; i < samples; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration > 0) {
      vals[count++] = duration;
    }
    delay(20);
  }

  if (count == 0) return -1.0;

  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (vals[j] < vals[i]) {
        long tmp = vals[i];
        vals[i] = vals[j];
        vals[j] = tmp;
      }
    }
  }

  long median = vals[count / 2];
  return median * 0.0343 / 2.0;
}

void showSeconds(unsigned long msLeft) {
  int totalSec = (msLeft + 999) / 1000;
  int minutes = totalSec / 60;
  int seconds = totalSec % 60;
  int value = minutes * 100 + seconds;
  display.showNumberDecEx(value, 0b01000000, true);
}

void scheduleNextCall() {
  currentWaitDuration = random(WAIT_MIN_MS, WAIT_MAX_MS + 1);
  nextCallAt = millis() + currentWaitDuration;
  Serial.print("Next wait duration: ");
  Serial.print(currentWaitDuration / 1000);
  Serial.println(" s");
}

void startIdle(bool reschedule) {
  state = IDLE;
  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);
  display.clear();
  distanceMisses = 0;
  lastGoodDistance = -1.0;
  if (reschedule) {
    scheduleNextCall();
  }
}

void startCalling() {
  state = CALLING;
  stateStart = millis();
  lastBeepAt = 0;
}

void startPresence() {
  state = PRESENCE;
  stateStart = millis();
  presenceDuration = random(PRESENCE_MIN_MS, PRESENCE_MAX_MS + 1);
  distanceMisses = 0;
  lastGoodDistance = -1.0;
  tone(BUZZER_PIN, CALL_BEEP_FREQ + 200, 150);
  Serial.print("Presence duration: ");
  Serial.print(presenceDuration / 1000.0);
  Serial.println(" s");
}

void startPunish() {
  state = PUNISH;
  noTone(BUZZER_PIN);
  punishBlinkAt = millis();
  ledState = true;
  digitalWrite(LED_PIN, HIGH);
  display.showNumberDecEx(0, 0b01000000, true);
  Serial.println("PUNISH triggered");
}

void startSleep() {
  blinkLedTimes(SLEEP_ON_BLINKS, SLEEP_SIGNAL_ON_MS, SLEEP_SIGNAL_OFF_MS);
  state = SLEEP_MODE;
  noTone(BUZZER_PIN);
  digitalWrite(LED_PIN, LOW);
  display.clear();
  Serial.println("Sleep mode ON");
}

void wakeFromSleep() {
  blinkLedTimes(SLEEP_OFF_BLINKS, SLEEP_SIGNAL_ON_MS, SLEEP_SIGNAL_OFF_MS);
  Serial.println("Sleep mode OFF");
  startIdle(true);
}

void handleSleepToggle() {
  bool pressed = buttonPressed();

  if (pressed && !buttonWasPressed) {
    buttonPressStart = millis();
  }

  if (!pressed && buttonWasPressed) {
    buttonPressStart = 0;
  }

  if (pressed && buttonPressStart > 0 && millis() - buttonPressStart >= SLEEP_HOLD_MS) {
    while (buttonPressed()) {
      delay(10);
    }
    if (state == SLEEP_MODE) {
      wakeFromSleep();
    } else {
      startSleep();
    }
    buttonPressStart = 0;
  }

  buttonWasPressed = pressed;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  display.setBrightness(7);
  randomSeed(micros() ^ analogRead(34) ^ analogRead(35));
  startIdle(true);
}

void loop() {
  handleSleepToggle();

  if (state == SLEEP_MODE) {
    delay(20);
    return;
  }

  if (state == IDLE) {
    display.clear();
    if (millis() >= nextCallAt) {
      startCalling();
    }
    delay(50);
    return;
  }

  if (state == CALLING) {
    unsigned long elapsed = millis() - stateStart;
    unsigned long remaining = (elapsed < CALL_TIMEOUT_MS) ? (CALL_TIMEOUT_MS - elapsed) : 0;
    showSeconds(remaining);

    if (millis() - lastBeepAt >= CALL_BEEP_INTERVAL_MS) {
      tone(BUZZER_PIN, CALL_BEEP_FREQ, CALL_BEEP_DURATION_MS);
      lastBeepAt = millis();
    }

    if (buttonClicked()) {
      startPresence();
      return;
    }

    if (elapsed >= CALL_TIMEOUT_MS) {
      startPunish();
      return;
    }

    delay(20);
    return;
  }

  if (state == PRESENCE) {
    unsigned long elapsed = millis() - stateStart;
    unsigned long remaining = (elapsed < presenceDuration) ? (presenceDuration - elapsed) : 0;
    showSeconds(remaining);

    float distance = readDistanceCm();

    if (distance >= 0) {
      lastGoodDistance = distance;
      if (distance > DISTANCE_THRESHOLD_CM) {
        distanceMisses++;
      } else {
        distanceMisses = 0;
      }
    } else {
      if (lastGoodDistance >= 0 && lastGoodDistance <= DISTANCE_THRESHOLD_CM) {
      } else {
        distanceMisses++;
      }
    }

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print(" cm | lastGood: ");
    Serial.print(lastGoodDistance);
    Serial.print(" cm | misses: ");
    Serial.println(distanceMisses);

    if (distanceMisses >= MAX_MISSES) {
      startPunish();
      return;
    }

    if (elapsed >= presenceDuration) {
      tone(BUZZER_PIN, SUCCESS_BEEP_FREQ, SUCCESS_BEEP_DURATION_MS);
      startIdle(true);
      return;
    }

    delay(50);
    return;
  }

  if (state == PUNISH) {
    display.showNumberDecEx(0, 0b01000000, true);

    if (millis() - punishBlinkAt >= PUNISH_BLINK_MS) {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      punishBlinkAt = millis();
    }

    if (buttonClicked()) {
      digitalWrite(LED_PIN, LOW);
      startIdle(true);
      return;
    }

    delay(20);
    return;
  }
}


// Made by Viteckpl with the help from Gemini 3.1 Pro
