#include <Arduino.h>

#include <rxv1600comm.h>

/// Print RX-V1600 configuration and status changes

RxV1600Comm rxv(Serial);

void recvd( const char *resp, void *ctx ) {
    static bool first = true;

    if( resp ) {
        Serial.println(resp);
        if( first ) {
            first = false;
            // Start getting messages on RX-V1600 status changes
            rxv.send("\0x0220000\0x03");  // Enable report command code: STX, "20000", ETX
        }
    }
    else {
        Serial.println("TIMEOUT");
    }
}

void setup() {
    Serial.begin(BAUDRATE);
    Serial1.begin(9600, SERIAL_8N1, 16, 17);  // chosen arbitrary rx, tx pins
    rxv.on_recv(recvd, NULL);
    // Trigger response with current RX-V1600 config
    rxv.send("\0x11000\0x03");  // Ready message: DC1,"000",ETX.
}

void loop() {
    rxv.handle();
}