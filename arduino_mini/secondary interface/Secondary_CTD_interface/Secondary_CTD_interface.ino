#define SS_MAX_RX_BUFF 128
#include <SoftwareSerial.h>

// CTD Commands
#define COM_TEST        0x00
#define COM_VERSION     0x04
#define COM_PCMODE      0x0C
#define COM_ONLINE_MEAS 0x01

// ---- USER CONFIGURATION ----
#define SENSOR_ADDRESS  0     // Change to 1, 2, 3... for each sensor
// ----------------------------

#define OUT_PIN     9
#define EN_PIN      8
#define ACK         0x55
#define PC_MODE_RET 2
#define TIMEOUT_MS  5000
#define CMD_TX_WAIT_MS 150

SoftwareSerial ctdSerial(3, 4);   // RX=3, TX=4 (CTD sensor)
SoftwareSerial cmdSerial(11, 12); // RX=11, TX=12 (controller bus)

String inputBuffer = "";
bool sensorReady = false;

// ---- Helpers ----

String myAddress() {
    if (SENSOR_ADDRESS < 10) return "0" + String(SENSOR_ADDRESS);
    return String(SENSOR_ADDRESS);
}

void cmdTxBegin() {
    digitalWrite(EN_PIN, HIGH);
}

void cmdTxEnd() {
    delay(CMD_TX_WAIT_MS);
    digitalWrite(EN_PIN, LOW);
}

int readByteTimeout() {
    unsigned long start = millis();
    while (!ctdSerial.available()) {
        if (millis() - start > TIMEOUT_MS) return -1;
    }
    return ctdSerial.read();
}

// ---- CTD Protocol ----

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

bool takeMeasurement() {
    uint8_t buf[16];
    int len = 0;

    ctdSerial.write((uint8_t)COM_ONLINE_MEAS);

    int echo = readByteTimeout();
    if (echo != COM_ONLINE_MEAS) {
        Serial.print("ERROR: bad echo 0x");
        Serial.println(echo, HEX);
        return false;
    }

    ctdSerial.write((uint8_t)ACK);

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

    // Send result back with address prefix
    cmdTxBegin();
    cmdSerial.listen();
    cmdSerial.print("*"); cmdSerial.print(myAddress()); cmdSerial.print("*");
    cmdSerial.print("Temp_bin: ");   cmdSerial.print(temp_bin);
    cmdSerial.print("  Pres_bin: "); cmdSerial.print(pressure_bin);
    cmdSerial.print("  Sal_bin: ");  cmdSerial.println(salinity_bin);
    cmdTxEnd();

    return true;
}

// ---- Command Parsing ----

bool checkForCommand() {
    cmdSerial.listen();
    if (cmdSerial.available()) {
        delay(20);
        while (cmdSerial.available()) {
            char c = cmdSerial.read();
            if (c == '\n' || c == '\r') {
                inputBuffer.trim();
                Serial.print("Buffer: '");
                Serial.print(inputBuffer);
                Serial.println("'");

                // Check if message is addressed to us
                // Expected format: *00*MEASURE
                String expectedPrefix = "*" + myAddress() + "*";

                if (inputBuffer.startsWith(expectedPrefix)) {
                    String command = inputBuffer.substring(expectedPrefix.length());
                    if (command == "MEASURE") {
                        inputBuffer = "";
                        return true;
                    } else {
                        // Addressed to us but unknown command
                        // Only send CLARIFY if we are address 00
                        if (SENSOR_ADDRESS == 0) {
                            Serial.println("Unknown command, sending CLARIFY");
                            cmdTxBegin();
                            cmdSerial.println("CLARIFY");
                            cmdTxEnd();
                        }
                        inputBuffer = "";
                    }
                } else if (inputBuffer.length() > 0) {
                    // Message for a different address — stay silent
                    // BUT if garbled (no valid address prefix) and we are 00, send CLARIFY
                    bool hasValidPrefix = inputBuffer.startsWith("*");
                    if (!hasValidPrefix && SENSOR_ADDRESS == 0) {
                        Serial.println("Garbled command, sending CLARIFY");
                        cmdTxBegin();
                        cmdSerial.println("CLARIFY");
                        cmdTxEnd();
                    } else {
                        Serial.println("Message not for us, ignoring");
                    }
                    inputBuffer = "";
                }
            } else {
                inputBuffer += c;
                if (inputBuffer.length() > 30) inputBuffer = "";
            }
        }
    }
    return false;
}

// ---- Setup & Loop ----

void setup() {
    pinMode(EN_PIN, OUTPUT);
    pinMode(OUT_PIN, OUTPUT);
    digitalWrite(EN_PIN, LOW);
    digitalWrite(OUT_PIN, HIGH);

    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
    delay(100);

    Serial.begin(9600);
    ctdSerial.begin(4800);

    Serial.print("Sensor address: ");
    Serial.println(myAddress());

    Serial.println("Waiting for sensor to boot...");
    delay(3000);

    ctdSerial.listen();
    while (ctdSerial.available()) ctdSerial.read();

    Serial.println("Testing connection...");
    if (!testConnection()) {
        Serial.println("ERROR: test_connection failed");
        sensorReady = false;
    } else {
        Serial.println("Connection OK");
        if (!setToPCMode()) {
            Serial.println("ERROR: set_to_pc_mode failed");
            sensorReady = false;
        } else {
            Serial.println("PC mode OK - sensor ready");
            sensorReady = true;
        }
    }

    cmdSerial.begin(4800);
    cmdSerial.listen();
    Serial.print("Secondary ");
    Serial.print(myAddress());
    Serial.println(" ready - waiting for MEASURE command");
}

void loop() {
    cmdSerial.listen();

    if (checkForCommand()) {
        Serial.println("MEASURE received");

        if (!sensorReady) {
            Serial.println("Sensor not ready, sending ERROR");
            cmdTxBegin();
            cmdSerial.print("*"); cmdSerial.print(myAddress()); cmdSerial.print("*");
            cmdSerial.println("ERROR");
            cmdTxEnd();
            return;
        }

        ctdSerial.listen();
        if (!takeMeasurement()) {
            Serial.println("Measurement failed, sending ERROR");
            cmdTxBegin();
            cmdSerial.print("*"); cmdSerial.print(myAddress()); cmdSerial.print("*");
            cmdSerial.println("ERROR");
            cmdTxEnd();
        }
        cmdSerial.listen();
    }
}