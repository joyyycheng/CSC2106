#include <M5StickCPlus.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <ArduinoJson.h>

#define WIFI_SSID         "NG_FAMILY_2.4"
#define WIFI_PASSWORD     "S81618778"

#define MQTT_BROKER "pi.local"
#define MQTT_PORT 1883

#define BLE_SERVER_NAME "TEST_SERVER"
#define BLE_SERVICE_UUID "d76d86d7-39ad-41e6-b38a-9353be6438ce"

#define LED_PIN 10

// Prototypes
void setupWifi();
void reConnectMqtt();
void setupBLE();

WiFiClient espClient;
PubSubClient client(espClient);

BLEAddress *pServerAddress;
BLERemoteCharacteristic* dataCharacteristic;

// Data characteristic and descriptor
static BLEUUID dataCharacteristicUUID("d76d86d7-39ad-41e6-b38a-9353be6438ce");

bool ledStatus = false;
// BLE connection
bool doConnect = false;
bool connected = false;
bool newData = false;

// // BLE notify VARIABLE RELATED TO CRASH
// const uint8_t notificationOn[] = {0x01, 0x00};
// const uint8_t notificationOff[] = {0x00, 0x00};

// variable to store the BLE data
char bleData[20];

class AdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.getName() == BLE_SERVER_NAME) {
            BLEDevice::getScan()->stop();
            pServerAddress = new BLEAddress(advertisedDevice.getAddress());
            doConnect = true;
            Serial.println("Device found. Connecting to the BLE Server");
        }
    }
};

void setup() {
  Serial.begin(115200);
  M5.begin();
  M5.Lcd.setRotation(3);
  pinMode(M5_BUTTON_HOME, INPUT);
  pinMode(LED_PIN, OUTPUT);

  setupWifi();
  setupBLE();

  // Start the MQTT thread
  xTaskCreate(mqttTask, "mqttTask", 4096, NULL, 1, NULL);
}

void loop() {
  if (digitalRead(M5_BUTTON_HOME) == LOW) {
        toggleLED();
        while(digitalRead(M5_BUTTON_HOME) == LOW); 
  }
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)){
        Serial.println("Connected to the BLE Server");
        // CRASH HERE WHEN UNCOMMENTED
        // dataCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
        connected = true;
    } else {
        Serial.println("Failed to connect to the BLE Server");
    }
    doConnect = false;
  }
  if (newData) {
    Serial.println("New data received\n");
    Serial.println(bleData);
    newData = false;
    // Publish the data to the MQTT broker
    client.publish("presenceModule/data", bleData);
  }
  delay(1000);
}

void setupBLE() {
    BLEDevice::init(BLE_SERVER_NAME);
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->start(30);
}

void setupWifi() {
    delay(10);
    M5.Lcd.printf("Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        M5.Lcd.print(".");
    }
    M5.Lcd.printf("\nSuccess\n");
}

bool connectToServer(BLEAddress pAddress) {
    BLEClient*  pClient  = BLEDevice::createClient();
    
    // Connect to the remove BLE Server.
    pClient->connect(pAddress);
    Serial.println(" - Connected to the BLE Server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(BLE_SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(BLE_SERVICE_UUID);
        return (false);
    }

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(dataCharacteristicUUID);

    if (pRemoteCharacteristic == nullptr) {
        Serial.print("Failed to find our characteristic UUID");
        return (false);
    }
    Serial.println(" - Found our characteristic");

    // Assign the notify callback to the characteristic
    pRemoteCharacteristic->registerForNotify(dataNotifyCallback);

    return true;
}


void mqttTask(void* pvParameters) {
  // Connect to MQTT broker
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(mqtt_callback);
  M5.Lcd.print("Attempting MQTT connection...");
  if (client.connect("m5stickcplus")){
    M5.Lcd.printf("\nSuccess\n");
  } 

  for(;;) {
    if (!client.connected()) {
        reConnectMqtt();
    }

    // Handle MQTT messages
    client.loop();
  }
}

void reConnectMqtt() {
  while(!client.connected()){
    M5.Lcd.print("Attempting MQTT connection...");
    if (client.connect("m5stickcplus")) {
        M5.Lcd.printf("\nSuccess\n");
        Serial.println("Connected to MQTT broker");
        // Subscribe to the heart rate and SpO2 topics
        client.publish("heartsensor/heartrate", "start");
        client.publish("heartsensor/spo2", "start");
    } else {
        M5.Lcd.printf("\nFailed\n");
        Serial.print("Failed to connect to MQTT broker, rc=");
        Serial.print(client.state());
        Serial.println(" Try again in 5 seconds");
        delay(5000);
    }
  }
}


void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for(int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

static void dataNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    std::string readingsValue(reinterpret_cast<char*>(pData), length);
    strncpy(bleData, readingsValue.c_str(), sizeof(bleData) - 1);
    bleData[sizeof(bleData)-1] = '\0';
    newData = true;
}

void toggleLED(){
    ledStatus = !ledStatus;
    digitalWrite(LED_PIN, ledStatus ? LOW : HIGH); // Toggle LED
    client.publish("presenceModule/data", ledStatus ? "true" : "false"); // Publish LED status as presence
}