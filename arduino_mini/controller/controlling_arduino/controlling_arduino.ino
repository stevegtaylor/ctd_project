#define SS_MAX_RX_BUFF 128
#include <SoftwareSerial.h>

// ---- USER CONFIGURATION ----
#define NUM_SENSORS      1     // Change this to 1-9
#define POLL_INTERVAL_MS 5000
// ----------------------------

// Demux pins (only used if NUM_SENSORS > 1)
#define DEMUX_A0 5
#define DEMUX_A1 6
#define DEMUX_A2 7

// Shared serial bus to demux or direct to secondary
#define SHARED_RX 12
#define SHARED_TX 11

SoftwareSerial sensorBus(SHARED_RX, SHARED_TX);

struct Measurement {
    bool valid;
    uint16_t temp_bin;
    uint16_t pressure_bin;
    uint16_t salinity_bin;
};

Measurement readings[NUM_SENSORS];

// ---- Demux ----

void selectSensor(int index) {
#if NUM_SENSORS > 1
    digitalWrite(DEMUX_A0, (index >> 0) & 1);
    digitalWrite(DEMUX_A1, (index >> 1) & 1);
    digitalWrite(DEMUX_A2, (index >> 2) & 1);
    delay(10);
    Serial.print("Selecting sensor ");
    Serial.println(index);
#endif
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

void sendMeasureCommand(int index) {
    selectSensor(index);
    readings[index].valid = false;

    int retries = 5;
    while (retries > 0) {
        Serial.print("Sending MEASURE to sensor ");
        Serial.print(index);
        Serial.print(" (attempts remaining: ");
        Serial.print(retries);
        Serial.println(")");

        sensorBus.println("*MEASURE");

        String response = readLineTimeout(3000);
        response.trim();

        Serial.print("  Response: '");
        Serial.print(response);
        Serial.println("'");

        if (response == "CLARIFY") {
            Serial.println("  Got CLARIFY, retrying...");
            delay(100);
            retries--;
        } else if (response.startsWith("Temp_bin")) {
            parseMeasurement(response, index);
            Serial.println("  Measurement OK");
            return;
        } else if (response == "ERROR") {
            Serial.println("  Secondary reported CTD error");
            return;
        } else {
            // Unknown or empty response
            Serial.println("  Unknown response, retrying...");
            delay(100);
            retries--;
        }
    }

    Serial.print("ERROR: sensor ");
    Serial.print(index);
    Serial.println(" failed after all retries");
}

// ---- Printing ----

void printReadings() {
    Serial.println("========== Readings ==========");
    for (int i = 0; i < NUM_SENSORS; i++) {
        Serial.print("Sensor ");
        Serial.print(i);
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
    Serial.begin(9600);
    sensorBus.begin(4800);

#if NUM_SENSORS > 1
    pinMode(DEMUX_A0, OUTPUT);
    pinMode(DEMUX_A1, OUTPUT);
    pinMode(DEMUX_A2, OUTPUT);
    digitalWrite(DEMUX_A0, LOW);
    digitalWrite(DEMUX_A1, LOW);
    digitalWrite(DEMUX_A2, LOW);
    Serial.println("Demux enabled");
#else
    Serial.println("Single sensor mode - demux disabled");
#endif

    for (int i = 0; i < NUM_SENSORS; i++) {
        readings[i].valid = false;
    }

    Serial.println("Waiting for secondaries to boot...");
    delay(3000);

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