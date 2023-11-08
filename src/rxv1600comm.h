#pragma once

#include <Stream.h>

/// Class to send commands to an RX-V1600 and receive its messages
/// Each sent command triggers at least one callback.
/// Since the RX-V1600 sends two responses for some commands and messages on status changes
/// there is no strict 1:1 correlation between send and callback
class RxV1600Comm {
    public:

    /// @brief type of function called when a complete response is received
    /// @param resp complete response received, NULL on error
    /// @param ctx context as given when the callback was registered
    typedef void (* recv_t)(const char *resp, void *ctx);

    static const uint32_t TIMEOUT_MS;  // how long until giving up on receiving a full response
    static const unsigned MAX_TRIES;   // how many times to retry sending a command

    /// @brief handle communication with an RX-V1600 via serial connection
    /// @param stream serial port connected to the RX-V1600.
    ///        Initialize to 9600 baud 8N1 before calling handle()
    RxV1600Comm(Stream &stream);

    /// @brief trigger sending a command to the receiver during next handle()
    /// @param cmd the full command string to send
    /// @return true if no other command is still active
    bool send(const char *cmd);

    /// @brief register a function that is called once a request is done
    /// @param cb the callback function
    /// @param ctx context to hand over to the callback
    void on_recv(recv_t cb, void *ctx);

    /// @brief check if a response comes in and if a timeout occurred to resend a command or give up
    /// If a response is fully received or a sent command took too long the registered callback is called
    void handle();

    /// @brief stop retrying to send a command
    /// A sent request cannot be aborted, so if a send is active, there will still be a callback.
    /// Either on timeout or on getting the full response
    void abort();

    private:

    void respond( bool valid );  // invoke callback and prepare for receiving the next response

    Stream &_stream;
    recv_t _cb;
    const char *_cmd;   // command to send
    size_t _pos;        // received chars
    unsigned _tries;    // number of send tries
    uint32_t _sent_ms;  // start of current try
    void *_ctx;         // context by/for the callback implementor
    char _resp[268];    // length of full config response (157) probably enough
};
