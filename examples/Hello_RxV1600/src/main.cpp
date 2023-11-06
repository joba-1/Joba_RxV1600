/// Print RX-V1600 configuration and status changes

#include <Arduino.h>

#include <rxv1600comm.h>

RxV1600Comm rxv(Serial1);

void recvd( const char *resp, void *ctx ) {
    static bool first = true;

    if( resp ) {
        Serial.println("Response:");
        Serial.println(resp);
        if( first ) {
            first = false;
            // Start getting messages on RX-V1600 status changes (on by default)
            // rxv.send("\x02" "20000" "\x03");  // Enable report command code: STX, "20000", ETX
        }
    }
    else {
        Serial.println("TIMEOUT");

        // optional
        Serial.println("Rebooting in 10s");
        delay(10000);
        ESP.restart();
    }
}

void setup() {
    Serial.begin(BAUDRATE);
    Serial.println(HOSTNAME " " __TIMESTAMP__ " starting");
    Serial1.begin(9600, SERIAL_8N1, 16, 17);  // chosen arbitrary rx, tx pins
    rxv.on_recv(recvd, NULL);
    // Trigger response with current RX-V1600 config
    rxv.send("\x11" "000" "\x03");  // Ready message: DC1,"000",ETX.
    Serial.println("Sent Ready message");
}

void loop() {
    rxv.handle();
}