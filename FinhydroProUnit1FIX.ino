#include <OneWire.h>
#include <DallasTemperature.h>
#include <Firebase_ESP_Client.h>
#include <WiFi.h>

// Firebase
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// --- Konfigurasi WiFi dan Firebase ---
#define WIFI_SSID "TEKMEK BLK PANGKEP"
#define WIFI_PASSWORD "TekmekPangkep1234"
#define API_KEY "AIzaSyCkFNQKsr_bZvnfwtw5ZuiB-Vhxr1HfjwY"
#define DATABASE_URL "https://finhydroprounit1-default-rtdb.asia-southeast1.firebasedatabase.app"

// --- Konfigurasi Suhu (DS18B20) ---
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// --- Konfigurasi pH ---
unsigned int ph_Value = 0;
float Voltage = 0.0;
const float V4 = 1.17;  // Tegangan untuk buffer pH 4
const float V7 = 1.10;  // Tegangan untuk buffer pH 7

// --- Konfigurasi EC ---
#define analogInPin 34  // Pin ADC untuk sensor EC
float calibrationFactor;  // Faktor kalibrasi EC

// --- Konfigurasi Relay ---
#define RELAY_POMPA_PH_UP 26
#define RELAY_POMPA_PH_DOWN 27
#define RELAY_POMPA_NUTRISI_1 14
#define RELAY_POMPA_NUTRISI_2 12
#define RELAY_PELTIER 13

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;
unsigned long sendDataPrevMillis = 0;

void setup() {
  Serial.begin(115200);

  // Inisialisasi sensor suhu
  sensors.begin();

  // Inisialisasi ADC pH
  analogReadResolution(12); // Resolusi ADC 12-bit

  // Kalibrasi EC
  int adcStandard = 700;  // Nilai ADC dari larutan standar
  float ecStandard = 264.0;  // Nilai EC larutan standar dalam μS/cm
  calibrationFactor = ecStandard / adcStandard;

  // Konfigurasi pin relay
  pinMode(RELAY_POMPA_PH_UP, OUTPUT);
  pinMode(RELAY_POMPA_PH_DOWN, OUTPUT);
  pinMode(RELAY_POMPA_NUTRISI_1, OUTPUT);
  pinMode(RELAY_POMPA_NUTRISI_2, OUTPUT);
  pinMode(RELAY_PELTIER, OUTPUT);

  digitalWrite(RELAY_POMPA_PH_UP, HIGH);
  digitalWrite(RELAY_POMPA_PH_DOWN, HIGH);
  digitalWrite(RELAY_POMPA_NUTRISI_1, HIGH);
  digitalWrite(RELAY_POMPA_NUTRISI_2, HIGH);
  digitalWrite(RELAY_PELTIER, HIGH);

  // Inisialisasi WiFi dan Firebase
  initWiFi();
  initFirebase();

  Serial.println("Inisialisasi selesai. Memulai pembacaan sensor...");
}

/* Fungsi WiFi */
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
}

/* Fungsi Firebase */
void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Ready");
    signupOK = true;
  } else {
    Serial.printf("Signup Error: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

/* Fungsi untuk mengontrol relay berdasarkan pH */
void controlRelayPH(float pH) {
  if (pH < 5.0) {
    digitalWrite(RELAY_POMPA_PH_UP, LOW);
    delay(10000);
    digitalWrite(RELAY_POMPA_PH_UP, HIGH);
  } else if (pH > 7.0) {
    digitalWrite(RELAY_POMPA_PH_DOWN, LOW);
    delay(10000);
    digitalWrite(RELAY_POMPA_PH_DOWN, HIGH);
  }
}

/* Fungsi untuk mengontrol relay berdasarkan suhu */
void controlRelaySuhu(float suhu) {
  if (suhu > 27.0) {
    digitalWrite(RELAY_PELTIER, LOW);
    delay(30000);
    digitalWrite(RELAY_PELTIER, HIGH);
  }
}

/* Fungsi untuk mengontrol relay berdasarkan EC */
void controlRelayEC(float ec) {
  if (ec < 1000.0) {
    digitalWrite(RELAY_POMPA_NUTRISI_1, LOW);
    digitalWrite(RELAY_POMPA_NUTRISI_2, LOW);
    delay(10000);
    digitalWrite(RELAY_POMPA_NUTRISI_1, HIGH);
    digitalWrite(RELAY_POMPA_NUTRISI_2, HIGH);
  }
}

/* Mengirim Data ke Firebase */
void sendDataToFirebase(float suhu, float pH, float ec) {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000)) {
    sendDataPrevMillis = millis();

    FirebaseJson json;
    json.set("fields/Suhu/doubleValue", suhu);
    json.set("fields/pH/doubleValue", pH);
    json.set("fields/EC/doubleValue", ec);

    if (Firebase.RTDB.setJSON(&fbdo, "Monitoring/Room1", &json)) {
      Serial.println("Data sent to Firebase");
    } else {
      Serial.println(fbdo.errorReason());
    }
  }
}

void loop() {
  sensors.requestTemperatures();
  float temperatureC = sensors.getTempCByIndex(0);

  ph_Value = 0;
  for (int i = 0; i < 10; i++) {
    ph_Value += analogRead(32);
    delay(10);
  }
  Voltage = (ph_Value / 10.0) * (3.3 / 4095.0);
  float slope = (4.0 - 7.0) / (V4 - V7);
  float intercept = 7.0 - slope * V7;
  float ph_act = slope * Voltage + intercept;

  int adcValue = analogRead(analogInPin);
  float ecValue = adcValue * calibrationFactor;

  sendDataToFirebase(temperatureC, ph_act, ecValue);

  controlRelayPH(ph_act);
  controlRelaySuhu(temperatureC);
  controlRelayEC(ecValue);

  delay(1000);
}
