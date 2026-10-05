# ESPmouse: wired ESP32-S3 mouse

This fork currently has one active sketch: [`esp32s3-wired/esp32s3-wired.ino`](esp32s3-wired/esp32s3-wired.ino). It presents a USB HID mouse through the ESP32-S3's native USB port. It is based on [esp32s3-Mouse-Jiggler](https://github.com/rithwikmakesthings/esp32s3-Mouse-Jiggler).

## What it does

After a five-second startup delay, the sketch sends small random mouse movements every 0.7–3 seconds. Each movement is divided into 20 steps with cosine easing over about 200 ms. It sends no clicks or keystrokes and continues until the USB connection is removed. The firmware keeps its reported movement within a virtual ±40-count range; computer pointer acceleration may affect the cursor's actual screen position.

The USB product name, manufacturer, VID, and PID are set to `Dell Laser Mouse MS3220`, `Dell`, `0x413C`, and `0x250E` as requested for this prototype.

## Build and test

1. Open `esp32s3-wired/esp32s3-wired.ino` in Arduino IDE with **esp32 by Espressif Systems** installed.
2. Select the board definition that matches your ESP32-S3. For a generic board, use **ESP32S3 Dev Module**.
3. Set **USB Mode: USB-OTG (TinyUSB)** and **USB CDC On Boot: Disabled**. When uploading through the board's UART port, use **Upload Mode: UART0 / Hardware CDC**.
4. Upload through UART, then connect the native **USB/OTG** port to the computer. Allow five seconds for startup, then watch for movement.
5. To stop immediately, disconnect the USB/OTG connection.

The exact board model and a hardware test result have not yet been recorded in this repository.
