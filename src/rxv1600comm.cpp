#include "rxv1600comm.h"

#include <Arduino.h>


const uint32_t RxV1600Comm::TIMEOUT_MS = 1000;
const unsigned RxV1600Comm::MAX_TRIES = 5;


RxV1600Comm::RxV1600Comm(Stream &stream) : _stream(stream), _cb(NULL), _cmd(NULL), _pos(0) {
}


bool RxV1600Comm::send(const char *cmd) {
    if( _cmd ) return false;
    _cmd = cmd;
    _pos = 0;
    _tries = 0;
    return true;
}


void RxV1600Comm::on_recv(recv_t cb, void *ctx) {
    _cb = cb;
    _ctx = ctx;
}


void RxV1600Comm::respond( bool valid ) {
    if( !valid ) _resp[_pos] = '\0';
    Serial.printf("DEBUG: recv '%s'\n", _resp);

    _cmd = NULL;  // stop resending current command
    _pos = 0;     // reset response pointer
    if( _cb ) {
        // tell the callback a full response is available or an error occurred
        (*_cb)(valid ? _resp : NULL, _ctx);
    }
}


void RxV1600Comm::handle() {
    while( _stream.available() ) {
        _cmd = NULL;
        // RX-V1600 has sent something
        _resp[_pos] = _stream.read();
        if( _resp[_pos++] == '\x03' ) {
            // this was the last char of a response
            _resp[_pos] = '\0';  // terminate the response string
            respond(true);
        }
        else if( _pos == sizeof(_resp) - 1 ) {
            // discard oversized response
            respond(false);
        }
    }

    if( _cmd ) {
        // command request ongoing
        uint32_t now = millis();
        if( !_tries || now - _sent_ms > TIMEOUT_MS ) {
            // command should be sent
            if( ++_tries > MAX_TRIES ) {
                // too many tries timed out: give up
                respond(false);
            }
            else {
                // start timeout and send the command
                _sent_ms = now;
                _stream.print(_cmd);
                Serial.printf("DEBUG: sent '%s'\n", _cmd);
            }
        }
    }
}


void RxV1600Comm::abort() {
    _tries = MAX_TRIES;
}