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
    const char *name;
    const char *value;
    char buf[10];

    if( resp ) {
        if( rxv.decodeConfig(resp, power) ) {
            Serial.printf("Got config while power is %s\n", power ? "on" : "off");
            for( unsigned i=0; i<=0xff; i++) {
                name = rxv.report_name(i);
                value = rxv.report_value_string(i);
                if( name || value ) {
                    Serial.printf("Config x%02X: %s = %s\n", i, name ? name : "invalid", value ? value : "invalid");
                }
            }
        }
        else if( rxv.decode(resp, id, guard, origin) ) {
            if( guard != RxV1600::G_NONE ) {
                Serial.printf("Guard for report x%02X is %s\n", id, guard == RxV1600::G_SETTINGS ? "Settings" : "System");
            }
            name = rxv.report_name(id);
            value = rxv.report_value_string(id);
            if( !value && (id == 0x26 || id == 0x27 || id == 0xa2) ) {
                snprintf(buf, sizeof(buf), "raw %u", rxv.report_value(id));
                value = buf;
            }
            Serial.printf("Report x%02X: %s = %s\n", id, name ? name : "invalid", value ? value : "invalid");
        }
        else if( rxv.decodeText(resp, id, text) ) {
            name = rxv.report_name(id);
            Serial.printf("Report x%02X: %s = %s\n", id, name ? name : "invalid", text);
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