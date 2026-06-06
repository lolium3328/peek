#include <Arduino.h>

#include "drivers/MotorDriver.h"

namespace {
MotorDriver motor;
String input;
enum class LastCommand {
  Effect,
  Vibrate,
};
LastCommand lastCommand = LastCommand::Effect;
uint8_t lastEffect = 1;
uint8_t lastStrength = 80;
uint32_t lastDurationMs = 120;
bool toneActive = false;
bool toneOutputHigh = false;
uint8_t toneStrength = 80;
uint8_t toneDutyPercent = 50;
uint32_t tonePeriodUs = 10000;
uint32_t toneHighUs = 5000;
uint32_t tonePhaseStartedUs = 0;

void printMenu() {
  Serial.println();
  Serial.println("DRV2605L motor effect test");
  Serial.println("Commands:");
  Serial.println("  1-117        play a library effect");
  Serial.println("  e 12         play a library effect");
  Serial.println("  v 80 200     vibrate strength 80 for 200ms");
  Serial.println("  s 80         set realtime strength 80");
  Serial.println("  m 120 90 50  motor tone 120Hz, strength 90, duty 50%");
  Serial.println("  q            stop motor tone");
  Serial.println("  x            stop realtime vibration");
  Serial.println("  r            replay the last command");
  Serial.print("> ");
}

void stopTone() {
  toneActive = false;
  toneOutputHigh = false;
}

void playEffect(uint8_t effect) {
  if (effect < 1 || effect > 117) {
    Serial.println("Effect must be 1-117.");
    Serial.print("> ");
    return;
  }

  lastEffect = effect;
  lastCommand = LastCommand::Effect;
  stopTone();
  Serial.print("Playing effect ");
  Serial.println(effect);
  motor.play(effect);
  Serial.print("> ");
}

void vibrate(uint8_t strength, uint32_t durationMs) {
  if (strength < 1 || strength > 127) {
    Serial.println("Strength must be 1-127.");
    Serial.print("> ");
    return;
  }
  if (durationMs < 1 || durationMs > 5000) {
    Serial.println("Duration must be 1-5000ms.");
    Serial.print("> ");
    return;
  }

  lastStrength = strength;
  lastDurationMs = durationMs;
  lastCommand = LastCommand::Vibrate;
  stopTone();
  Serial.print("Vibrating strength ");
  Serial.print(strength);
  Serial.print(" for ");
  Serial.print(durationMs);
  Serial.println("ms");
  motor.vibrate(strength, durationMs);
  Serial.print("> ");
}

void setRealtimeStrength(uint8_t strength) {
  if (strength > 127) {
    Serial.println("Strength must be 0-127.");
    Serial.print("> ");
    return;
  }

  stopTone();
  Serial.print("Realtime strength ");
  Serial.println(strength);
  motor.setRealtimeStrength(strength);
  Serial.print("> ");
}

void stopMotor() {
  Serial.println("Stopping motor");
  stopTone();
  motor.stop();
  Serial.print("> ");
}

void startMotorTone(uint16_t frequencyHz, uint8_t strength, uint8_t dutyPercent) {
  if (frequencyHz < 1 || frequencyHz > 500) {
    Serial.println("Frequency must be 1-500Hz.");
    Serial.print("> ");
    return;
  }
  if (strength < 1 || strength > 127) {
    Serial.println("Strength must be 1-127.");
    Serial.print("> ");
    return;
  }
  if (dutyPercent < 1 || dutyPercent > 99) {
    Serial.println("Duty must be 1-99%.");
    Serial.print("> ");
    return;
  }

  toneStrength = strength;
  toneDutyPercent = dutyPercent;
  tonePeriodUs = 1000000UL / frequencyHz;
  toneHighUs = (tonePeriodUs * toneDutyPercent) / 100;
  if (toneHighUs == 0) {
    toneHighUs = 1;
  }
  tonePhaseStartedUs = micros();
  toneOutputHigh = true;
  toneActive = true;
  motor.setRealtimeStrength(toneStrength);

  Serial.print("Motor tone ");
  Serial.print(frequencyHz);
  Serial.print("Hz strength ");
  Serial.print(strength);
  Serial.print(" duty ");
  Serial.print(dutyPercent);
  Serial.println("%");
  Serial.print("> ");
}

void stopMotorTone() {
  stopTone();
  motor.stop();
  Serial.println("Motor tone stopped");
  Serial.print("> ");
}

void updateMotorTone() {
  if (!toneActive) {
    return;
  }

  const uint32_t now = micros();
  const uint32_t elapsed = now - tonePhaseStartedUs;
  if (toneOutputHigh) {
    if (elapsed < toneHighUs) {
      return;
    }
    toneOutputHigh = false;
    tonePhaseStartedUs = now;
    motor.setRealtimeStrength(0);
    return;
  }

  const uint32_t lowUs = tonePeriodUs - toneHighUs;
  if (elapsed < lowUs) {
    return;
  }
  toneOutputHigh = true;
  tonePhaseStartedUs = now;
  motor.setRealtimeStrength(toneStrength);
}

int readNumber(const String &command, int &offset) {
  while (offset < command.length() && command[offset] == ' ') {
    ++offset;
  }

  const int start = offset;
  while (offset < command.length() && isDigit(command[offset])) {
    ++offset;
  }

  if (start == offset) {
    return -1;
  }

  return command.substring(start, offset).toInt();
}

void handleCommand(String command) {
  if (command.length() == 0) {
    Serial.print("> ");
    return;
  }

  if (command == "r" || command == "R") {
    if (lastCommand == LastCommand::Effect) {
      playEffect(lastEffect);
    } else {
      vibrate(lastStrength, lastDurationMs);
    }
    return;
  }

  if (command == "x" || command == "X") {
    stopMotor();
    return;
  }

  if (command == "q" || command == "Q") {
    stopMotorTone();
    return;
  }

  if (command[0] == 'e' || command[0] == 'E') {
    int offset = 1;
    const int effect = readNumber(command, offset);
    playEffect(static_cast<uint8_t>(effect));
    return;
  }

  if (command[0] == 'v' || command[0] == 'V') {
    int offset = 1;
    const int strength = readNumber(command, offset);
    const int durationMs = readNumber(command, offset);
    vibrate(static_cast<uint8_t>(strength), static_cast<uint32_t>(durationMs));
    return;
  }

  if (command[0] == 's' || command[0] == 'S') {
    int offset = 1;
    const int strength = readNumber(command, offset);
    setRealtimeStrength(static_cast<uint8_t>(strength));
    return;
  }

  if (command[0] == 'm' || command[0] == 'M') {
    int offset = 1;
    const int frequencyHz = readNumber(command, offset);
    const int strength = readNumber(command, offset);
    const int dutyPercent = readNumber(command, offset);
    startMotorTone(static_cast<uint16_t>(frequencyHz),
                   static_cast<uint8_t>(strength),
                   static_cast<uint8_t>(dutyPercent));
    return;
  }

  const int effect = command.toInt();
  if (effect < 1 || effect > 117) {
    Serial.println("Enter 1-117, e <effect>, v <strength> <ms>, s <strength>, m <hz> <strength> <duty>, q, x, or r.");
    Serial.print("> ");
    return;
  }

  playEffect(static_cast<uint8_t>(effect));
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  if (!motor.begin()) {
    Serial.println("DRV2605L not found at I2C address 0x5A.");
    Serial.println("Check SDA=GPIO8, SCL=GPIO7, 3V3, GND, and EN.");
  } else {
    Serial.println("DRV2605L ready.");
  }

  printMenu();
}

void loop() {
  updateMotorTone();

  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      Serial.println();
      input.trim();
      handleCommand(input);
      input = "";
      continue;
    }
    if (ch == '\b' || ch == 127) {
      if (input.length() > 0) {
        input.remove(input.length() - 1);
        Serial.print("\b \b");
      }
      continue;
    }
    if (isPrintable(ch)) {
      input += ch;
      Serial.print(ch);
    }
  }
}
