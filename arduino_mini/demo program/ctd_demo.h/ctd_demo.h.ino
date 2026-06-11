ifndef SO_CTD_H
#define SO_CTD_H
#include "Arduino.h"
class SO_CTD {
public:
double ctd_c;
double ctd_t;
double ctd_p;
double ctd_d;
double T;
double D;
double C;
uint8_t data[6];
uint8_t connected=false;
uint8_t logger_id;
uint8_t logger_sw_version=0;
HardwareSerial* _ser;
void init();
void end();
uint8_t sample();
void compute(double T, double D, double C);
uint8_t send_command(uint8_t command);
uint8_t set_to_pc_mode();
uint8_t test_connection();
uint8_t get_version(uint8_t *ret);
SO_CTD(HardwareSerial* ser) {
4 _ser = ser;
}
private:
const uint8_t COM_TEST = 0x00;
const uint8_t COM_VERSION = 0x04;
const uint8_t COM_PCMODE = 0x0C;
const uint8_t COM_ONLINE_MEAS = 0x01;
const uint8_t ACK = 0x55;
const uint8_t NAK = 0xAA;
const uint8_t PC_MODE_RET = 2;
const uint8_t TIMEOUT = 2;
};
#endif