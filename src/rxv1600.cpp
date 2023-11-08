#include <rxv1600.h>

#include <map>
#include <string.h>


// ASCII control characters used by the protocol
#define STX "\x02"
#define ETX "\x03"
#define DC1 "\x11"
#define DC2 "\x12"
#define DC3 "\x13"
#define DEL "\x7F"


static RxV1600::cmds_t CMDS = {
    // Init communication and request current configuration
    { "Ready",                 DC1 "000" ETX },

    // Reset config to factory defaults
    { "ResetConfig",           DC3 DEL DEL DEL ETX },

    // Operation commands (same as IR commands)
    { "MainVolume_Up",         STX "07A1A" ETX },
    { "MainVolume_Down",       STX "07A1B" ETX },

    { "Mute_On",               STX "07EA2" ETX },
    { "Mute_20dB",             STX "07EDF" ETX },
    { "Mute_Off",              STX "07EA3" ETX },

    { "Input_Phono",           STX "07A14" ETX },
    { "Input_Cd",              STX "07A15" ETX },
    { "Input_Tuner",           STX "07A16" ETX },
    { "Input_CD-R",            STX "07A19" ETX },
    { "Input_MD-Tape",         STX "07A18" ETX },
    { "Input_Dvd",             STX "07AC1" ETX },
    { "Input_Dtv",             STX "07A54" ETX },
    { "Input_Cbl-Sat",         STX "07AC0" ETX },
    { "Input_Vcr1",            STX "07A0F" ETX },
    { "Input_Dvr-Vcr2",        STX "07A13" ETX },
    { "Input_V-Aux",           STX "07A55" ETX },

    { "Zone2Volume_Up",        STX "07ADA" ETX },
    { "Zone2Volume_Down",      STX "07ADB" ETX },

    { "Zone2Mute_On",          STX "07EA0" ETX },
    { "Zone2Mute_Off",         STX "07EA1" ETX },

    { "Zone2Input_Phono",      STX "07AD0" ETX },
    { "Zone2Input_Cd",         STX "07AD1" ETX },
    { "Zone2Input_Tuner",      STX "07AD2" ETX },
    { "Zone2Input_CD-R",       STX "07AD4" ETX },
    { "Zone2Input_MD-Tape",    STX "07AD3" ETX },
    { "Zone2Input_Dvd",        STX "07ACD" ETX },
    { "Zone2Input_Dtv",        STX "07AD9" ETX },
    { "Zone2Input_Cbl-Sat",    STX "07ACC" ETX },
    { "Zone2Input_Vcr1",       STX "07AD6" ETX },
    { "Zone2Input_Dvr-Vcr2",   STX "07AD7" ETX },
    { "Zone2Input_V-Aux",      STX "07AD8" ETX },

    { "AllZonePower_On",       STX "07A1D" ETX },
    { "AllZonePower_Off",      STX "07A1E" ETX },
    
    { "MainZonePower_On",      STX "07E7E" ETX },
    { "MainZonePower_Off",     STX "07E7F" ETX },
    
    { "Zone2ZonePower_On",     STX "07EBA" ETX },
    { "Zone2ZonePower_Off",    STX "07EBB" ETX },
    
    { "Zone3ZonePower_On",     STX "07AED" ETX },
    { "Zone3ZonePower_Off",    STX "07AEE" ETX },
    
    { "Zone3Mute_On",          STX "07E26" ETX },
    { "Zone3Mute_Off",         STX "07E66" ETX },

    { "Zone3Volume_Up",        STX "07AFD" ETX },
    { "Zone3Volume_Down",      STX "07AFE" ETX },

    { "Zone3Input_Phono",      STX "07AF1" ETX },
    { "Zone3Input_Cd",         STX "07AF2" ETX },
    { "Zone3Input_Tuner",      STX "07AF3" ETX },
    { "Zone3Input_CD-R",       STX "07AF5" ETX },
    { "Zone3Input_MD-Tape",    STX "07AF4" ETX },
    { "Zone3Input_Dvd",        STX "07AFC" ETX },
    { "Zone3Input_Dtv",        STX "07AF6" ETX },
    { "Zone3Input_Cbl-Sat",    STX "07AF7" ETX },
    { "Zone3Input_Vcr1",       STX "07AF9" ETX },
    { "Zone3Input_Dvr-Vcr2",   STX "07AFA" ETX },
    { "Zone3Input_V-Aux",      STX "07AF0" ETX },

    { "NightListening_Off",    STX "07E9C" ETX },
    { "NightListening_Cinema", STX "07E9B" ETX },
    { "NightListening_Music",  STX "07ECF" ETX },

    // probably mutually exclusive
    { "Effect",                STX "07E27" ETX },
    { "Straight",              STX "07EE0" ETX },

    { "DSP_Vienna",            STX "07EE5" ETX },
    { "DSP_TheBottomLine",     STX "07EEC" ETX },
    { "DSP_TheRoxyTheatre",    STX "07EED" ETX },
    { "DSP_Disco",             STX "07EF0" ETX },
    { "DSP_Game",              STX "07EF2" ETX },
    { "DSP_7chStereo",         STX "07EFF" ETX },
    { "DSP_2chStereo",         STX "07EC0" ETX },
    { "DSP_Pop-Rock",          STX "07EF3" ETX },
    { "DSP_MonoMovie",         STX "07EF7" ETX },
    { "DSP_TvSports",          STX "07EF8" ETX },
    { "DSP_Spectacle",         STX "07EF9" ETX },
    { "DSP_SciFi",             STX "07EFA" ETX },
    { "DSP_Adventure",         STX "07EFB" ETX },
    { "DSP_General",           STX "07EFC" ETX },
    { "DSP_Standard",          STX "07EFD" ETX },
    { "DSP_Enhanced",          STX "07EFE" ETX },
    { "DSP_ThxCinema",         STX "07EC2" ETX },
    { "DSP_ThxMusic",          STX "07EC3" ETX },
    { "DSP_ThxGame",           STX "07EC8" ETX },

    { "SpeakerRelayA_On",      STX "07EAB" ETX },
    { "SpeakerRelayA_Off",     STX "07EAC" ETX },

    { "SpeakerRelayB_On",      STX "07EAD" ETX },
    { "SpeakerRelayB_Off",     STX "07EAE" ETX },

    { "2ChDecoder_PliixMovie", STX "07E67" ETX },
    { "2ChDecoder_PliixMusic", STX "07E68" ETX },
    { "2ChDecoder_Neo6Cinema", STX "07E69" ETX },
    { "2ChDecoder_Neo6Music",  STX "07E6A" ETX },
    { "2ChDecoder_PliixGame",  STX "07EC7" ETX },
    { "2ChDecoder_ProLogic",   STX "07EC9" ETX },

    { "Zone2Tone_BassUp",      STX "07A73" ETX },
    { "Zone2Tone_BassDown",    STX "07A74" ETX },
    { "Zone2Tone_TrebleUp",    STX "07A75" ETX },
    { "Zone2Tone_TrebleDown",  STX "07A76" ETX },

    { "Zone3Tone_BassUp",      STX "07A77" ETX },
    { "Zone3Tone_BassDown",    STX "07A78" ETX },
    { "Zone3Tone_TrebleUp",    STX "07A79" ETX },
    { "Zone3Tone_TrebleDown",  STX "07A7A" ETX },

    // System commands
    { "ReportCommandCode_Enable",  STX "20000" ETX },
    { "ReportCommandCode_Disable", STX "20001" ETX },

    { "ReportCommandDelay_0",      STX "20100" ETX },
    { "ReportCommandDelay_50",     STX "20101" ETX },
    { "ReportCommandDelay_100",    STX "20102" ETX },
    { "ReportCommandDelay_150",    STX "20103" ETX },
    { "ReportCommandDelay_200",    STX "20104" ETX },
    { "ReportCommandDelay_250",    STX "20105" ETX },
    { "ReportCommandDelay_300",    STX "20106" ETX },
    { "ReportCommandDelay_350",    STX "20107" ETX },
    { "ReportCommandDelay_400",    STX "20108" ETX },

    { "OsdMessageStart",           STX "21000" ETX },
    { "TuningFrequencyText",       STX "22000" ETX },
    { "MainVolumeText",            STX "22001" ETX },
    { "Zone2VolumeText",           STX "22002" ETX },
    { "MainInputText",             STX "22003" ETX },
    { "Zone2InputText",            STX "22004" ETX },
    { "Zone3VolumeText",           STX "22005" ETX },
    { "Zone3InputText",            STX "22006" ETX },
    { "FirmwareVersion",           STX "22F00" ETX },

    { "Dimmer_4",                  STX "22610" ETX },
    { "Dimmer_3",                  STX "22611" ETX },
    { "Dimmer_2",                  STX "22612" ETX },
    { "Dimmer_1",                  STX "22613" ETX },
    { "Dimmer_Off",                STX "22614" ETX },

    { "MultiChannel_6Ch",          STX "27B00" ETX },
    { "MultiChannel_8ChTuner",     STX "27B01" ETX },
    { "MultiChannel_8ChCd",        STX "27B02" ETX },
    { "MultiChannel_8ChCd-R",      STX "27B03" ETX },
    { "MultiChannel_8ChMd-Tape",   STX "27B04" ETX },
    { "MultiChannel_8ChDvd",       STX "27B05" ETX },
    { "MultiChannel_8ChDtv",       STX "27B06" ETX },
    { "MultiChannel_8ChCblSat",    STX "27B07" ETX },
    { "MultiChannel_8ChVcr1",      STX "27B09" ETX },
    { "MultiChannel_8ChDvr-Vcr2",  STX "27B0A" ETX },
    { "MultiChannel_8ChV-Aux",     STX "27B0C" ETX },

    { "NightMode_Off",             STX "28B00" ETX },
    { "NightMode_CinemaLow",       STX "28B10" ETX },
    { "NightMode_CinemaMid",       STX "28B11" ETX },
    { "NightMode_CinemaHigh",      STX "28B12" ETX },
    { "NightMode_MusicLow",        STX "28B20" ETX },
    { "NightMode_MusicMid",        STX "28B21" ETX },
    { "NightMode_MusicHigh",       STX "28B22" ETX },

    { "WakeOnRs232C_Off",          STX "2BD00" ETX },
    { "WakeOnRs232C_On",           STX "2BD01" ETX }
};


static const std::map<const char *, const char *, RxV1600::key1_less_key2_t> FMTS = {
    // System commands with values
    { "MainVolumeSet",             STX "230%02X" ETX },
    { "Zone2VolumeSet",            STX "231%02X" ETX },
    { "Zone3VolumeSet",            STX "234%02X" ETX }
};


static const std::map<uint8_t, const char *> RPTS = {
    { 0x00, "System" },
    { 0x01, "Warning" },

    { 0x10, "Playback" },
    { 0x11, "Fs" },  // sampling frequency
    { 0x12, "ExEs" },
    { 0x13, "ThrBypass" },
    { 0x14, "Red-Dts" },
    { 0x15, "TunerTuned" },
    { 0x16, "Dts96-24" },

    { 0x20, "Power" },
    { 0x21, "Input" },
    { 0x22, "AudioMode" },
    { 0x23, "AudioMute" },
    { 0x24, "Zone2Input" },
    { 0x25, "Zone2Mute" },
    { 0x26, "MainVolume" },
    { 0x27, "Zone2Volume"},
    { 0x28, "Program"},
    { 0x29, "TunerPage"},
    { 0x2A, "PresetNo"},
    { 0x2B, "Osd"},
    { 0x2C, "Sleep"},
    { 0x2D, "ExtendedSurround"},
    { 0x2E, "SpeakerRelayA"},
    { 0x2F, "SpeakerRelayB"},

    { 0x34, "Headphone"},
    { 0x35, "TunerBand"},
    { 0x3D, "SpeakerBZone"},

    { 0x4B, "Zone2Bass"},
    { 0x4C, "Zone2Treble"},
    { 0x4D, "Zone3Bass"},
    { 0x4E, "Zone3Treble"},

    { 0x5F, "DecoderSelect"},

    { 0x60, "AudioSelect"},
    { 0x61, "Dimmer"},
    { 0x6E, "2ChDecoder"},

    { 0x7B, "MultiChannel"},

    { 0x8B, "NightMode"},
    { 0x8C, "PureDirect"},

    { 0xA0, "Zone3Input"},
    { 0xA1, "Zone3Mute"},
    { 0xA2, "Zone3Volume"},
    { 0xA5, "MuteType"},
    { 0xA7, "EqualizerType"},
    { 0xA8, "ToneBypass"},

    { 0xB2, "FanControl"},
    { 0xB3, "SpeakerImpedance"},
    { 0xB9, "RemoteSensor"},
    { 0xBB, "Bi-Amp"},
    { 0xBD, "WakeOnRs232"},
};


static const std::map<uint16_t, const char *> VALS = {
    // System status
    { 0x0000, "Ok" },  // only send new commands in this status
    { 0x0001, "Busy" },
    { 0x0002, "Standby" },

    // Warnings
    { 0x0100, "Over Current" },
    { 0x0101, "Dc Detect" },
    { 0x0102, "Power Trouble" },
    { 0x0103, "Over Heat" },

    // Playback decoder
    { 0x1000, "MultiChannel Input" },
    { 0x1001, "Analog" },
    { 0x1002, "Pcm" },
    { 0x1003, "DolbyDigital Multi" },
    { 0x1004, "DolbyDigital Stereo" },
    { 0x1005, "DolbyDigital Karaoke" },
    { 0x1006, "DolbyDigital Ex" },
    { 0x1007, "Dts" },
    { 0x1008, "Dts Es" },
    { 0x1009, "Other Digital" },
    { 0x100A, "Dts Analog Mute" },
    { 0x100B, "Dts Discrete" },
    { 0x100C, "Aac Multi" },
    { 0x100D, "Aac Stereo" },

    // Sampling frequency
    { 0x1100, "Analog" },
    { 0x1101, "32 kHz" },
    { 0x1102, "44.1 kHz" },
    { 0x1103, "48 kHz" },
    { 0x1104, "64 kHz" },
    { 0x1105, "88.2 kHz" },
    { 0x1106, "96 kHz" },
    { 0x1107, "Unknown" },
    { 0x1108, "128 kHz" },
    { 0x1109, "176.4 kHz" },
    { 0x110A, "192 kHz" },
    { 0x110B, "48 kHz/96 kHz" },

    // EX / ES mode
    { 0x1200, "Off" },
    { 0x1201, "Matrix" },
    { 0x1202, "Discrete" },

    // THR DSP bypass
    { 0x1300, "Off" },
    { 0x1301, "On" },

    // RED DTS status
    { 0x1400, "Release" },
    { 0x1401, "Wait" },

    // Tuner status
    { 0x1500, "Not Tuned" },
    { 0x1501, "Tuned" },

    // DTS 96/24 mode
    { 0x1600, "Off" },
    { 0x1601, "On" },

    // Power state of all zones
    { 0x2000, "All Off" },
    { 0x2001, "All On" },
    { 0x2002, "Main On Zone2,Zone3 Off" },
    { 0x2003, "Zone2,Zone3 On Main Off" },
    { 0x2004, "Main,Zone2 On Zone3 Off" },
    { 0x2005, "Main,Zone3 On Zone2 Off" },
    { 0x2006, "Zone2 On Main,Zone3 Off" },
    { 0x2007, "Zone3 On Main,Zone2 Off" },

    // Main input
    { 0x2100, "Phono" },
    { 0x2101, "Cd" },
    { 0x2102, "Tuner" },
    { 0x2103, "Cd-R" },
    { 0x2104, "Md/Tape" },
    { 0x2105, "Dvd" },
    { 0x2106, "Dtv" },
    { 0x2107, "Cbl/Sat" },
    { 0x2109, "Vcr1" },
    { 0x210A, "Dvr/Vcr2" },
    { 0x210C, "V-Aux" },

    // Multichannel overriding main input
    { 0x2110, "MultiChannel Phono" },
    { 0x2111, "MultiChannel Cd" },
    { 0x2112, "MultiChannel Tuner" },
    { 0x2113, "MultiChannel Cd-R" },
    { 0x2114, "MultiChannel Md/Tape" },
    { 0x2115, "MultiChannel Dvd" },
    { 0x2116, "MultiChannel Dtv" },
    { 0x2117, "MultiChannel Cbl/Sat" },
    { 0x2119, "MultiChannel Vcr1" },
    { 0x211A, "MultiChannel Dvr/Vcr2" },
    { 0x211C, "MultiChannel V-Aux" },

    // Audio type without decoder
    { 0x2200, "Auto" },
    { 0x2203, "Auto Coax/Opt" },
    { 0x2204, "Auto Analog" },
    { 0x2205, "Auto Analog Only" },
    { 0x2208, "Auto Hdmi" },

    // Audio type for Dts decoder mode
    { 0x2210, "Dts Auto" },
    { 0x2213, "Dts Coax/Opt" },
    { 0x2214, "Dts Analog" },
    { 0x2215, "Dts Analog Only" },
    { 0x2218, "Dts Hdmi" },

    // Audio type for AAC decoder mode
    { 0x2220, "Aac Auto" },
    { 0x2223, "Aac Coax/Opt" },
    { 0x2224, "Aac Analog" },
    { 0x2225, "Aac Analog Only" },
    { 0x2228, "Aac Hdmi" },

    // Main audio mute
    { 0x2300, "Off" },
    { 0x2301, "On" },

    // Zone 2 input
    { 0x2400, "Phono" },
    { 0x2401, "Cd" },
    { 0x2402, "Tuner" },
    { 0x2403, "Cd-R" },
    { 0x2404, "Md/Tape" },
    { 0x2405, "Dvd" },
    { 0x2406, "Dtv" },
    { 0x2407, "Cbl/Sat" },
    { 0x2409, "Vcr1" },
    { 0x240A, "Dvr/Vcr2" },
    { 0x240C, "V-Aux" },

    // Zone 2 mute
    { 0x2500, "Off" },
    { 0x2501, "On" },

    // Main volume
    { 0x2600, "Infinite" },
    // { 0x2627, "-80 dB" },   calculated
    { 0x26C7, "0 dB" },
    // { 0x26E8, "16.5 dB" },  calculated

    // Zone 2 volume
    { 0x2700, "Infinite" },
    // { 0x2727, "-80 dB" },   calculated
    { 0x27C7, "0 dB" },
    // { 0x27E8, "16.5 dB" },  calculated

    // DSP effect program
    { 0x2805, "Vienna" },
    { 0x280E, "The Bottom Line" },
    { 0x2810, "The Roxy Theatre" },
    { 0x2814, "Disco" },
    { 0x2816, "Game" },
    { 0x2817, "7 Channel Stereo" },
    { 0x2818, "Pop/Rock" },
    { 0x2820, "Mono Movie" },
    { 0x2821, "Tv Sports" },
    { 0x2824, "Spectacle" },
    { 0x2825, "Sci-Fi" },
    { 0x2828, "Adventure" },
    { 0x2829, "General" },
    { 0x282C, "Standard" },
    { 0x282D, "Enhanced" },
    { 0x2834, "2 Channel Stereo" },
    { 0x2836, "Thx Cinema" },
    { 0x2837, "Thx Music" },
    { 0x283C, "Thx Game" },

    // Straight overriding DSP program 
    { 0x2885, "Straight Vienna" },
    { 0x288E, "Straight The Bottom Line" },
    { 0x2890, "Straight The Roxy Theatre" },
    { 0x2894, "Straight Disco" },
    { 0x2896, "Straight Game" },
    { 0x2897, "Straight 7 Channel Stereo" },
    { 0x2898, "Straight Pop/Rock" },
    { 0x28A0, "Straight Mono Movie" },
    { 0x28A1, "Straight Tv Sports" },
    { 0x28A4, "Straight Spectacle" },
    { 0x28A5, "Straight Sci-Fi" },
    { 0x28A8, "Straight Adventure" },
    { 0x28A9, "Straight General" },
    { 0x28AC, "Straight Standard" },
    { 0x28AD, "Straight Enhanced" },
    { 0x28B4, "Straight 2 Channel Stereo" },
    { 0x28B6, "Straight Thx Cinema" },
    { 0x28B7, "Straight Thx Music" },
    { 0x28BC, "Straight Thx Game" },

    // Tuner preset page
    { 0x2900, "A" },
    { 0x2901, "B" },
    { 0x2902, "C" },
    { 0x2903, "D" },
    { 0x2904, "E" },

    // Tuner preset number
    { 0x2A00, "1" },
    { 0x2A01, "2" },
    { 0x2A02, "3" },
    { 0x2A03, "4" },
    { 0x2A04, "5" },
    { 0x2A05, "6" },
    { 0x2A06, "7" },
    { 0x2A07, "8" },

    // OSD
    { 0x2B00, "Full" },
    { 0x2B01, "Short" },
    { 0x2B02, "Off" },

    // Sleep delay
    { 0x2C00, "120" },
    { 0x2C01, "90" },
    { 0x2C02, "60" },
    { 0x2C03, "30" },
    { 0x2C04, "Off" },

    // Extended surround mode
    { 0x2D00, "Off" },
    { 0x2D01, "EX/ES" },
    { 0x2D02, "Discrete On" },
    { 0x2D03, "Auto" },
    { 0x2D04, "EX" },
    { 0x2D05, "PLIIx Movie" },
    { 0x2D06, "PLIIx Music" },

    // Speaker Relay A
    { 0x2E00, "Off" },
    { 0x2E01, "On" },

    // Speaker Relay B
    { 0x2F00, "Off" },
    { 0x2F01, "On" },

    /// @todo fill gaps

    // Headphone
    { 0x3400, "Off" },
    { 0x3401, "On" },

    // Speaker B zone
    { 0x3D00, "Main" },
    { 0x3D01, "Zone 2" },

    // Zone 2 Bass
    { 0x4B00, "-10dB" },
    { 0x4B01, "-8dB" },
    { 0x4B02, "-6dB" },
    { 0x4B03, "-4dB" },
    { 0x4B04, "-2dB" },
    { 0x4B05, "0dB" },
    { 0x4B06, "2dB" },
    { 0x4B07, "4dB" },
    { 0x4B08, "6dB" },
    { 0x4B09, "8dB" },
    { 0x4B0A, "10dB" },

    // Zone 2 Treble
    { 0x4C00, "-10dB" },
    { 0x4C01, "-8dB" },
    { 0x4C02, "-6dB" },
    { 0x4C03, "-4dB" },
    { 0x4C04, "-2dB" },
    { 0x4C05, "0dB" },
    { 0x4C06, "2dB" },
    { 0x4C07, "4dB" },
    { 0x4C08, "6dB" },
    { 0x4C09, "8dB" },
    { 0x4C0A, "10dB" },

    // Zone 2 Bass
    { 0x4D00, "-10dB" },
    { 0x4D01, "-8dB" },
    { 0x4D02, "-6dB" },
    { 0x4D03, "-4dB" },
    { 0x4D04, "-2dB" },
    { 0x4D05, "0dB" },
    { 0x4D06, "2dB" },
    { 0x4D07, "4dB" },
    { 0x4D08, "6dB" },
    { 0x4D09, "8dB" },
    { 0x4D0A, "10dB" },

    // Zone 3 Treble
    { 0x4E00, "-10dB" },
    { 0x4E01, "-8dB" },
    { 0x4E02, "-6dB" },
    { 0x4E03, "-4dB" },
    { 0x4E04, "-2dB" },
    { 0x4E05, "0dB" },
    { 0x4E06, "2dB" },
    { 0x4E07, "4dB" },
    { 0x4E08, "6dB" },
    { 0x4E09, "8dB" },
    { 0x4E0A, "10dB" },

    // Initial Decoder
    { 0x5F00, "Auto" },
    { 0x5F01, "Last" },

    // Initial Audio
    { 0x6000, "Auto" },
    { 0x6001, "Last" },

    // Dimmer
    { 0x6100, "-4" },
    { 0x6101, "-3" },
    { 0x6102, "-2" },
    { 0x6103, "-1" },
    { 0x6104, "0" },

    // Zone 2 volume out
    { 0x6600, "Variable" },
    { 0x6601, "Fixed" },

    // Memory guard
    { 0x6800, "Off" },
    { 0x6801, "On" },

    // Zone 3 volume out
    { 0x6B00, "Variable" },
    { 0x6B01, "Fixed" },

    // 2 channel decoder
    { 0x6E00, "Pro Logic" },
    { 0x6E01, "PLIIx Movie" },
    { 0x6E02, "PLIIx Music" },
    { 0x6E03, "PLIIx Game" },
    { 0x6E04, "Neo:6 Cinema" },
    { 0x6E05, "Neo:6 Music" },

    // Multi channel select
    { 0x7B00, "6ch" },
    { 0x7B01, "8ch Tuner" },
    { 0x7B02, "8ch CD" },
    { 0x7B03, "8ch CD-R" },
    { 0x7B04, "8ch MD/TAPE" },
    { 0x7B05, "8ch DVD" },
    { 0x7B06, "8ch DTV" },
    { 0x7B07, "8ch CBL/SAT" },
    { 0x7B09, "8ch VCR1" },
    { 0x7B0A, "8ch DVR/VCR2" },
    { 0x7B0C, "8ch V-AUX" },

    // Night mode parameters
    { 0x8B00, "Off" },
    { 0x8B10, "Cinema Level Low" },
    { 0x8B11, "Cinema Level Middle" },
    { 0x8B12, "Cinema Level High" },
    { 0x8B20, "Music Level Low" },
    { 0x8B21, "Music Level Middle" },
    { 0x8B22, "Music Level High" },

    // Pure direct
    { 0x8C00, "Off" },
    { 0x8C01, "On" },

    // Zone 3 input
    { 0xA000, "Phono" },
    { 0xA001, "Cd" },
    { 0xA002, "Tuner" },
    { 0xA003, "Cd-R" },
    { 0xA004, "Md/Tape" },
    { 0xA005, "Dvd" },
    { 0xA006, "Dtv" },
    { 0xA007, "Cbl/Sat" },
    { 0xA009, "Vcr1" },
    { 0xA00A, "Dvr/Vcr2" },
    { 0xA00C, "V-Aux" },

    // Zone 3 mute
    { 0xA100, "Off" },
    { 0xA101, "On" },

    // Zone3 volume
    { 0xA200, "Infinite" },
    // { 0xA227, "-80 dB" },   calculated
    { 0xA2C7, "0 dB" },
    // { 0xA2E8, "16.5 dB" },  calculated

    { 0xA500, "Full" },
    { 0xA501, "-20 dB" },
    
    // EQ select type
    { 0xA700, "Auto Peq" },
    { 0xA701, "Geq" },
    { 0xA702, "Eq off" },

    // Tone bypass
    { 0xA800, "Auto" },
    { 0xA801, "Off" },

    // Fan control
    { 0xB200, "Auto" },
    { 0xB201, "Continuous" },

    // Speaker impedance
    { 0xB300, "8 ohm" },
    { 0xB301, "6 ohm" },

    // Remote sensor (IR)
    { 0xB900, "On" },
    { 0xB901, "Off" },

    // Bi-Amp
    { 0xBB00, "On" },
    { 0xBB01, "Off" },

    // Wake on RS232
    { 0xBD00, "No" },
    { 0xBD01, "Yes" },
};


static const std::map<uint16_t, const char *> TXTS = {
    { 0x00, "TunerFrequency" },
    { 0x01, "MainVolume" },
    { 0x02, "Zone2Volume" },
    { 0x03, "Main Input" },
    { 0x04, "Zone2Input" },
    { 0x05, "Zone3Volume" },
    { 0x06, "Zone3Input" },
    { 0xF0, "OperationCode" },
    { 0xFF, "Versions" }        // Major,SwHi,SwLo,_,RS232,DSPHi,DSPLo, 
};


RxV1600::cmds_iter_t RxV1600::begin() {
    return CMDS.cbegin();
}


RxV1600::cmds_iter_t RxV1600::end() {
    return CMDS.cend();
}


RxV1600::RxV1600() {
    memset(_status, UNKNOWN_VALUE, sizeof(_status));
}


const char *RxV1600::command(const char *name) {
    auto cmd = CMDS.find(name);

    return (cmd == CMDS.end()) ? NULL : cmd->second;
}


const char *RxV1600::command_value(const char *name, uint8_t value) {
    static char cmd[8] = "";

    auto fmt = FMTS.find(name);
    if( fmt == FMTS.end() ) return NULL;
    if( snprintf(cmd, sizeof(cmd), fmt->second, value) >= sizeof(cmd) ) return NULL;

    return cmd;
}


const char *RxV1600::report_name(uint8_t id) {
    auto rpt = RPTS.find(id);

    return (rpt == RPTS.end()) ? NULL : rpt->second;
}


const char *RxV1600::display_name(uint8_t id) {
    auto dsp = TXTS.find(id);

    return (dsp == TXTS.end()) ? NULL : dsp->second;
}


uint8_t RxV1600::report_value(uint8_t id) {
    return _status[id];
}


const char *RxV1600::report_value_string(uint8_t id) {
    static char buf[10];

    uint16_t index = id << 8 | _status[id];
    auto val = VALS.find(index);

    if( val != VALS.end() ) {
        return val->second;
    }

    if( id == 0x26 || id == 0x27 || id == 0xa2 ) {
        float dB = ((float)_status[id] - 0xc7) / 2;
        snprintf(buf, sizeof(buf), "%.1f dB", dB);
        return buf;
    }

    return NULL;
}


static uint8_t nibble( char ch ) {
    if( (ch >= '0') && (ch <= '9') ) {
        return ch - '0';
    } 
    else if( (ch >= 'A') && (ch <= 'F') ) {
        return 10 + ch - 'A';
    }
    else {
        return RxV1600::UNKNOWN_VALUE;
    }
}


bool RxV1600::decode( const char *resp, uint8_t &id, guard_t &guard, origin_t &origin ) {
    if( resp[0] != *STX || resp[7] != *ETX ) return false;
    if( resp[1] < '0' || resp[1] > '4' ) return false;
    if( resp[2] < '0' || resp[2] > '2' ) return false;

    uint8_t val[4];
    for( int i=0; i<sizeof(val); i++) {
        val[i] = nibble(resp[i+3]);
        if( val[i] == UNKNOWN_VALUE ) return false;
    }

    guard = (guard_t)(resp[2] - '0');
    origin = (origin_t)(resp[1] - '0');
    id = (val[0] << 4) | val[1];
    _status[id] = (val[2] << 4) | val[3];

    return true;
}


bool RxV1600::decodeText( const char *resp, uint8_t &id, char *text ) {
    if( resp[0] != *DC1 || resp[11] != *ETX ) return false;

    uint8_t val[2];
    for( int i=0; i<sizeof(val); i++) {
        val[i] = nibble(resp[i+1]);
        if( val[i] == UNKNOWN_VALUE ) return false;
    }

    id = (val[0] << 4) | val[1];
    for( int i=0; i<8; i++ ) {
        text[i] = resp[i+3];
    }
    text[8] = '\0';

    return true;
}


bool RxV1600::decodeConfig( const char *resp, bool &power ) {
    if( resp[0] != *DC2 ) return false;

    uint8_t len = nibble(resp[7]);
    if( len == UNKNOWN_VALUE ) return false;
    len = len << 4 | nibble(resp[8]);
    if( len == UNKNOWN_VALUE ) return false;

    power = !(len == 10);

    const char *curr = &resp[16];  // start with DT7

    // assuming config data up to len is valid
    _status[0x00] = nibble(*(curr++));  // System
    _status[0x20] = nibble(*(curr++));  // Power
    _status[0x21] = nibble(*(curr++));  // Input

    if( !power ) return true;

    if( nibble(*(curr++) == 1) ) _status[0x21] |= 0x10;  // set MultiChannel bit on Input
    _status[0x22] = nibble(*(curr++));  // Audio select
    _status[0x23] = nibble(*(curr++));  // Audio mute
    _status[0x24] = nibble(*(curr++));  // Zone 2 input
    _status[0x25] = nibble(*(curr++));  // Zone 2 mute
    _status[0x26] = nibble(*(curr++));  // Main volume hi
    if( _status[0x26] != UNKNOWN_VALUE )  _status[0x26] = _status[0x26] << 4 | nibble(*(curr++));  // lo
    _status[0x27] = nibble(*(curr++));  // Zone 2 volume hi
    if( _status[0x27] != UNKNOWN_VALUE )  _status[0x27] = _status[0x27] << 4 | nibble(*(curr++));  // lo
    _status[0x28] = nibble(*(curr++));  // DSP effect program
    if( _status[0x28] != UNKNOWN_VALUE )  _status[0x28] = _status[0x28] << 4 | nibble(*(curr++));  // lo
    if( nibble(*(curr++) == 0) ) _status[0x28] |= 0x80;  // Set Straight bit on program
    _status[0x2D] = nibble(*(curr++));  // Extended surround
    _status[0x2B] = nibble(*(curr++));  // OSD
    _status[0x2C] = nibble(*(curr++));  // Sleep delay
    _status[0x29] = nibble(*(curr++));  // Tuner preset page
    _status[0x2A] = nibble(*(curr++));  // Tuner preset number
    _status[0x8B] = nibble(*(curr++));  // Night mode hi
    if( _status[0x8B] != UNKNOWN_VALUE )  _status[0x8B] = _status[0x8B] << 4 | nibble(*(curr++));  // lo
    _status[0x2E] = nibble(*(curr++));  // Speaker A
    _status[0x2F] = nibble(*(curr++));  // Speaker B
    _status[0x10] = nibble(*(curr++));  // Playback decoder
    _status[0x11] = nibble(*(curr++));  // Sampling frequency
    _status[0x12] = nibble(*(curr++));  // EX / ES mode
    _status[0x13] = nibble(*(curr++));  // THR DSP bypass
    _status[0x14] = nibble(*(curr++));  // RED DTS status
    _status[0x34] = nibble(*(curr++));  // Headphone
    _status[0x35] = nibble(*(curr++));  // Tuner band
    _status[0x15] = nibble(*(curr++));  // Tuner tuned
    curr++;  // DC1 Trigger Output
    uint8_t n = nibble(*(curr++)); if( n != 0 && n != UNKNOWN_VALUE ) _status[0x22] |= n << 4;  // Decoder mode
    curr++;  // Dual mono
    curr++;  // DC1 Trigger Control
    _status[0x16] = nibble(*(curr++));  // DTS 96/24 mode
    curr++;  // DC2 Trigger Control
    curr++;  // DC2 Trigger Output
    _status[0x3D] = nibble(*(curr++));  // Speaker B zone
    curr += 83-47;  // skip DT47-DT82
    _status[0x5F] = nibble(*(curr++));  // Decoder select
    _status[0x60] = nibble(*(curr++));  // Audio select
    _status[0x61] = nibble(*(curr++));  // Dimmer
    curr += 106-86;  // skip DT86-DT105
    _status[0xA7] = nibble(*(curr++));  // Equalizer type
    curr += 119-107;  // skip DT107-DT118
    _status[0x6E] = nibble(*(curr++));  // 2 channel decoder
    curr += 123-120;  // skip DT120-DT122
    _status[0xB2] = nibble(*(curr++));  // Fan control
    _status[0xB3] = nibble(*(curr++));  // Speaker impedance
    curr++;  // Tuner Setup
    _status[0x8C] = nibble(*(curr++));  // Pure direct
    _status[0xA0] = nibble(*(curr++));  // Zone 3 input
    _status[0xA1] = nibble(*(curr++));  // Zone 3 mute
    _status[0xA2] = nibble(*(curr++));  // Zone 3 volume hi
    if( _status[0xA2] != UNKNOWN_VALUE )  _status[0xA2] = _status[0xA2] << 4 | nibble(*(curr++));  // lo
    _status[0xB9] = nibble(*(curr++));  // Remote sensor (IR)
    _status[0x7B] = nibble(*(curr++));  // Multi channel select
    curr++;  // Remote ID XM
    _status[0xBB] = nibble(*(curr++));  // Bi-Amp
    curr += 139-135;  // skip DT135-DT138
    _status[0x4B] = nibble(*(curr++));  // Zone 2 Bass
    _status[0x4C] = nibble(*(curr++));  // Zone 2 Treble
    _status[0x4D] = nibble(*(curr++));  // Zone 3 Bass
    _status[0x4E] = nibble(*(curr++));  // Zone 3 Treble
    _status[0xA8] = nibble(*(curr++));  // Tone bypass
    _status[0xBD] = nibble(*(curr++));  // Wake on RS232

    return true;
}
