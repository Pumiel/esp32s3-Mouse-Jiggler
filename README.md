# ESPmouse for ESP32-S3

Two sketches are available:

- [`esp32s3-wired/esp32s3-wired.ino`](esp32s3-wired/esp32s3-wired.ino): the original smooth USB mouse.
- [`esp32s3-ble-webui/esp32s3-ble-webui.ino`](esp32s3-ble-webui/esp32s3-ble-webui.ino): a BLE mouse with a Wi-Fi control page.

The project began from [esp32s3-Mouse-Jiggler](https://github.com/rithwikmakesthings/esp32s3-Mouse-Jiggler). The BLE control idea was informed by [ESP32-S3-Bluetooth-Mouse-Jiggler-Pro](https://github.com/joelcosta2/ESP32-S3-Bluetooth-Mouse-Jiggler-Pro); this implementation keeps the smooth ESPmouse movement and uses the BLE HID library included with the Espressif Arduino core.

## BLE mouse and web controls

The wireless sketch advertises as **Dell Laser Mouse MS3220**, with manufacturer **Dell** and BLE HID PnP vendor/product IDs **0x413C/0x250E**. Pair it with the computer over Bluetooth. A separate control device, such as a phone, joins the board's **ESPmouse-...** Wi-Fi network and opens **http://192.168.4.1**. The page lets you start and stop movement, set the maximum movement size and random pause range, and set an optional run timer. The board starts moving when paired unless stopped from the page. It sends no clicks or keystrokes.

The Wi-Fi password is generated on the first boot, saved on the board, and printed on the **UART serial monitor at 115200 baud** each boot. The web page never returns the password. Settings persist after power cycling; the running/stopped state does not. The control Wi-Fi network does not provide internet access.

Each movement follows the same 20-step cosine easing as the wired sketch and remains within a virtual ±40-count area. Computer pointer acceleration can change the actual screen position. The smooth movement is nonblocking, so the web page remains responsive. The Wi-Fi and BLE radios share the board; range and responsiveness depend on the environment.

### Build and test the BLE sketch

1. Install **esp32 by Espressif Systems** in Arduino IDE and select the board definition matching your ESP32-S3. The sketch uses only libraries included with this board package.
2. Open `esp32s3-ble-webui/esp32s3-ble-webui.ino` and upload through the board's **UART** port. Keep it connected to USB power; no OTG data connection is needed for the BLE mouse.
3. Open the UART serial monitor at **115200 baud**. Reset the board if needed to see the generated Wi-Fi name, password, and page address.
4. On the computer, remove the previous **ESPmouse BLE** pairing if present, then pair **Dell Laser Mouse MS3220**. If the old name remains visible, remove it and scan again; operating systems can cache Bluetooth names and HID details.
5. Join the ESPmouse Wi-Fi network from a phone or other control device, then visit **http://192.168.4.1**. Use **Stop movement** when you want the pointer still. To stop immediately without the page, remove board power.

The BLE sketch compiles for **ESP32S3 Dev Module** with Espressif Arduino core 3.3.12. Bluetooth pairing, the Wi-Fi page, and movement still need a hardware test.

## Wired mouse

After a five-second startup delay, the sketch sends small random mouse movements every 0.7–3 seconds. Each movement is divided into 20 steps with cosine easing over about 200 ms. It sends no clicks or keystrokes and continues until the USB connection is removed. The firmware keeps its reported movement within a virtual ±40-count range; computer pointer acceleration may affect the cursor's actual screen position.

The USB product name, manufacturer, VID, and PID are set to `Dell Laser Mouse MS3220`, `Dell`, `0x413C`, and `0x250E` as requested for this prototype.

### Build and test the wired sketch

1. Open `esp32s3-wired/esp32s3-wired.ino` in Arduino IDE with **esp32 by Espressif Systems** installed.
2. Select the board definition that matches your ESP32-S3. For a generic board, use **ESP32S3 Dev Module**.
3. Set **USB Mode: USB-OTG (TinyUSB)** and **USB CDC On Boot: Disabled**. When uploading through the board's UART port, use **Upload Mode: UART0 / Hardware CDC**.
4. Upload through UART, then connect the native **USB/OTG** port to the computer. Allow five seconds for startup, then watch for movement.
5. To stop immediately, disconnect the USB/OTG connection.

The exact board model and a hardware test result have not yet been recorded in this repository.
