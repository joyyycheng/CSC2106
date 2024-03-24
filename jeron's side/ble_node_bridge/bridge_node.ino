#include <painlessMesh.h>
#include <WiFi.h>
#include <M5StickCPlus.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <ArduinoJson.h>

// some gpio pin that is connected to an LED...
// on my rig, this is 5, change to the right number of your LED.
#ifdef LED_BUILTIN
#define LED LED_BUILTIN
#else
#define LED G10
#endif

#define   BLINK_PERIOD    3000 // milliseconds until cycle repeat
#define   BLINK_DURATION  100  // milliseconds LED is on for

#define   MESH_SSID       "JoyCheng"
#define   MESH_PASSWORD   "heartsensor"
#define   MESH_PORT       5555

#define BLE_SERVER_NAME "TEST_SERVER"
#define BLE_SERVICE_UUID "d76d86d7-39ad-41e6-b38a-9353be6438ce"

// Prototypes
void setupBLE();
void setupMesh();
void sendMessage(); 
void printMessages() ; // Prototype
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
void nodeTimeAdjustedCallback(int32_t offset); 
void delayReceivedCallback(uint32_t from, int32_t delay);

Task taskSendMessage( TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval
Task taskReceivedMessage( TASK_SECOND * 1, TASK_FOREVER, &printMessages ); // start with a one second interval
Scheduler     userScheduler; // to control your personal task

painlessMesh  mesh;
// Data characteristic and descriptor
BLECharacteristic dataCharacteristic("d76d86d7-39ad-41e6-b38a-9353be6438ce", BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY |BLECharacteristic::PROPERTY_WRITE);
BLEDescriptor dataDescriptor(BLEUUID((uint16_t)0x2901));

bool calc_delay = false;
SimpleList<uint32_t> nodes;
SimpleList<String> messageList;

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;
bool deviceConnected = false;
unsigned long lastMillis = 0;
unsigned long timerInterval = 5000;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        Serial.println("BLE Client Connected");
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("BLE Client Disconnected");
    }
};

void setup() {
  Serial.begin(115200);
  M5.begin();
  M5.Lcd.setRotation(3);
  pinMode(LED, OUTPUT);

  setupBLE();
  setupMesh();
  setupTaskA();

}

void loop() {
    mesh.update();
    if (deviceConnected && (millis() - lastMillis > timerInterval)){
        notifyClient();
        lastMillis = millis();
    }
    digitalWrite(LED, !onFlag);
}


void setupBLE() {
    // Initialize BLE
    BLEDevice::init(BLE_SERVER_NAME);
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    BLEService *bleService = pServer->createService(BLE_SERVICE_UUID);

    // Add the data characteristic to the service
    bleService->addCharacteristic(&dataCharacteristic);
    dataDescriptor.setValue("Data to be Transmmitted");
    dataCharacteristic.addDescriptor(&dataDescriptor);

    // Start the service
    bleService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pServer->getAdvertising()->start();
    Serial.println("BLE Server Started, Waiting for connection...");
}


void notifyClient() {
    if (deviceConnected) {
        dataCharacteristic.setValue("TEST DATA");
        dataCharacteristic.notify();
        Serial.println("Sending................");
    }
}

void setupTaskA(){
  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();
  userScheduler.addTask( taskReceivedMessage );
  taskReceivedMessage.enable();

  blinkNoNodes.set(BLINK_PERIOD, (mesh.getNodeList().size() + 1) * 2, []() {
      // If on, switch off, else switch on
      if (onFlag)
        onFlag = false;
      else
        onFlag = true;
      blinkNoNodes.delay(BLINK_DURATION);

      if (blinkNoNodes.isLastIteration()) {
        // Finished blinking. Reset task for next run 
        // blink number of nodes (including this node) times
        blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
        // Calculate delay based on current mesh time and BLINK_PERIOD
        // This results in blinks between nodes being synced
        blinkNoNodes.enableDelayed(BLINK_PERIOD - 
            (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
      }
  });
  userScheduler.addTask(blinkNoNodes);
  blinkNoNodes.enable();

  randomSeed(analogRead(G10));
}

void setupMesh(){
  // mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION | STARTUP );  // set before init() so that you can see error messages
  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);
  // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
  mesh.setRoot(true);
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
  mesh.setContainsRoot(true);
}

// ################################ MESH ################################

// Lab 6 add-ons
int randomizePriority() {
  return random(1, 4);
}

void sendMessage() {
  // StaticJsonDocument<200> jsonDocument;
  // String jsonString;

  // jsonDocument["id"] = mesh.getNodeId();
  // jsonDocument["message"] = "Hello";
  // jsonDocument["priority"] = randomizePriority();

  // serializeJson(jsonDocument, jsonString);

  // mesh.sendBroadcast(jsonString);

  // if (calc_delay) {
  //   SimpleList<uint32_t>::iterator node = nodes.begin();
  //   while (node != nodes.end()) {
  //     mesh.startDelayMeas(*node);
  //     node++;
  //   }
  //   calc_delay = false;
  // }

  // Serial.printf("Sending message: %s\n", jsonString.c_str());
  
  // taskSendMessage.setInterval(TASK_SECOND); // send every 1 second
  // // taskSendMessage.setInterval( random(TASK_SECOND * 1, TASK_SECOND * 5));  // between 1 and 5 seconds
}

void receivedCallback(uint32_t from, String & msg) {
  StaticJsonDocument<200> jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, msg);
  if (error) {
      Serial.print("Parsing failed: ");
      Serial.println(error.c_str());
      return;
  }
  
  uint32_t priority = jsonDocument["priority"].as<uint32_t>();

  SimpleList<String>::iterator it = messageList.begin();
  while (it != messageList.end()) {
      StaticJsonDocument<200> existingMsg;
      DeserializationError existingError = deserializeJson(existingMsg, *it);
      if (existingError) {
          Serial.print("Parsing failed for existing message: ");
          Serial.println(existingError.c_str());
          continue; // Skip this message
      }
      
      uint32_t existingPriority = existingMsg["priority"].as<uint32_t>();
      if (priority < existingPriority) {
          break; // Found the position to insert the message
      }
      ++it;
  }

  messageList.insert(it, msg);
}

void printMessages() {
  Serial.println("Messages received in the last 2 seconds:");
  for (const auto &msg : messageList) {
    StaticJsonDocument<200> jsonDocument;
    DeserializationError error = deserializeJson(jsonDocument, msg);
    if (error) {
        Serial.print("Parsing failed: ");
        Serial.println(error.c_str());
        continue;
    }
    
    uint32_t id = jsonDocument["id"];
    JsonObject message = jsonDocument["message"];

    // Loop through all the properties in the message object
    Serial.printf("Received message from node %u: ", id);
    for (JsonPair property : message) {
      Serial.printf("%s = %f, ", property.key().c_str(), property.value().as<float>());
      String topic = "sensor/" + String(property.key().c_str());
      String payload = String(property.value().as<float>()).c_str();
      // client.publish(topic.c_str(), payload.c_str());
    }
    Serial.println();

    uint32_t priority = jsonDocument["priority"];

    // Format and print the message
    // Serial.printf("Received message from node %u: heart rate = %f, SpO2 = %f, priority = %u\n", id, heartRate, spO2, priority);
  }
  // Clear the message list after printing
  messageList.clear();
  taskReceivedMessage.setInterval(TASK_SECOND * 2); // send every 1 second
}

void newConnectionCallback(uint32_t nodeId) {
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
 
  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
  Serial.printf("--> startHere: New Connection, %s\n", mesh.subConnectionJson(true).c_str());
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD*1000))/1000);
 
  nodes = mesh.getNodeList();

  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");

  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) {
    Serial.printf(" %u", *node);
    node++;
  }
  Serial.println();
  calc_delay = true;
}

void nodeTimeAdjustedCallback(int32_t offset) {
  Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void delayReceivedCallback(uint32_t from, int32_t delay) {
  Serial.printf("Delay to node %u is %d us\n", from, delay);
}