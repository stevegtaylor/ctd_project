#define ACK 0x55
#define LED_PIN 13  // Arduino Pro Mini onboard LED

#include <SoftwareSerial.h>


SoftwareSerial ctdSerial(12, 11); // Bit-banging software serial for the CTD interface (rxPin, txPin)

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    ctdSerial.begin(4800);
    Serial.begin(9600);
    Serial.println("Configured hw & sw serial");
    wakeupSensor();
}

void wakeupSensor() {
    Serial.println("Attempting to wake CTD");
    // Poll until sensor responds with 0x33
    for (int i = 0; i < 5; i++) {
        Serial.println("Attempting wake");
        ctdSerial.write((uint8_t)0x00);
        delay(2000);
        if (ctdSerial.available() && ctdSerial.read() == 0x00) {
            Serial.println("Recieved ACK");
            break;
        }
    }
    // Confirm ready
    ctdSerial.write((uint8_t)0x00);
    while (!ctdSerial.available());
    ctdSerial.read(); // expect 0x55

    // Successfully woken — light the LED
    digitalWrite(LED_PIN, HIGH);
    Serial.write("Woke sensor");
}

bool sendCommand(uint8_t cmd, uint8_t* buf, uint8_t& len) {
    ctdSerial.write(cmd);
    // Wait for echo
    while (!ctdSerial.available());
    if (ctdSerial.read() != cmd) return false;
    // Send ACK
    ctdSerial.write(ACK);
    // Read response
    len = 0;
    delay(50);
    while (ctdSerial.available()) {
        buf[len++] = ctdSerial.read();
    }
    return true;
}

void loop(){}