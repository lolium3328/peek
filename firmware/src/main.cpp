#include <Arduino.h>

#include "app/AppController.h"

AppController app;

void setup() {
  app.begin();
}

void loop() {
  app.loop();
}
