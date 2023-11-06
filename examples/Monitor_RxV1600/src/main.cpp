/// Print RX-V1600 configuration and status changes as text

#include <Arduino.h>

#include <rxv1600.h>

RxV1600Comm rxvcomm(Serial1);
RxV1600 rxv;

void recvd( const char *resp, void *ctx ) {
    bool power;
    uint8_t id;
    RxV1600::guard_t guard;
    RxV1600::origin_t origin;
    char text[9];

    if( resp ) {
        if( rxv.decodeConfig(resp, power) ) {
            Serial.printf("Got config while power is %s\n", power ? "on" : "off");
            for( unsigned i=0; i<=0xff; i++) {
                const char *name = rxv.report_name(i);
                if( name ) {
                    const char *value = rxv.report_value_string(i);
                    if( value ) {
                        Serial.printf("Config %s = %s\n", name, value);
                    }
                    else {
                        Serial.printf("Config for %s is invalid\n", name);
                    }
                }
                else {
                    Serial.printf("Config id x%02x is unknown\n", i);
                }
            }
        }
        else if( rxv.decode(resp, id, guard, origin) ) {
            Serial.printf("Guard for report id %u is %s\n", id, guard == RxV1600::G_NONE ? "off" : "on");
            Serial.printf("Report %s = %s\n", rxv.report_name(id), rxv.report_value_string(id));
        }
        else if( rxv.decodeText(resp, id, text) ) {
            Serial.printf("Report %s = %s\n", rxv.report_name(id), text);
        }
        else {
            Serial.printf("Ignoring unknown response '%s'\n", resp);
        }
    }
    else {
        Serial.println("TIMEOUT");
    }
}

void setup() {
    Serial.begin(BAUDRATE);
    Serial.println(HOSTNAME " " __TIMESTAMP__ " starting");
    Serial1.begin(9600, SERIAL_8N1, 16, 17);  // chosen arbitrary rx, tx pins
    rxvcomm.on_recv(recvd, NULL);
    // Send ready to RX-V1600 to receive config
    rxvcomm.send(rxv.command("Ready"));
    Serial.println("Sent Ready message");
}

void loop() {
    rxvcomm.handle();
}