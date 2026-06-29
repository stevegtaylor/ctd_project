#include <SoftwareSerial.h>

#define EN_PIN   8
#define CMD_RX   11
#define CMD_TX   12
#define BYTE_TX_US 2100

SoftwareSerial testBus(CMD_RX, CMD_TX);

void txBegin() {
    digitalWrite(EN_PIN, HIGH); // HIGH = transmit
}

void txEnd() {
    //delayMicroseconds(BYTE_TX_US);
    digitalWrite(EN_PIN, LOW); // LOW = receive
}

void setup() {
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, LOW); // start in receive mode
    Serial.begin(9600);
    testBus.begin(4800);
    Serial.println("Secondary ready");
}

void loop() {
    if (testBus.available()) {
        uint8_t b = testBus.read();
        Serial.print("Received: 0x");
        Serial.println(b, HEX);

        // Send 1 back
        Serial.println("Sending back...");
        txBegin();
        testBus.write((uint8_t)0xaa);
        txEnd();
    }
}