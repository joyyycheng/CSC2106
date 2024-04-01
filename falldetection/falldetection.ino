#include <painlessMesh.h>
#include <M5StickCPlus.h>  // Include M5StickC Plus library
#include <60ghzfalldetection.h>
#include <SoftwareSerial.h>
#include <Wire.h>

#define rxPin 32
#define txPin 33

#ifdef LED_BUILTIN
#define LED LED_BUILTIN
#else
#define LED G10
#endif

#define REPORTING_PERIOD_MS 1000
#define BLINK_PERIOD 3000   // milliseconds until cycle repeat
#define BLINK_DURATION 100  // milliseconds LED is on for

#define MESH_SSID "JoyCheng"
#define MESH_PASSWORD "heartsensor"
#define MESH_PORT 5555

SoftwareSerial mySerial(rxPin, txPin);
FallDetection_60GHz radar = FallDetection_60GHz(&mySerial);
// Prototypes
void sendMessage();
void receivedCallback(uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();
void nodeTimeAdjustedCallback(int32_t offset);
void delayReceivedCallback(uint32_t from, int32_t delay);


Scheduler userScheduler;  // to control your personal task
painlessMesh mesh;

uint32_t tsLastReport = 0;

bool calc_delay = false;
SimpleList<uint32_t> nodes;
SimpleList<String> messageList;
SimpleList<String> receiveMessage;

String messageString = "";
uint32_t rootNodeID;

void sendMessage();                                                       // Prototype
Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage);        // start with a one second interval
void printMessages();                                                     // Prototype
Task taskReceivedMessage(TASK_SECOND * 1, TASK_FOREVER, &printMessages);  // start with a one second interval

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;

void setup() {
  M5.begin();
  pinMode(rxPin, INPUT);
  pinMode(txPin, OUTPUT);
  pinMode(LED, OUTPUT);

  Serial.begin(115200);
  // Serial2.begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, bool invert)
  // Serial2.begin(115200, SERIAL_8N1, RX, TX);
  mySerial.begin(115200);

  while (!Serial)
    ;
  Serial.println("123Ready");

  mesh.setDebugMsgTypes(ERROR | DEBUG);  // set before init() so that you can see error messages

  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
  userScheduler.addTask(taskReceivedMessage);
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
      blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);
    }
  });
  userScheduler.addTask(blinkNoNodes);
  blinkNoNodes.enable();

  // jsonDocument = StaticJsonDocument<200>();
  // array = jsonDocument.to<JsonArray>();

  randomSeed(analogRead(G10));
}

void loop() {
  mesh.update();
  digitalWrite(LED, !onFlag);
  // put your main code here, to run repeatedly:
  radar.Fall_Detection();  //Receive radar data and start processing
  if (radar.sensor_report != 0x00) {
    Serial.println("IF LOOP");
    switch (radar.sensor_report) {
      Serial.println("SWITCH CASE");
      case NOFALL:
        Serial.println("The sensor detects this movement is not a fall.");
        Serial.println("----------------------------");
        messageString = "The sensor detects this movement is not a fall.";
        break;
      case FALL:
        Serial.println("The sensor detects a fall.");
        Serial.println("----------------------------");
        messageString = "The sensor detects a fall.";
        break;
      case NORESIDENT:
        Serial.println("The sensors did not detect anyone staying in place.");
        Serial.println("----------------------------");
        messageString = "The sensors did not detect anyone staying in place.";
        break;
      case RESIDENCY:
        Serial.println("The sensor detects someone staying in place.");
        Serial.println("----------------------------");
        messageString = "The sensor detects someone staying in place.";
        break;
    }
  }
  delay(200);  //Add time delay to avoid program jam
}

int randomizePriority() {
  return random(1, 4);
}

void receivedCallback(uint32_t from, String &msg) {
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
      continue;  // Skip this message
    }

      // existingMsg["id"] = mesh.getNodeId();
      // String modifiedMsg;
      // serializeJson(existingMsg, modifiedMsg);
      // mesh.sendBroadcast(modifiedMsg);
      // Serial.printf("Sending modified message: %s\n", modifiedMsg.c_str());
    uint32_t existingPriority = existingMsg["priority"].as<uint32_t>();
    if (priority < existingPriority) {
      break;  // Found the position to insert the message
    }

    existingMsg["id"] = mesh.getNodeId();
    String modifiedMsg;
    serializeJson(existingMsg, modifiedMsg);
    mesh.sendBroadcast(modifiedMsg);
    Serial.printf("Sending modified message: %s\n", modifiedMsg.c_str());

    ++it;
  }

  receiveMessage.insert(it, msg);

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
      Serial.printf("%s = %s, ", property.key().c_str(), property.value().as<String>().c_str());
    }
    Serial.println();

    uint32_t priority = jsonDocument["priority"];

    // Format and print the message
    // Serial.printf("Received message from node %u: heart rate = %f, SpO2 = %f, priority = %u\n", id, heartRate, spO2, priority);
  }
  // Clear the message list after printing
  messageList.clear();
  taskReceivedMessage.setInterval(TASK_SECOND * 2);  // send every 1 second
}

void sendMessage() {
  // Create a JSON document containing the heart rate and SpO2 values
  StaticJsonDocument<200> jsonDocument;
  String jsonString;
  JsonObject message = jsonDocument.createNestedObject("message");
  message["fall_detection"] = messageString;

  jsonDocument["id"] = mesh.getNodeId();
  jsonDocument["priority"] = randomizePriority();

  // Convert the JSON document to a string
  serializeJson(jsonDocument, jsonString);

  Serial.printf("Root Node Id: %d\n", rootNodeID);
  // Send the JSON string
  mesh.sendSingle(rootNodeID, jsonString);

  // Print the message being sent
  Serial.printf("Sending message: %s\n", jsonString.c_str());
  

  // Set the interval for sending messages
  taskSendMessage.setInterval(TASK_SECOND);  // send every 1 second
}

StaticJsonDocument<200> doc;
void newConnectionCallback(uint32_t nodeId) {
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);

  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
  Serial.printf("--> startHere: New Connection, %s\n", mesh.subConnectionJson(true).c_str());

  doc.clear();
  DeserializationError error = deserializeJson(doc, mesh.subConnectionJson(true).c_str());
  if (error) {
    Serial.println("Failed to parse JSON");
    return;
  }
  
  JsonArray subs = doc["subs"];
  for (JsonObject sub : subs) {
    // Check if 'root' is true
    if (sub["root"] == true) {
      // Get the 'nodeId' and set it to the global variable 'rootNodeID'
      rootNodeID = sub["nodeId"];
      Serial.printf("Root node ID: %u\n", rootNodeID);
      break;
    }
  }
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);

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