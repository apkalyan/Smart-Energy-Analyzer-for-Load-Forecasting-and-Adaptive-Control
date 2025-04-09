#include <WiFi.h>
#include <esp_now.h>
#include <ZMPT101B.h>
#include <ACS712.h>

#define RELAY_PIN 5  // Change based on wiring
#define VOLTAGE_SENSOR_PIN 34  // ZMPT101B
#define CURRENT_SENSOR_PIN 35  // ACS712
#define SENSITIVITY 636.75f   // Adjust based on calibration
#define CURRENT_FACTOR 150.00f   

// Initialize Sensors
ZMPT101B voltageSensor(VOLTAGE_SENSOR_PIN, 50.0);
ACS712 ACS(CURRENT_SENSOR_PIN, 5, 4095, 66);

// Edge Node MAC Address (ESP32-S2) - Update this!
uint8_t edgeMAC[] = {0x80, 0x65, 0x99, 0x41, 0x04, 0xC0};

// Struct for Power Data
typedef struct __attribute__((packed)) {
    uint8_t senderID;
    float voltage;
    float current;
    float power;
} PowerData;

PowerData powerData;

// ESP-NOW Callback (Relay Command Reception)
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    String command = String((char*)incomingData);

    if (command == "ON") {
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("⚡ Relay ON");
    } else if (command == "OFF") {
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("⚡ Relay OFF");
    }
}

// ESP-NOW Callback (Sent Data Status)
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("📡 Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✅ Success" : "❌ Failed");
}

// Read Voltage
float readVoltage() { 
    return voltageSensor.getRmsVoltage(); 
}

// Read Current with threshold correction & no negative values
float readCurrent() { 
    float current = (ACS.mA_AC() - CURRENT_FACTOR) / 1000.0;  // Convert mA to A
    if (fabs(current) < 0.005 || current < 0) {  // Filter small and negative values
        return 0.0;
    }
    return current;
}

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);  // Default OFF

    WiFi.mode(WIFI_STA);
    delay(100);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init Failed!");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);

    // Register Edge Node as Peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, edgeMAC, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Failed to Add Edge Node as Peer!");
    } else {
        Serial.println("✅ Edge Node Added as Peer!");
    }

    // Sensor Calibration
    voltageSensor.setSensitivity(SENSITIVITY);
    ACS.autoMidPoint();
}

void loop() {
    // Read Sensor Data
    powerData.senderID = 3;  // Change per node (1,2,3,4)
    powerData.voltage = readVoltage();
    powerData.current = readCurrent();
    powerData.power = powerData.voltage * powerData.current;

    Serial.printf("⚡ Sending: Node %d | Voltage: %.2fV | Current: %.2fA | Power: %.2fW\n",
                  powerData.senderID, powerData.voltage, powerData.current, powerData.power);

    // Send Data to Edge Node
    esp_now_send(edgeMAC, (uint8_t *)&powerData, sizeof(PowerData));

    delay(2000);  // Adjust for real-time transmission
}
