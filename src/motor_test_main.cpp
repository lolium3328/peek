#include <Arduino.h>

#include "drivers/MotorDriver.h"

namespace {
MotorDriver motor;
String input;

void printMenu() {
  Serial.println();
  Serial.println("DRV2605L motor effect test");
  Serial.println("Type 1-47, then press Enter.");
  Serial.println("Type r to replay the last effect.");
  Serial.print("> ");
}

void playEffect(uint8_t effect) {
  if (effect < 1 || effect > 47) {
    Serial.println("Effect must be 1-47.");
    Serial.print("> ");
    return;
  }

  Serial.print("Playing effect ");
  Serial.println(effect);
  motor.play(effect);
  Serial.print("> ");
}

void handleCommand(const String &command) {
  static uint8_t lastEffect = 1;

  if (command.length() == 0) {
    Serial.print("> ");
    return;
  }

  if (command == "r" || command == "R") {
    playEffect(lastEffect);
    return;
  }

  const int effect = command.toInt();
  if (effect < 1 || effect > 47) {
    Serial.println("Enter a number from 1 to 47.");
    Serial.print("> ");
    return;
  }

  lastEffect = static_cast<uint8_t>(effect);
  playEffect(lastEffect);
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
      input.trim();
      handleCommand(input);
      input = "";
      continue;
    }
    if (isPrintable(ch)) {
      input += ch;
    }
  }
}
