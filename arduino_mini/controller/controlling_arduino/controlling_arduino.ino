#define SS_MAX_RX_BUFF 128
#include <SoftwareSerial.h>

// ---- USER CONFIGURATION ----
#define NUM_SENSORS      2     // Number of sensors on the bus (0 to NUM_SENSORS-1)
#define POLL_INTERVAL_MS 5000
// ----------------------------

#define EN_PIN      8
#define BYTE_TX_US  2100
#define SHARED_RX   11
#define SHARED_TX   12

SoftwareSerial sensorBus(SHARED_RX, SHARED_TX);

struct Measurement {
    bool valid;
    uint16_t temp_bin;
    uint16_t pressure_bin;
    uint16_t salinity_bin;
};

Measurement readings[NUM_SENSORS];

// ---- RS485 Helpers ----

void txBegin() {
    digitalWrite(EN_PIN, HIGH);
}

void txEnd() {
    digitalWrite(EN_PIN, LOW);
}

// ---- Communication ----

String readLineTimeout(unsigned long timeoutMs) {
    String line = "";
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        if (sensorBus.available()) {
            char c = sensorBus.read();
            if (c == '\n') break;
            if (c != '\r') line += c;
        }
    }
    return line;
}

void parseMeasurement(String response, int index) {
    // Expected format: *00*Temp_bin: XXXX  Pres_bin: XXXX  Sal_bin: XXXX
    int tIdx = response.indexOf("Temp_bin: ");
    int pIdx = response.indexOf("Pres_bin: ");
    int sIdx = response.indexOf("Sal_bin: ");

    if (tIdx == -1 || pIdx == -1 || sIdx == -1) {
        Serial.println("  ERROR: malformed measurement");
        readings[index].valid = false;
        return;
    }

    readings[index].temp_bin     = response.substring(tIdx + 10, pIdx).toInt();
    readings[index].pressure_bin = response.substring(pIdx + 10, sIdx).toInt();
    readings[index].salinity_bin = response.substring(sIdx + 9).toInt();
    readings[index].valid = true;
}

String addressString(int index) {
    // Zero pad to 2 digits
    if (index < 10) return "0" + String(index);
    return String(index);
}

void sendMeasureCommand(int index) {
    readings[index].valid = false;
    String addr = addressString(index);
    String cmd = "*" + addr + "*MEASURE";

    int retries = 5;
    while (retries > 0) {
        Serial.print("Sending ");
        Serial.print(cmd);
        Serial.print(" (attempts remaining: ");
        Serial.print(retries);
        Serial.println(")");

        txBegin();
        sensorBus.println(cmd);
        txEnd();

        String response = readLineTimeout(3000);
        response.trim();

        Serial.print("  Response: '");
        Serial.print(response);
        Serial.println("'");

        if (response == "CLARIFY") {
            Serial.println("  Got CLARIFY, retrying...");
            delay(100);
            retries--;
        } else if (response.startsWith("*" + addr + "*Temp_bin")) {
            parseMeasurement(response, index);
            Serial.println("  Measurement OK");
            return;
        } else if (response == "*" + addr + "*ERROR") {
            Serial.println("  Secondary reported CTD error");
            return;
        } else {
            Serial.println("  Unknown response, retrying...");
            delay(100);
            retries--;
        }
    }

    Serial.print("ERROR: sensor ");
    Serial.print(addr);
    Serial.println(" failed after all retries");
}

// ---- Printing ----

void printReadings() {
    Serial.println("========== Readings ==========");
    for (int i = 0; i < NUM_SENSORS; i++) {
        Serial.print("Sensor ");
        Serial.print(addressString(i));
        Serial.print(": ");
        if (readings[i].valid) {
            Serial.print("Temp_bin=");   Serial.print(readings[i].temp_bin);
            Serial.print("  Pres_bin="); Serial.print(readings[i].pressure_bin);
            Serial.print("  Sal_bin=");  Serial.println(readings[i].salinity_bin);
        } else {
            Serial.println("NO DATA");
        }
    }
    Serial.println("==============================");
}

// ---- Setup & Loop ----

void setup() {
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, LOW);

    Serial.begin(9600);
    sensorBus.begin(4800);

    for (int i = 0; i < NUM_SENSORS; i++) {
        readings[i].valid = false;
    }

    Serial.println("Waiting for secondaries to boot...");
    delay(8000);

    Serial.print("Controller ready - managing ");
    Serial.print(NUM_SENSORS);
    Serial.println(" sensor(s)");
}

void loop() {
    unsigned long cycleStart = millis();

    for (int i = 0; i < NUM_SENSORS; i++) {
        sendMeasureCommand(i);
    }

    printReadings();

    unsigned long elapsed = millis() - cycleStart;
    if (elapsed < POLL_INTERVAL_MS) {
        delay(POLL_INTERVAL_MS - elapsed);
    }
}