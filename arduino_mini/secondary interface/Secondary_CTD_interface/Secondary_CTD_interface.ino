#define SS_MAX_RX_BUFF 128
#include <SoftwareSerial.h>

// CTD Commands
#define COM_TEST        0x00
#define COM_VERSION     0x04
#define COM_PCMODE      0x0C
#define COM_ONLINE_MEAS 0x01

// Protocol constants
#define ACK          0x55
#define PC_MODE_RET  2
#define EN_PIN       8
#define TIMEOUT_MS   2000
#define BYTE_TX_US   2100

SoftwareSerial ctdSerial(12, 11); // RX=12, TX=11 (CTD sensor)
SoftwareSerial cmdSerial(4, 3);   // RX=4,  TX=3  (controller)

String inputBuffer = "";

// ---- RS485 Helpers ----

void txBegin() {
    digitalWrite(EN_PIN, HIGH);
}

void txEnd() {
    delayMicroseconds(BYTE_TX_US);
    digitalWrite(EN_PIN, LOW);
}

// ---- CTD Helpers ----

int readByteTimeout() {
    unsigned long start = millis();
    while (!ctdSerial.available()) {
        if (millis() - start > TIMEOUT_MS) return -1;
    }
    return ctdSerial.read();
}

// ---- CTD Protocol ----

bool wakeAndConnect() {
    pinMode(11, OUTPUT);
    digitalWrite(11, HIGH);
    delay(500);

    ctdSerial.listen();
    while (ctdSerial.available()) ctdSerial.read();

    // Test connection
    txBegin();
    ctdSerial.write((uint8_t)COM_TEST);
    txEnd();

    int b0 = readByteTimeout();
    int b1 = readByteTimeout();
    if (b0 != COM_TEST || b1 != ACK) {
        Serial.println("ERROR: test_connection failed");
        return false;
    }

    // Set PC mode
    txBegin();
    ctdSerial.write((uint8_t)COM_PCMODE);
    txEnd();

    b0 = readByteTimeout();
    b1 = readByteTimeout();
    if (b0 != COM_PCMODE || b1 != PC_MODE_RET) {
        Serial.println("ERROR: set_to_pc_mode failed");
        return false;
    }

    return true;
}

bool takeMeasurement() {
    uint8_t buf[16];
    int len = 0;

    txBegin();
    ctdSerial.write((uint8_t)COM_ONLINE_MEAS);
    txEnd();

    int echo = readByteTimeout();
    if (echo != COM_ONLINE_MEAS) {
        Serial.print("ERROR: bad echo 0x");
        Serial.println(echo, HEX);
        return false;
    }

    txBegin();
    ctdSerial.write((uint8_t)ACK);
    txEnd();

    int b = readByteTimeout();
    if (b == -1) return false;
    buf[len++] = (uint8_t)b;

    delay(250);
    while (ctdSerial.available()) {
        buf[len++] = ctdSerial.read();
    }

    if (len < 6) {
        Serial.println("ERROR: short response");
        return false;
    }

    uint16_t temp_bin     = buf[0] | (buf[1] << 8);
    uint16_t pressure_bin = buf[2] | (buf[3] << 8);
    uint16_t salinity_bin = 4096 - (buf[4] | (buf[5] << 8));

    Serial.print("Temp_bin: ");   Serial.print(temp_bin);
    Serial.print("  Pres_bin: "); Serial.print(pressure_bin);
    Serial.print("  Sal_bin: ");  Serial.println(salinity_bin);

    // Send result to controller
    cmdSerial.listen();
    cmdSerial.print("Temp_bin: ");   cmdSerial.print(temp_bin);
    cmdSerial.print("  Pres_bin: "); cmdSerial.print(pressure_bin);
    cmdSerial.print("  Sal_bin: ");  cmdSerial.println(salinity_bin);
    cmdSerial.listen();

    return true;
}

// ---- Command Parsing ----

bool checkForCommand() {
    cmdSerial.listen();
    if (cmdSerial.available()) {
        delay(20); // wait for full message to arrive
        while (cmdSerial.available()) {
            char c = cmdSerial.read();
            if (c == '\n' || c == '\r') {
                inputBuffer.trim();
                Serial.print("Buffer: '");
                Serial.print(inputBuffer);
                Serial.println("'");

                if (inputBuffer.endsWith("MEASURE")) {
                    inputBuffer = "";
                    return true;
                } else if (inputBuffer.length() > 0) {
                    // Got something unrecognised — send CLARIFY
                    Serial.println("Unknown command, sending CLARIFY");
                    cmdSerial.println("CLARIFY");
                    inputBuffer = "";
                }
            } else {
                inputBuffer += c;
                if (inputBuffer.length() > 20) inputBuffer = "";
            }
        }
    }
    return false;
}

// ---- Setup & Loop ----

void setup() {
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, LOW);

    Serial.begin(9600);
    ctdSerial.begin(4800);
    cmdSerial.begin(4800);
    cmdSerial.listen();

    Serial.println("Secondary ready - waiting for MEASURE command");
}

void loop() {
    cmdSerial.listen();

    if (checkForCommand()) {
        Serial.println("MEASURE received, waking CTD...");
        ctdSerial.listen();
        if (wakeAndConnect()) {
            takeMeasurement();
        } else {
            // Failed to connect to CTD, notify controller
            cmdSerial.listen();
            cmdSerial.println("ERROR");
        }
        cmdSerial.listen();
    }
}