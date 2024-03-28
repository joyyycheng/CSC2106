// Include Dependencies
#include <painlessMesh.h>
#include <M5StickCPlus.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

// Call MLX90614 Temperature Sensor Library
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// some gpio pin that is connected to an LED...
// on my rig, this is 5, change to the right number of your LED.
#ifdef LED_BUILTIN
#define LED LED_BUILTIN
#else
#define LED G10
#endif

// Constants
#define BLINK_PERIOD 3000   // milliseconds until cycle repeat
#define BLINK_DURATION 100  // milliseconds LED is on for
#define MESH_SSID "JoyCheng"
#define MESH_PASSWORD "heartsensor"
#define MESH_PORT 5555

// Global Variables
float obj_temp = 0.0;
float amb_temp = 0.0;
bool calc_delay = false;
uint32_t tsLastReport = 0;
Scheduler userScheduler;  // to control your personal task
painlessMesh mesh;
SimpleList<uint32_t> nodes;
SimpleList<String> messageList;

// Prototypes
int randomizePriority();
void receivedCallback(uint32_t from, String& msg);
void printMessages();  // Prototype
void sendMessage();
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();
void nodeTimeAdjustedCallback(int32_t offset);
void delayReceivedCallback(uint32_t from, int32_t delay);
Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage);        // start with a one second interval
Task taskReceivedMessage(TASK_SECOND * 1, TASK_FOREVER, &printMessages);  // start with a one second interval

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;

// Set up M5StickCPlus & Sensor
void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);

  M5.begin();
  Wire.begin();
  mlx.begin();

  M5.Lcd.setTextSize(2);
  M5.Lcd.println("MLX90614 Test");
  M5.Lcd.setRotation(3);

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
  randomSeed(analogRead(G10));
}

void loop() {
  obj_temp = mlx.readObjectTempC();
  amb_temp = mlx.readAmbientTempC();
  mesh.update();
  digitalWrite(LED, !onFlag);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0, 2);
  M5.Lcd.printf("Obj Temp: %5.2f C", obj_temp);
  M5.Lcd.setCursor(0, 40, 2);
  M5.Lcd.printf("Amb Temp: %5.2f C", amb_temp);
  delay(1000);
}

// Lab 6 add-ons
int randomizePriority() {
  return random(1, 4);
}

void receivedCallback(uint32_t from, String& msg) {
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
    uint32_t existingPriority = existingMsg["priority"].as<uint32_t>();
    if (priority < existingPriority) {
      break;  // Found the position to insert the message
    }
    ++it;
  }
  messageList.insert(it, msg);
}

void printMessages() {
  Serial.println("Messages received in the last 2 seconds:");
  for (const auto& msg : messageList) {
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
  message["amb_temp"] = amb_temp;
  message["obj_temp"] = obj_temp;
  jsonDocument["id"] = mesh.getNodeId();
  jsonDocument["priority"] = randomizePriority();

  // Convert the JSON document to a string
  serializeJson(jsonDocument, jsonString);

  mesh.sendBroadcast(jsonString);

  if (calc_delay) {
    SimpleList<uint32_t>::iterator node = nodes.begin();
    while (node != nodes.end()) {
      mesh.startDelayMeas(*node);
      node++;
    }
    calc_delay = false;
  }
  Serial.printf("Sending message: %s\n", jsonString.c_str());
  taskSendMessage.setInterval(TASK_SECOND);  // send every 1 second
}

void newConnectionCallback(uint32_t nodeId) {
  // Reset blink task
  onFlag = false;
  blinkNoNodes.setIterations((mesh.getNodeList().size() + 1) * 2);
  blinkNoNodes.enableDelayed(BLINK_PERIOD - (mesh.getNodeTime() % (BLINK_PERIOD * 1000)) / 1000);
  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
  Serial.printf("--> startHere: New Connection, %s\n", mesh.subConnectionJson(true).c_str());
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
