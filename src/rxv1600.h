#pragma once

// Serial protocol for encoding and decoding commands and responses 
// sent to and received from a Yamaha RX-V1600 AV Receiver.
// Based on RX-V1600_V2600_RS232C_ST.pdf
// Joachim Banzhaf, 2023

// Todo
// - Test (once I have the serial connector)
// - Mqtt gateway on an ESP or PicoW
// - Fill in some gaps as needed (left out some commands that are not important to me)
//   - Use checksum sent with response on Ready command
//   - Speaker setup, tuner setup, most OSD features, subwoofer, zone3, ?
//   - Extend to RX-V2600 (for now additional features won't work)
//   - Extend to US and Japanese variants (for now additional features won't work: XM, dual mono, ?)

#include <rxv1600comm.h>

class RxV1600 {
    public:

    static const char UNKNOWN_VALUE = '\0xff';

    typedef enum guard { G_NONE, G_SYSTEM, G_SETTINGS, G_UNKNOWN=UNKNOWN_VALUE } guard_t;
    typedef enum origin { O_RS232C, O_IR, O_PANEL, O_SYSTEM, O_ENCODER, O_UNKNOWN=UNKNOWN_VALUE } origin_t;

    RxV1600();

    /// @brief get full command bytes to send for given command name
    /// @param name camel cased command name from spec
    /// @return ascii string to send to the RX-V1600 or NULL if name unknown
    static const char *command(const char *name);

    /// @brief get full command bytes to send for given command name with value
    /// @param name camel cased command name from spec
    /// @return ascii string to send to the RX-V1600 or NULL if name unknown
    ///         uses internal buffer, invalidated on next call.
    static const char *command_value(const char *name, uint8_t value);

    /// @brief get camel cased report name from report id (rcmd0,1)
    /// @param id binary value, i.e. rcmd0,1 = '1','A' -> id = 26
    /// @return camel cased name of the report from spec or null if id not known
    static const char *report_name(uint8_t id);


    /// @brief get last report value of report id
    /// @param id binary value, i.e. rcmd0,1 = '1','A' -> id = 26
    /// @return value of the report from spec or UNKNOWN_VALUE if id or value not known
    uint8_t report_value(uint8_t id);

    /// @brief get string representation of last value of report id
    /// @param id binary value, i.e. rcmd0,1 = '1','A' -> id = 26
    /// @return value of the report from spec or NULL if id or value not known
    const char *report_value_string(uint8_t id);


    /// @brief decode and store command or system report (starts with STX)
    /// @param resp complete command string as received from RX-V1600
    /// @param id report id
    /// @param guard value from from RX-V1600 (changeable or protected)
    /// @param origin value from from RX-V1600 (RS232, IR, ...)
    /// @return true if report was valid and not guarded
    bool decode( const char *resp, uint8_t &id, guard_t &guard, origin_t &origin );

    /// @brief decode display text report (starts with DC1)
    /// @param resp complete command string as received from RX-V1600
    /// @param id text type (volumes, inputs, ...)
    /// @param text pointer to at least 9 bytes (8 chars + EOS)
    ///             receives right justified report text
    /// @return true if report was valid
    bool decodeText( const char *resp, uint8_t &id, char *text );

    /// @brief decode display text report (starts with DC1)
    /// @param resp complete command string as received from RX-V1600
    /// @param power true if on (an values beyond DT9 are initialized)
    /// @return true if report was valid
    bool decodeConfig( const char *resp, bool &power );


    private:

    uint8_t _status[256];  // cached report states of the RX-V1600
};
