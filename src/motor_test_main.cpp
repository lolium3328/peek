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

void printMenu() {
  Serial.println();
  Serial.println("DRV2605L motor effect test");
  Serial.println("Commands:");
  Serial.println("  1-117        play a library effect");
  Serial.println("  e 12         play a library effect");
  Serial.println("  v 80 200     vibrate strength 80 for 200ms");
  Serial.println("  r            replay the last command");
  Serial.print("> ");
}

void playEffect(uint8_t effect) {
  if (effect < 1 || effect > 117) {
    Serial.println("Effect must be 1-117.");
    Serial.print("> ");
    return;
  }

  lastEffect = effect;
  lastCommand = LastCommand::Effect;
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
  Serial.print("Vibrating strength ");
  Serial.print(strength);
  Serial.print(" for ");
  Serial.print(durationMs);
  Serial.println("ms");
  motor.vibrate(strength, durationMs);
  Serial.print("> ");
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

  const int effect = command.toInt();
  if (effect < 1 || effect > 117) {
    Serial.println("Enter 1-117, e <effect>, v <strength> <ms>, or r.");
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
