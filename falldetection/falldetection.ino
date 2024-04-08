#include <painlessMesh.h>
#include <M5StickCPlus.h>  // Include M5StickC Plus library
#include <60ghzfalldetection.h>
#include <SoftwareSerial.h>
// #include <Wire.h>

#define RX_Pin 32
#define TX_Pin 33

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

SoftwareSerial mySerial(RX_Pin, TX_Pin);
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
uint32_t sensor_report = 0;

bool calc_delay = false;
SimpleList<uint32_t> nodes;
SimpleList<String> messageList;

// Root node id
uint32_t rootNodeID;

StaticJsonDocument<200> doc;

// Retention value for human detection
unsigned long lastDetectionTime = 0;
const unsigned long retentionPeriod = 20000;

String fall_msg = "";
String exist_msg = "";

void sendMessage();                                                       // Prototype
Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage);        // start with a one second interval
void printMessages();                                                     // Prototype
Task taskReceivedMessage(TASK_SECOND * 1, TASK_FOREVER, &printMessages);  // start with a one second interval

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;

void setup() {
  delay(5000);
  Serial.begin(115200);
  mySerial.begin(115200);
  M5.begin();

  pinMode(RX_Pin, INPUT);
  pinMode(TX_Pin, OUTPUT);  
  pinMode(LED, OUTPUT);

  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Fall Detection initiate");
  M5.Lcd.setRotation(3);

  while (!Serial);
  if (mySerial.isListening()) { 
    Serial.println("mySerial is listening!");
  }

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

void human_existent() {
  radar.HumanExis_Func();

  if(radar.sensor_report != 0x00){

    lastDetectionTime = millis();
    switch(radar.sensor_report){
      case NOONE:
        Serial.println("Nobody here.");
        Serial.println("----------------------------");
        exist_msg = "Nobody here.";
        break;
      case SOMEONE:
        Serial.println("Someone is here.");
        Serial.println("----------------------------");
        exist_msg = "Someone is here.";
        break;
      case NONEPSE:
        Serial.println("No human activity messages.");
        Serial.println("----------------------------");
        exist_msg = "No human activity messages.";
        break;
      case STATION:
        Serial.println("Someone stop");
        Serial.println("----------------------------");
        exist_msg = "Someone stop";
        break;
      case MOVE:
        Serial.println("Someone moving");
        Serial.println("----------------------------");
        exist_msg = "Someone moving";
        break;
      case BODYVAL:
        Serial.print("The parameters of human body signs are: ");
        Serial.println(radar.bodysign_val, DEC);
        exist_msg = "The parameters of human body signs are: ";
        // Serial.println(radar.bodysign_val, DEC);
        break;
    }
  } else {
    // If no detection has occurred and the retention period has passed, clear the message
    if (millis() - lastDetectionTime >= retentionPeriod) {
      exist_msg = ""; // Clear the message
    }
  }
}

void loop() {
  static unsigned long lastMeshUpdateTime = 0; // Variable to store the last time mesh.update() was called
  static unsigned long lastFallDetectionTime = 0; // Variable to store the last time fall_detection() was called

  if (!mySerial.isListening()) { 
    Serial.println("mySerial is not listening!");
  }

  // Check if it's time to run fall_detection()
  if (millis() - lastFallDetectionTime >= 1000) {
    // radar.Fall_Detection();           //Receive radar data and start processing
    // sensor_report = radar.sensor_report;
    // fall_detection();

    // radar.reset_func();

    // radar.HumanExis_Func();
    // sensor_report = radar.sensor_report;
    human_existent();
    lastFallDetectionTime = millis(); // Update the last fall_detection() time
  }
  
  // Check if it's time to run mesh.update()
  if (millis() - lastMeshUpdateTime >= 1000) {
    mesh.update();
    digitalWrite(LED, !onFlag);
    lastMeshUpdateTime = millis(); // Update the last mesh.update() time
  }

  // delay(1000); //Add time delay to avoid program jam
}


void receivedCallback(uint32_t from, String& msg) {
  StaticJsonDocument<200> jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, msg);
  if (error) {
    Serial.print("Parsing failed: ");
    Serial.println(error.c_str());
    return;
  }
  // uint32_t priority = jsonDocument["priority"].as<uint32_t>();
  SimpleList<String>::iterator it = messageList.begin();
  while (it != messageList.end()) {
    StaticJsonDocument<200> existingMsg;
    DeserializationError existingError = deserializeJson(existingMsg, *it);
    if (existingError) {
      Serial.print("Parsing failed for existing message: ");
      Serial.println(existingError.c_str());
      continue;  // Skip this message
    }
    // uint32_t existingPriority = existingMsg["priority"].as<uint32_t>();
    // if (priority < existingPriority) {
    //   break;  // Found the position to insert the message
    // }
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

  if(exist_msg != ""){
    // message["fall_detection"] = fall_msg;
    message["human_existent"] = exist_msg;
    jsonDocument["id"] = mesh.getNodeId();

    // Convert the JSON document to a string
    serializeJson(jsonDocument, jsonString);

    // Send the JSON string
    mesh.sendSingle(rootNodeID, jsonString);

    // Print the message being sent
    Serial.printf("Sending message: %s\n", jsonString.c_str());
  }

  if (calc_delay) {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }
  
  // Set the interval for sending messages
  taskSendMessage.setInterval(TASK_SECOND);  // send every 1 second
}

void findRootNode(JsonObject node) {
  // Check if 'root' is true
  if (node["root"] == true) {
    // Get the 'nodeId' and set it to the global variable 'rootNodeID'
    rootNodeID = node["nodeId"];
    Serial.printf("Root node ID: %u\n", rootNodeID);
    return;
  }

  // If 'root' is not true, check the sub-nodes
  JsonArray subs = node["subs"];
  for (JsonObject sub : subs) {
    findRootNode(sub);
  }
}

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

  findRootNode(doc.as<JsonObject>());
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
