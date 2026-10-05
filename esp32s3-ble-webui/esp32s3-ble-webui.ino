#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <math.h>

// BLE HID mouse: three buttons, relative X and Y, report ID 1.
const uint8_t mouseDescriptor[] = {
  0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x01,
  0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01,
  0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03,
  0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05,
  0x81, 0x03, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
  0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02,
  0x81, 0x06, 0xC0, 0xC0
};

struct MouseReport {
  uint8_t buttons;
  int8_t x;
  int8_t y;
} __attribute__((packed));

const char page[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESPmouse</title><style>
body{font:16px system-ui,sans-serif;background:#f4f5f7;color:#20242a;margin:0;padding:24px}
main{max-width:520px;margin:auto;background:white;border-radius:16px;padding:24px;box-shadow:0 4px 20px #0001}
h1{margin:0 0 4px}p{line-height:1.4}label{display:block;margin:16px 0 5px}
input{font:inherit;box-sizing:border-box;width:100%;padding:9px;border:1px solid #bbb;border-radius:7px}
button{font:inherit;padding:11px 18px;border:0;border-radius:8px;cursor:pointer;margin:12px 8px 0 0}
#start{background:#176b42;color:white}#stop{background:#b42d35;color:white}#save{background:#233b64;color:white}
#status{font-weight:600}small{color:#555}#message{min-height:1.4em}
</style></head><body><main>
<h1>ESPmouse</h1><p id="status">Loading…</p>
<button id="start" type="button">Start movement</button><button id="stop" type="button">Stop movement</button>
<form id="settings">
<label for="distance">Maximum change per movement (counts)</label><input id="distance" name="distance" type="number" min="1" max="20" required>
<label for="minPause">Minimum pause (ms)</label><input id="minPause" name="minPause" type="number" min="500" max="60000" required>
<label for="maxPause">Maximum pause (ms)</label><input id="maxPause" name="maxPause" type="number" min="500" max="60000" required>
<label for="minutes">Run timer (minutes; 0 = continuous)</label><input id="minutes" name="minutes" type="number" min="0" max="480" required>
<button id="save" type="submit">Save settings</button></form>
<p id="message" role="status"></p><small>Pair your computer with “Dell Laser Mouse MS3220”. Keep this Wi-Fi network connected on the control device.</small>
</main><script>
const $=id=>document.getElementById(id);let loaded=false;
async function request(path,body){const r=await fetch(path,{method:body?'POST':'GET',body,cache:'no-store'});if(!r.ok)throw Error(await r.text());return r.json()}
async function refresh(){try{const s=await request('/api/status');$('status').textContent=(s.connected?'Bluetooth connected':'Waiting for Bluetooth')+' · '+(s.active?'Movement on':'Movement off')+(s.remainingSec!==null?' · '+Math.ceil(s.remainingSec/60)+' min left':'');if(!loaded){for(const k of ['distance','minPause','maxPause','minutes'])$(k).value=s[k];loaded=true}}catch(e){$('status').textContent='Control connection lost'}}
async function action(path,body){try{await request(path,body);$('message').textContent='Saved';await refresh()}catch(e){$('message').textContent=e.message}}
$('start').onclick=()=>action('/api/start','start=1');$('stop').onclick=()=>action('/api/stop','stop=1');
$('settings').onsubmit=e=>{e.preventDefault();action('/api/settings',new URLSearchParams(new FormData(e.target)))};
refresh();setInterval(refresh,2000);
</script></body></html>
)HTML";

Preferences prefs;
WebServer web(80);
BLECharacteristic *mouseInput = nullptr;
volatile bool bleConnected = false;

bool active = true;
int distance = 12;
uint32_t minPause = 700;
uint32_t maxPause = 3000;
uint16_t minutes = 0;
uint32_t stopAt = 0;

int virtualX = 0;
int virtualY = 0;
int pathX = 0;
int pathY = 0;
int sentX = 0;
int sentY = 0;
uint8_t step = 0;
bool moving = false;
uint32_t nextStepAt = 0;
uint32_t nextMoveAt = 0;

class MouseServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { bleConnected = true; }
  void onDisconnect(BLEServer *) override {
    bleConnected = false;
    BLEDevice::startAdvertising();
  }
};

void sendMotion(int dx, int dy) {
  if (!bleConnected || (dx == 0 && dy == 0)) return;
  MouseReport report = {0, static_cast<int8_t>(dx), static_cast<int8_t>(dy)};
  mouseInput->setValue(reinterpret_cast<uint8_t *>(&report), sizeof(report));
  mouseInput->notify();
}

bool parseNumber(const String &name, uint32_t low, uint32_t high, uint32_t &value) {
  if (!web.hasArg(name)) return false;
  String raw = web.arg(name);
  if (raw.isEmpty()) return false;
  uint32_t parsed = 0;
  for (size_t i = 0; i < raw.length(); ++i) {
    if (raw[i] < '0' || raw[i] > '9') return false;
    parsed = parsed * 10 + (raw[i] - '0');
    if (parsed > high) return false;
  }
  if (parsed < low) return false;
  value = parsed;
  return true;
}

void jsonReply(const String &body) {
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", body);
}

void handleStatus() {
  String body = "{\"connected\":";
  body += bleConnected ? "true" : "false";
  body += ",\"active\":";
  body += active ? "true" : "false";
  body += ",\"distance\":" + String(distance);
  body += ",\"minPause\":" + String(minPause);
  body += ",\"maxPause\":" + String(maxPause);
  body += ",\"minutes\":" + String(minutes);
  body += ",\"remainingSec\":";
  if (active && minutes > 0 && stopAt != 0) {
    int32_t remaining = static_cast<int32_t>(stopAt - millis());
    body += String(remaining > 0 ? (remaining + 999) / 1000 : 0);
  } else {
    body += "null";
  }
  body += "}";
  jsonReply(body);
}

void handleSettings() {
  uint32_t d, low, high, timer;
  if (!parseNumber("distance", 1, 20, d) || !parseNumber("minPause", 500, 60000, low) ||
      !parseNumber("maxPause", 500, 60000, high) || !parseNumber("minutes", 0, 480, timer) || low > high) {
    web.send(400, "text/plain", "Enter valid settings, with minimum pause no greater than maximum pause.");
    return;
  }
  distance = d;
  minPause = low;
  maxPause = high;
  minutes = timer;
  prefs.putUInt("distance", distance);
  prefs.putUInt("minPause", minPause);
  prefs.putUInt("maxPause", maxPause);
  prefs.putUInt("minutes", minutes);
  stopAt = active && bleConnected && minutes > 0 ? millis() + minutes * 60000UL : 0;
  moving = false;
  nextMoveAt = millis();
  jsonReply("{\"ok\":true}");
}

void startMovement() {
  active = true;
  stopAt = bleConnected && minutes > 0 ? millis() + minutes * 60000UL : 0;
  nextMoveAt = millis();
  jsonReply("{\"ok\":true}");
}

void stopMovement() {
  active = false;
  moving = false;
  stopAt = 0;
  jsonReply("{\"ok\":true}");
}

void setupBle() {
  BLEDevice::init("Dell Laser Mouse MS3220");
  BLESecurity *security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  security->setAuthenticationMode(true, false, true);

  BLEServer *bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new MouseServerCallbacks());
  BLEHIDDevice *hid = new BLEHIDDevice(bleServer);
  hid->manufacturer()->setValue("Dell");
  hid->pnp(0x02, 0x413C, 0x250E, 0x0001);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap(const_cast<uint8_t *>(mouseDescriptor), sizeof(mouseDescriptor));
  mouseInput = hid->inputReport(1);
  hid->setBatteryLevel(100);  // Required HID service field; USB-powered, no battery measurement.
  hid->startServices();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(HID_MOUSE);
  advertising->addServiceUUID(hid->hidService()->getUUID());
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
}

void setupWifi() {
  String password = prefs.getString("apPass", "");
  if (password.length() != 16) {
    char generated[17];
    snprintf(generated, sizeof(generated), "%08lX%08lX", static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    password = generated;
    prefs.putString("apPass", password);
  }
  String suffix = String(static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFF), HEX);
  suffix.toUpperCase();
  String ssid = "ESPmouse-" + suffix;
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(ssid.c_str(), password.c_str())) {
    Serial.println("Could not start Wi-Fi control network");
    return;
  }
  web.on("/", HTTP_GET, []() { web.send_P(200, "text/html", page); });
  web.on("/api/status", HTTP_GET, handleStatus);
  web.on("/api/settings", HTTP_POST, handleSettings);
  web.on("/api/start", HTTP_POST, startMovement);
  web.on("/api/stop", HTTP_POST, stopMovement);
  web.begin();
  Serial.printf("Control Wi-Fi: %s\nPassword: %s\nOpen: http://%s\n", ssid.c_str(), password.c_str(), WiFi.softAPIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());
  prefs.begin("espmouse", false);
  distance = constrain(static_cast<int>(prefs.getUInt("distance", 12)), 1, 20);
  minPause = constrain(prefs.getUInt("minPause", 700), 500UL, 60000UL);
  maxPause = constrain(prefs.getUInt("maxPause", 3000), minPause, 60000UL);
  uint32_t savedMinutes = prefs.getUInt("minutes", 0);
  minutes = savedMinutes > 480 ? 480 : savedMinutes;
  setupWifi();
  setupBle();
  nextMoveAt = millis() + 1000;
  Serial.println("Pair Dell Laser Mouse MS3220 with the computer.");
}

void loop() {
  web.handleClient();
  uint32_t now = millis();

  // Start a configured timer when the BLE host is first connected.
  if (active && bleConnected && minutes > 0 && stopAt == 0) {
    stopAt = now + minutes * 60000UL;
  }

  if (active && stopAt != 0 && static_cast<int32_t>(now - stopAt) >= 0) {
    active = false;
    moving = false;
  }
  if (!bleConnected) {
    moving = false;
    virtualX = 0;
    virtualY = 0;
    nextMoveAt = now + 1000;
    delay(2);
    return;
  }
  if (!active) {
    moving = false;
    delay(2);
    return;
  }

  if (moving && static_cast<int32_t>(now - nextStepAt) >= 0) {
    ++step;
    float t = static_cast<float>(step) / 20.0f;
    float eased = 0.5f * (1.0f - cosf(PI * t));
    int targetX = lroundf(pathX * eased);
    int targetY = lroundf(pathY * eased);
    int dx = targetX - sentX;
    int dy = targetY - sentY;
    sendMotion(dx, dy);
    virtualX += dx;
    virtualY += dy;
    sentX = targetX;
    sentY = targetY;
    nextStepAt = now + 10;
    if (step >= 20) {
      moving = false;
      nextMoveAt = now + random(minPause, maxPause + 1);
    }
  } else if (!moving && static_cast<int32_t>(now - nextMoveAt) >= 0) {
    const int targetX = constrain(virtualX + random(-distance, distance + 1), -40, 40);
    const int targetY = constrain(virtualY + random(-distance, distance + 1), -40, 40);
    pathX = targetX - virtualX;
    pathY = targetY - virtualY;
    if (pathX == 0 && pathY == 0) {
      nextMoveAt = now + random(minPause, maxPause + 1);
    } else {
      sentX = sentY = 0;
      step = 0;
      moving = true;
      nextStepAt = now;
    }
  }
  delay(2);
}
