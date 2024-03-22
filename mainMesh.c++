//************************************************************
// this is a simple example that uses the easyMesh library
//
// 1. blinks led once for every node on the mesh
// 2. blink cycle repeats every BLINK_PERIOD
// 3. sends a silly message to every node on the mesh at a random time between 1 and 5 seconds
// 4. prints anything it receives to Serial.print
//
//
//************************************************************
#include <painlessMesh.h>
#include <WiFi.h>
#include <PubSubClient.h>

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

const char* ssid = "Jeron";
const char* password =  "88888888";

#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT 1883

WiFiClient espClient;
PubSubClient client(espClient);

// Prototypes
void sendMessage(); 
void receivedCallback(uint32_t from, String & msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback(); 
void nodeTimeAdjustedCallback(int32_t offset); 
void delayReceivedCallback(uint32_t from, int32_t delay);


Scheduler     userScheduler; // to control your personal task
painlessMesh  mesh;

bool calc_delay = false;
SimpleList<uint32_t> nodes;
SimpleList<String> messageList;

void sendMessage() ; // Prototype
Task taskSendMessage( TASK_SECOND * 1, TASK_FOREVER, &sendMessage ); // start with a one second interval
void printMessages() ; // Prototype
Task taskReceivedMessage( TASK_SECOND * 1, TASK_FOREVER, &printMessages ); // start with a one second interval

// Task to blink the number of nodes
Task blinkNoNodes;
bool onFlag = false;




void reConnect() {
  while(!client.connected())
  {
    if (client.connect("m5stickcplus")) { //username and password
        Serial.println("Connected to MQTT broker");
        // Subscribe to the heart rate and SpO2 topics
        client.publish("heartsensor/heartrate", "start");
        client.publish("heartsensor/spo2", "start");
    } else {
        Serial.print("Failed to connect to MQTT broker, rc=");
        Serial.print(client.state());
        Serial.println(" Try again in 5 seconds");
        delay(5000);
    }
  }
}



void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for(int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);

  WiFi.begin(ssid, password);

  mesh.setDebugMsgTypes(ERROR | DEBUG);  // set before init() so that you can see error messages

  mesh.init(MESH_SSID, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  mesh.onNodeDelayReceived(&delayReceivedCallback);

    // Connect to MQTT broker
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
  client.connect("m5stickcplus");  //username and password

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

      // Start the MQTT thread
  xTaskCreate(mqttTask, "mqttTask", 4096, NULL, 1, NULL);

}

void loop() {
    mesh.update();
    digitalWrite(LED, !onFlag);
}


void mqttTask(void* pvParameters) {
  for(;;) {
    if (!client.connected()) {
        reConnect();
    }

    // Handle MQTT messages
    client.loop();
  }
}

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
      client.publish(topic.c_str(), payload.c_str());
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