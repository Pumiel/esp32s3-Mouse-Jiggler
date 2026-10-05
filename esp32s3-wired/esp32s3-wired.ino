#include <Arduino.h>
#include "USB.h"
#include "USBHIDMouse.h"
#include "esp_system.h"
#include <math.h>

#if !defined(ARDUINO_USB_MODE) || ARDUINO_USB_MODE != 0
#error Select Tools > USB Mode > USB-OTG (TinyUSB).
#endif

USBHIDMouse Mouse;
int x = 0;
int y = 0;

void moveSmoothly(int dx, int dy) {
  const int steps = 20;
  int sentX = 0;
  int sentY = 0;

  for (int i = 1; i <= steps; ++i) {
    const float t = float(i) / steps;
    const float eased = 0.5f * (1.0f - cosf(PI * t));
    const int targetX = lroundf(dx * eased);
    const int targetY = lroundf(dy * eased);
    const int stepX = targetX - sentX;
    const int stepY = targetY - sentY;

    if (stepX != 0 || stepY != 0) {
      Mouse.move(stepX, stepY);
    }
    sentX = targetX;
    sentY = targetY;
    delay(10);
  }
}

void setup() {
  randomSeed(esp_random());

  // Present the requested Dell USB identity.
  USB.VID(0x413C);
  USB.PID(0x250E);
  USB.productName("Dell Laser Mouse MS3220");
  USB.manufacturerName("Dell");

  Mouse.begin();
  USB.begin();
  delay(5000);
}

void loop() {
  const int nextX = constrain(x + random(-12, 13), -40, 40);
  const int nextY = constrain(y + random(-12, 13), -40, 40);

  if (nextX != x || nextY != y) {
    moveSmoothly(nextX - x, nextY - y);
    x = nextX;
    y = nextY;
  }

  delay(random(700, 3001));
}
