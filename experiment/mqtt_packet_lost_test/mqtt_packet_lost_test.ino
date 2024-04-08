#include <M5StickC.h>
#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "xr_phone";
const char* password = "hfhfhfhfhf";
const char* mqtt_server = "192.168.1.10";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

#define DEST_NODE_ID "m5stickc_2" // Topic of the destination M5StickC device
#define TOTAL_MESSAGES 200 // Total number of test messages to send
#define MESSAGE_INTERVAL_MS 1000 // Interval between test messages (in milliseconds)

uint32_t messagesSent = 0;
uint32_t messagesReceived = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  // Increment messagesReceived when a message is received from the destination node
  if (strcmp(topic, DEST_NODE_ID) == 0) {
    messagesReceived++;
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("M5StickCClient")) {
      Serial.println("connected");
      client.subscribe(DEST_NODE_ID);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Send test messages to destination node
  if (messagesSent < TOTAL_MESSAGES) {
    client.publish(DEST_NODE_ID, "TestMessage");
    messagesSent++;

    // Wait for a short interval before sending the next message
    delay(MESSAGE_INTERVAL_MS);
  } else {
    // After sending all test messages, print packet loss rate
    printPacketLossRate();
    
    // Pause execution (optional)
    while (true) {
      delay(1000);
    }
  }
}

void printPacketLossRate() {
  // Calculate and print packet loss rate
  float packetLossRate = (1 - (float)messagesReceived / messagesSent) * 100;
  Serial.print("Packet Loss Rate: ");
  Serial.print(packetLossRate, 2); // Print with 2 decimal places
  Serial.println("%");
}