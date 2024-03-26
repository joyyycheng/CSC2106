#include <painlessMesh.h>
#include <M5StickCPlus.h> // Include M5StickC Plus library
#include <60ghzbreathheart.h>
#include <SoftwareSerial.h>
#include <Wire.h>

#define rxPin 32
#define txPin 33

#ifdef LED_BUILTIN
#define LED LED_BUILTIN
#else
#define LED G10
#endif

#define REPORTING_PERIOD_MS     1000
#define   BLINK_PERIOD    3000 // milliseconds until cycle repeat
#define   BLINK_DURATION  100  // milliseconds LED is on for

#define   MESH_SSID       "JoyCheng"
#define   MESH_PASSWORD   "heartsensor"
#define   MESH_PORT       5555

SoftwareSerial mySerial (rxPin, txPin);
BreathHeart_60GHz radar = BreathHeart_60GHz(&Serial1);
// Prototypes
void sendMessage(); 
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
void nodeTimeAdjustedCallback(int32_t offset); 
void delayReceivedCallback(uint32_t from, int32_t delay);

Scheduler     userScheduler; // to control your personal task
painlessMesh  mesh;

uint32_t tsLastReport = 0;

bool calc_delay = false;
SimpleList<uint32_t> nodes;
SimpleList<String> messageList;
SimpleList<String> receiveMessage;

String heartRateString = "";

void sendMessage() ; // Prototype
Task taskSendMessage( TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval
void printMessages() ; // Prototype
Task taskReceivedMessage( TASK_SECOND * 1, TASK_FOREVER, &printMessages ); // start with a one second interval

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

  while (!Serial);
  Serial.println("123Ready");

  mesh.setDebugMsgTypes(ERROR | DEBUG);  // set before init() so that you can see error messages

  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

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

  // jsonDocument = StaticJsonDocument<200>();
  // array = jsonDocument.to<JsonArray>();

  randomSeed(analogRead(G10));
}

void loop()
{
  mesh.update();
  digitalWrite(LED, !onFlag);

  radar.Breath_Heart();           //Breath and heartbeat information output
  if(radar.sensor_report != 0x00){
    Serial.println("IF LOOP");
    switch(radar.sensor_report){
      Serial.println("SWITCH CASE");
      case HEARTRATEVAL:
        Serial.print("Sensor monitored the current heart rate value is: ");
        Serial.println(radar.heart_rate, DEC);
        Serial.println("----------------------------");
        heartRateString = "[HEARTRATEVAL] Sensor monitored the current heart rate value is: " + String(radar.heart_rate, DEC);
        break;
      case HEARTRATEWAVE:  //Valid only when real-time data transfer mode is on
        Serial.print("The heart rate waveform(Sine wave) -- point 1: ");
        Serial.print(radar.heart_point_1);
        Serial.print(", point 2 : ");
        Serial.print(radar.heart_point_2);
        Serial.print(", point 3 : ");
        Serial.print(radar.heart_point_3);
        Serial.print(", point 4 : ");
        Serial.print(radar.heart_point_4);
        Serial.print(", point 5 : ");
        Serial.println(radar.heart_point_5);
        Serial.println("----------------------------");
        heartRateString = "[HEARTRATEEAVE] The heart rate waveform(Sine wave) -- point 1: " + radar.heart_point_1
        + ", point 2 : " + radar.heart_point_2
        + ", point 3 : " + radar.heart_point_3
        + ", point 4 : " + radar.heart_point_4
        + ", point 5 : " + radar.heart_point_5;
        break;
      case BREATHNOR:
        Serial.println("Sensor detects current breath rate is normal.");
        Serial.println("----------------------------");
        heartRateString = "[BREATHNOR] Sensor detects current breath rate is normal.";
        break;
      case BREATHRAPID:
        Serial.println("Sensor detects current breath rate is too fast.");
        Serial.println("----------------------------");
        heartRateString = "[BREATHRAPID] Sensor detects current breath rate is too fast.";
        break;
      case BREATHSLOW:
        Serial.println("Sensor detects current breath rate is too slow.");
        Serial.println("----------------------------");
        heartRateString = "[BREATHSLOW] Sensor detects current breath rate is too slow.";
        break;
      case BREATHNONE:
        Serial.println("There is no breathing information yet, please wait...");
        Serial.println("----------------------------");
        heartRateString = "[BREATHNONE] There is no breathing information yet, please wait...";
        break;
      case BREATHVAL:
        Serial.print("Sensor monitored the current breath rate value is: ");
        Serial.println(radar.breath_rate, DEC);
        Serial.println("----------------------------");
        heartRateString = "[BREATHVAL] Sensor monitored the current breath rate value is: " + String(radar.breath_rate, DEC);
        break;
      case BREATHWAVE:  //Valid only when real-time data transfer mode is on
        Serial.print("The breath rate waveform(Sine wave) -- point 1: ");
        Serial.print(radar.breath_point_1);
        Serial.print(", point 2 : ");
        Serial.print(radar.breath_point_2);
        Serial.print(", point 3 : ");
        Serial.print(radar.breath_point_3);
        Serial.print(", point 4 : ");
        Serial.print(radar.breath_point_4);
        Serial.print(", point 5 : ");
        Serial.println(radar.breath_point_5);
        Serial.println("----------------------------");
        heartRateString = "[BREATHWAVE] The breath rate waveform(Sine wave) -- point 1: " + radar.heart_point_1
        + ", point 2 : " + radar.heart_point_2
        + ", point 3 : " + radar.heart_point_3
        + ", point 4 : " + radar.heart_point_4
        + ", point 5 : " + radar.heart_point_5;
        break;
    }
  }
  delay(200); //Add time delay to avoid program jam                
}

int randomizePriority() {
  return random(1, 4);
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
  taskReceivedMessage.setInterval(TASK_SECOND * 2); // send every 1 second
}

void sendMessage() {
  // Create a JSON document containing the heart rate and SpO2 values
  StaticJsonDocument<200> jsonDocument;
  String jsonString;
  JsonObject message = jsonDocument.createNestedObject("message");
  message["heartratedetection"] = heartRateString;

  jsonDocument["id"] = mesh.getNodeId();
  jsonDocument["priority"] = randomizePriority();
  // Convert the JSON document to a string
  serializeJson(jsonDocument, jsonString);

  // Send the JSON string
  mesh.sendBroadcast(jsonString);

  // Print the message being sent
  Serial.printf("Sending message: %s\n", jsonString.c_str());

  // Set the interval for sending messages
  taskSendMessage.setInterval(TASK_SECOND); // send every 1 second
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