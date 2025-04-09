#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Wi-Fi Credentials
const char* ssid = "Meant for BOYS";
const char* password = "34567893";

// MQTT Broker
const char* mqtt_server = "192.168.71.248";
WiFiClient espClient;
PubSubClient client(espClient);

// Power Data Structure
typedef struct __attribute__((packed)) {
    uint8_t senderID;
    float voltage;
    float current;
    float power;
} PowerData;

PowerData powerData;

// MAC Addresses of End Nodes (replace with actual MACs)
uint8_t endNodes[4][6] = {
    {0xFC, 0xE8, 0xC0, 0x74, 0xC5, 0xCC},
    {0x3C, 0x8A, 0x1F, 0x50, 0x8F, 0xAC},
    {0xD0, 0xEF, 0x76, 0x33, 0x96, 0x0C},
    {0xF4, 0x65, 0x0B, 0xE8, 0xB8, 0x74}
};

// System Variables
float lastPower[4] = {0};
bool relayState[4] = {false};
bool manualOverride[4] = {false};
uint8_t loadType[4] = {2, 2, 2, 1}; // 1 = essential, 2 = non-essential
unsigned long lastAggregate = 0;

// Helper: Get node index from node ID string
int getNodeIndex(String nodeID) {
    if (nodeID == "node1") return 0;
    if (nodeID == "node2") return 1;
    if (nodeID == "node3") return 2;
    if (nodeID == "node4") return 3;
    return -1;
}

// Helper: Match received MAC with endNodes[]
int getNodeIndexFromMAC(const uint8_t* mac) {
    for (int i = 0; i < 4; i++) {
        bool match = true;
        for (int j = 0; j < 6; j++) {
            if (mac[j] != endNodes[i][j]) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    return -1;
}

// Send Relay Command (explicit ON/OFF)
void sendRelayCommand(int index, bool state, const char* source = "edge") {
    const char* command = state ? "ON" : "OFF";
    esp_now_send(endNodes[index], (uint8_t*)command, strlen(command) + 1);
    relayState[index] = state;

    StaticJsonDocument<128> doc;
    doc["node_id"] = "node" + String(index + 1);
    doc["state"] = state;
    doc["source"] = source;
    char buffer[128];
    serializeJson(doc, buffer);
    client.publish("status/relay", buffer);

    Serial.printf("⚡ Relay %d set to %s\n", index + 1, command);
}

// ESP-NOW receive callback (with MAC verification)
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    if (len != sizeof(PowerData)) return;

    int idx = getNodeIndexFromMAC(info->peer_addr);
    if (idx == -1) {
        Serial.println("❌ Unknown sender MAC. Packet rejected.");
        return;
    }

    memcpy(&powerData, incomingData, sizeof(PowerData));
    uint8_t id = powerData.senderID;

    if (id != idx + 1) {
        Serial.printf("⚠ ID mismatch: MAC index = %d, reported ID = %d\n", idx, id);
        return;
    }

    lastPower[idx] = powerData.power;

    StaticJsonDocument<200> doc;
    doc["id"] = id;
    doc["voltage"] = powerData.voltage;
    doc["current"] = powerData.current;
    doc["power"] = powerData.power;
    doc["timestamp"] = millis();
    char buffer[200];
    serializeJson(doc, buffer);
    client.publish("power/nodes", buffer);

    Serial.printf("📥 Verified Node %d | V: %.2fV | I: %.2fA | P: %.2fW\n",
                  id, powerData.voltage, powerData.current, powerData.power);
}

// MQTT receive callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0';
    String msg = String((char*)payload);
    Serial.printf("📡 MQTT [%s]: %s\n", topic, msg.c_str());

    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (err) {
        Serial.println("❌ JSON parse error");
        return;
    }

    if (String(topic) == "relay/manual" || String(topic) == "relay/auto") {
        String nodeID = doc["node_id"];
        bool state = doc["state"];
        int idx = getNodeIndex(nodeID);
        if (idx >= 0) {
            if (String(topic) == "relay/manual") {
                manualOverride[idx] = true;
            }
            sendRelayCommand(idx, state, String(topic) == "relay/manual" ? "manual" : "auto");

            // Acknowledge
            StaticJsonDocument<64> ack;
            ack["node_id"] = nodeID;
            ack["status"] = "ok";
            char ackMsg[64];
            serializeJson(ack, ackMsg);
            client.publish("relay/manual/ack", ackMsg);
        }
    }
}

// MQTT reconnect logic
void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("Connecting to MQTT...");
        if (client.connect("EdgeNode")) {
            Serial.println("connected!");
            client.subscribe("relay/manual");
            client.subscribe("relay/auto");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5s");
            delay(5000);
        }
    }
}

// Setup
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    Serial.println("📶 WiFi connected");

    // ESP-NOW Init
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW init failed");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    for (int i = 0; i < 4; i++) {
        memcpy(peerInfo.peer_addr, endNodes[i], 6);
        esp_now_add_peer(&peerInfo);
    }

    // MQTT Init
    client.setServer(mqtt_server, 1883);
    client.setCallback(mqttCallback);
    reconnectMQTT();
}

// Loop
void loop() {
    if (!client.connected()) reconnectMQTT();
    client.loop();

    if (millis() - lastAggregate > 5000) {
        float totalPower = 0;
        for (int i = 0; i < 4; i++) totalPower += lastPower[i];

        StaticJsonDocument<100> doc;
        doc["total"] = totalPower;
        doc["timestamp"] = millis();
        char buffer[100];
        serializeJson(doc, buffer);
        client.publish("power/total", buffer);
        Serial.printf("🔢 Total Power: %.2f W\n", totalPower);

        lastAggregate = millis();
    }
}