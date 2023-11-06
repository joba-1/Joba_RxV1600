# Control and Monitor Yamaha RX-V1600 AV Receiver via RS232 port

The receiver allows sending commands to its serial interface.
It also sends receiver status updates

With this library text commands like "MainVolume\_Up" can be translated to codes strings the receiver understands. 
E.g. the ascii code string STX, "07A1A", ETX in this case. 
Response codes from the receiver, caused by sending commands or other (IR, buttons, internal) can be translated to readable text.
E.g. the "MainVolume\_Up" would trigger a response like STX, "0026C7", ETX which can be read and translated to "MainVolume" "0 dB" 

Intended usecases are providing a general mqtt gateway and switching receiver input based on bluetooth connection status changes.

See examples/ for how to use...

(c) Joachim Banzhaf, 2023
