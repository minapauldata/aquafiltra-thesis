#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── DISPLAY ──
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 5
#define OLED_SCL 4
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ── WIFI ──
const char* ssid     = "JUSTINIANO";
const char* password = "Yalega123456789";

// ── SERVER ──
const char* serverURL = "https://marlys-scientistic-realizingly.ngrok-free.dev/api/sensor-data";

// ── SENSOR PINS ──
const int PH_PIN        = 36; // SVP - ADC1 ✅
const int TURBIDITY_PIN = 39; // SVN - ADC1 ✅
const int TDS_PIN       = 26; // ADC2 - read with WiFi off

// ── CALIBRATION ──
float pH_offset   = 2.4;  // increase if pH too low, decrease if too high
float turb_offset = 0.0;  // decrease if turbidity too high in clear water
float tds_factor  = 0.1;  // decrease if TDS too high in purified water

// ── WIFI CONNECT ──
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(300);
    attempt++;
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi Failed - will retry");
  }
}

// ── DISPLAY SCREENS ──

void showSplash() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(8, 8);
  display.println("AquaFiltra");
  display.setTextSize(1);
  display.setCursor(20, 35);
  display.println("Water Monitor v1.0");
  display.setCursor(30, 50);
  display.println("Starting...");
  display.display();
  delay(2500);
}

void showConnecting() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting to WiFi");
  display.println(ssid);
  display.setCursor(0, 30);
  display.println("Please wait...");
  display.display();
}

void showConnected() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WiFi Connected!");
  display.setCursor(0, 16);
  display.println(WiFi.localIP().toString());
  display.setCursor(0, 35);
  display.println("Reading sensors...");
  display.display();
  delay(2000);
}

void showDisplay(float ph, float turb, float tds, String status) {
  display.clearDisplay();

  // ── TOP STATUS BAR ──
  display.fillRect(0, 0, 128, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);

  if (status == "normal") {
    display.setCursor(18, 3);
    display.print("SAFE TO DRINK  OK");
  } else if (status == "warning") {
    display.setCursor(16, 3);
    display.print("USE WITH CAUTION!");
  } else {
    display.setCursor(14, 3);
    display.print("NOT SAFE TO DRINK!");
  }

  // ── SENSOR VALUES ──
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // pH
  display.setCursor(0, 18);
  display.print("pH      : ");
  display.print(ph, 2);
  if (ph > 0 && (ph < 6.5 || ph > 8.5)) display.print(" !");

  // Turbidity
  display.setCursor(0, 30);
  display.print("Turbid  : ");
  display.print(turb, 1);
  display.print(" NTU");
  if (turb > 4.0) display.print(" !");

  // TDS
  display.setCursor(0, 42);
  display.print("TDS     : ");
  display.print(tds, 0);
  display.print(" ppm");
  if (tds > 500.0) display.print(" !");

  // ── BOTTOM STATUS LINE ──
  display.drawLine(0, 54, 128, 54, SSD1306_WHITE);
  display.setCursor(0, 57);
  if (status == "normal") {
    display.print("Status : NORMAL");
  } else if (status == "warning") {
    display.print("Status : WARNING");
  } else {
    display.print("Status : DANGER");
  }

  display.display();
}

// ── SENSOR READS ──

float readPH() {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PH_PIN);
    delay(10);
  }
  int raw = sum / 10;
  Serial.println("pH raw: " + String(raw));

  if (raw < 100) return 0.0;

  float voltage = raw * (3.3 / 4095.0);
  float ph = 3.5 * voltage + pH_offset;
  return constrain(ph, 0.0, 14.0);
}

float readTurbidity() {
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(TURBIDITY_PIN);
    delay(10);
  }
  int raw = sum / 10;
  Serial.println("Turbidity raw: " + String(raw));

  if (raw < 100) return 0.0;

  float voltage = raw * (3.3 / 4095.0);

  // Correct formula — high voltage = clear water = low NTU
  float ntu = -1120.4 * (voltage * voltage) + 5742.3 * voltage - 4352.9;

  // Apply calibration offset
  ntu = ntu + turb_offset;

  // Clear water gives near 0 or negative — clamp to 0
  if (ntu < 0) ntu = 0.0;

  return constrain(ntu, 0.0, 3000.0);
}

float readTDS() {
  // Completely turn off WiFi radio to use ADC2
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(500);

  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(TDS_PIN);
    delay(10);
  }
  int raw = sum / 10;
  Serial.println("TDS raw: " + String(raw));

  // Turn WiFi back on
  WiFi.mode(WIFI_STA);
  connectWiFi();

  if (raw < 100) return 0.0;

  float voltage = raw * (3.3 / 4095.0);
  float tds = voltage * 500 * tds_factor;
  return constrain(tds, 0.0, 1000.0);
}

// ── STATUS ──

String getStatus(float ph, float turb, float tds) {
  if (ph == 0.0 && turb == 0.0 && tds == 0.0) return "normal";

  if (ph > 0 && (ph < 6.5 || ph > 9.0)) return "danger";
  if (turb > 10.0 || tds > 1000.0)       return "danger";
  if (ph > 0 && (ph < 6.5 || ph > 8.5)) return "warning";
  if (turb > 4.0  || tds > 500.0)        return "warning";
  return "normal";
}

// ── SEND DATA ──

void sendData(float ph, float turb, float tds, String status) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping send.");
    return;
  }

  display.fillRect(0, 57, 128, 7, SSD1306_BLACK);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 57);
  display.print("Sending data...");
  display.display();

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("ngrok-skip-browser-warning", "true");

  StaticJsonDocument<200> doc;
  doc["ph_level"]  = round(ph   * 100.0) / 100.0;
  doc["turbidity"] = round(turb * 100.0) / 100.0;
  doc["tds"]       = round(tds  * 10.0)  / 10.0;
  doc["status"]    = status;

  String jsonBody;
  serializeJson(doc, jsonBody);

  int httpCode = http.POST(jsonBody);

  if (httpCode == 200 || httpCode == 201) {
    Serial.println("✅ Data sent!");
    display.fillRect(0, 57, 128, 7, SSD1306_BLACK);
    display.setCursor(0, 57);
    display.print("Status : " + status);
    display.display();
  } else {
    Serial.println("❌ Failed: " + String(httpCode));
    display.fillRect(0, 57, 128, 7, SSD1306_BLACK);
    display.setCursor(0, 57);
    display.print("Send failed!");
    display.display();
  }

  http.end();
}

// ── SETUP ──

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found!");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  showSplash();
  showConnecting();
  connectWiFi();
  showConnected();
}

// ── MAIN LOOP ──

void loop() {
  Serial.println("--- Reading Sensors ---");

  float ph   = readPH();
  float turb = readTurbidity();
  float tds  = readTDS();

  Serial.println("pH: "        + String(ph));
  Serial.println("Turbidity: " + String(turb));
  Serial.println("TDS: "       + String(tds));

  String status = getStatus(ph, turb, tds);
  Serial.println("Status: " + status);

  showDisplay(ph, turb, tds, status);
  sendData(ph, turb, tds, status);

  Serial.println("--- Done. Waiting 5s ---");
  delay(5000);
}