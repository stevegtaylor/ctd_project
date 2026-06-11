#include <SoftwareSerial.h>

// Commands
#define COM_TEST        0x00
#define COM_VERSION     0x04
#define COM_PCMODE      0x0C
#define COM_ONLINE_MEAS 0x01

// Protocol constants
#define ACK             0x55
#define NAK             0x33
#define PC_MODE_RET     2
#define LED_PIN         13
#define EN_PIN          8
#define TIMEOUT_MS      2000

SoftwareSerial ctdSerial(12, 11); // RX=12, TX=11

// ---- Helpers ----

int readByteTimeout() {
    unsigned long start = millis();
    while (!ctdSerial.available()) {
        if (millis() - start > TIMEOUT_MS) return -1; // timeout
    }
    return ctdSerial.read();
}

// ---- Protocol functions ----

bool testConnection() {
    ctdSerial.write((uint8_t)COM_TEST);
    
    int b0 = readByteTimeout();
    int b1 = readByteTimeout();

    Serial.print("  test_connection raw bytes: 0x");
    Serial.print(b0, HEX);
    Serial.print(" 0x");
    Serial.println(b1, HEX);

    return (b0 == COM_TEST && b1 == ACK);
}

bool setToPCMode() {
    ctdSerial.write((uint8_t)COM_PCMODE);

    int b0 = readByteTimeout();
    int b1 = readByteTimeout();

    Serial.print("  set_to_pc_mode raw bytes: 0x");
    Serial.print(b0, HEX);
    Serial.print(" 0x");
    Serial.println(b1, HEX);

    return (b0 == COM_PCMODE && b1 == PC_MODE_RET);
}

int sendCommand(uint8_t cmd, uint8_t* buf) {
    ctdSerial.write((uint8_t)cmd);
    int echo = readByteTimeout();
    if (echo != cmd) {
        Serial.print("  bad echo: 0x");
        Serial.println(echo, HEX);
        return -1;
    }

    ctdSerial.write((uint8_t)ACK);

    int len = 0;
    int b = readByteTimeout();
    if (b == -1) return -1;
    buf[len++] = (uint8_t)b;

    delay(250);
    while (ctdSerial.available()) {
        buf[len++] = ctdSerial.read();
    }
    return len;
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.begin(9600);
    ctdSerial.begin(4800);
    delay(500);
    digitalWrite(EN_PIN, HIGH);

    // Discard any wake bytes
    while (ctdSerial.available()) ctdSerial.read();

    Serial.println("Testing connection...");
    if (!testConnection()) {
        Serial.println("ERROR: test_connection failed");
        return;
    }
    Serial.println("Connection OK");

    Serial.println("Setting PC mode...");
    if (!setToPCMode()) {
        Serial.println("ERROR: set_to_pc_mode failed");
        return;
    }
    Serial.println("PC mode OK");
    digitalWrite(LED_PIN, HIGH);

    uint8_t buf[16];
    int len = sendCommand(COM_VERSION, buf);
    if (len < 2) {
        Serial.println("ERROR: version read failed");
        return;
    }
    Serial.print("Logger type: ");    Serial.println(buf[0]);
    Serial.print("Logger version: "); Serial.println(buf[1]);
}

void loop() {
    uint8_t buf[16];
    int len = sendCommand(COM_ONLINE_MEAS, buf);

    if (len < 6) {
        Serial.println("ERROR: measurement failed");
        delay(1000);
        return;
    }

    uint16_t temp_bin     = buf[0] | (buf[1] << 8);
    uint16_t pressure_bin = buf[2] | (buf[3] << 8);
    uint16_t salinity_bin = buf[4] | (buf[5] << 8);

    Serial.print("Temp_bin: ");   Serial.print(temp_bin);
    Serial.print("  Pres_bin: "); Serial.print(pressure_bin);
    Serial.print("  Sal_bin: ");  Serial.println(salinity_bin);

    delay(1000);
}
