#include <painlessMesh.h>

painlessMesh mesh;

#define DEST_NODE_ID 2 // Node ID of the destination M5StickC device
#define TOTAL_PACKETS 200 // Total number of test packets to send
#define PACKET_INTERVAL_MS 1000 // Interval between test packets (in milliseconds)

uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;

void setup() {
    Serial.begin(115200);
    mesh.setDebugMsgTypes(ERROR | DEBUG | CONNECTION);
    mesh.init("MeshNetwork", "password", 5555);
    mesh.onReceive(&receivedCallback);
    mesh.start();
}

void loop() {
    // Send test packets to destination node
    if (packetsSent < TOTAL_PACKETS) {
        mesh.sendSingle(DEST_NODE_ID, "TestPacket");
        packetsSent++;

        // Wait for a short interval before sending the next packet
        delay(PACKET_INTERVAL_MS);
    } else {
        // After sending all test packets, print packet loss rate
        printPacketLossRate();
        
        // Pause execution (optional)
        while (true) {
            delay(1000);
        }
    }
}

void receivedCallback(uint32_t from, String &msg) {
    // Increment packetsReceived when a packet is received from the destination node
    if (from == DEST_NODE_ID && msg == "TestPacket") {
        packetsReceived++;
    }
}

void printPacketLossRate() {
    // Calculate and print packet loss rate
    float packetLossRate = (1 - (float)packetsReceived / packetsSent) * 100;
    Serial.print("Packet Loss Rate: ");
    Serial.print(packetLossRate, 2); // Print with 2 decimal places
    Serial.println("%");
}