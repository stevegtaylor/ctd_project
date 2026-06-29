#include <SoftwareSerial.h>

#define EN_PIN    8
#define SHARED_RX 11
#define SHARED_TX 12
#define BYTE_TX_US 2100

SoftwareSerial testBus(SHARED_RX, SHARED_TX);

void txBegin() {
    digitalWrite(EN_PIN, HIGH); // HIGH = transmit on your modules
}

void txEnd() {
    //delayMicroseconds(BYTE_TX_US);
    digitalWrite(EN_PIN, LOW); // LOW = receive
}

void setup() {
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, LOW); // start in recieve mode
    Serial.begin(9600);
    testBus.begin(4800);
    Serial.println("Controller ready");
}

void loop() {
    // Send a 1
    Serial.println("Sending 1...");
    txBegin();
    testBus.write((uint8_t)0x01);
    txEnd();

    // Listen for response
    unsigned long start = millis();
    while (millis() - start < 1000) {
        if (testBus.available()) {
            uint8_t b = testBus.read();
            Serial.print("Got back: 0x");
            Serial.println(b, HEX);
            break;
        }
    }
    if (millis() - start >= 1000) {
        Serial.println("No response");
    }

    delay(1000);
}