// Web remote control for Yamaha RX-V1600 AV Receiver
// Modern single-page UI with live state updates via polling

#include <Arduino.h>


#define LED_PIN LED_BUILTIN
#define BTN_PIN 26


// Infrastructure
#include <Syslog.h>
#include <WiFiManager.h>


// Web server
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClient.h>


// Time sync
#include <time.h>


// Reset reason
#include "rom/rtc.h"


#define WEBSERVER_PORT 80

AsyncWebServer web_server(WEBSERVER_PORT);
uint32_t shouldReboot = 0;
uint32_t delayReboot = 100;


// Syslog
WiFiUDP logUDP;
Syslog syslog(logUDP, SYSLOG_PROTO_IETF);
char msg[512];

char start_time[30];


#include <rxv1600.h>

RxV1600Comm rxvcomm(Serial1);
RxV1600 rxv;

int first_vol = 0;


// define constant IsoDate as nicer variant of __DATE__
constexpr unsigned int compileYear = (__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0');
constexpr unsigned int compileMonth = (__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? 1 : ((__DATE__[2] == 'n') ? 6 : 7))
                                : (__DATE__[0] == 'F') ? 2
                                : (__DATE__[0] == 'M') ? ((__DATE__[2] == 'r') ? 3 : 5)
                                : (__DATE__[0] == 'A') ? ((__DATE__[2] == 'p') ? 4 : 8)
                                : (__DATE__[0] == 'S') ? 9
                                : (__DATE__[0] == 'O') ? 10
                                : (__DATE__[0] == 'N') ? 11
                                : (__DATE__[0] == 'D') ? 12
                                : 0;
constexpr unsigned int compileDay = (__DATE__[4] == ' ') ? (__DATE__[5] - '0') : (__DATE__[4] - '0') * 10 + (__DATE__[5] - '0');
constexpr char IsoDate[] = {
    compileYear/1000 + '0', (compileYear % 1000)/100 + '0', (compileYear % 100)/10 + '0', compileYear % 10 + '0',
    '-',  compileMonth/10 + '0', compileMonth%10 + '0',
    '-',  compileDay/10 + '0', compileDay%10 + '0',
    0
};


void slog(const char *message, uint16_t pri = LOG_INFO) {
    static bool log_infos = true;

    if (pri < LOG_INFO || log_infos) {
        Serial.println(message);
        syslog.log(pri, message);
    }

    if (log_infos && millis() > 10 * 60 * 1000) {
        log_infos = false;
        slog("Switch off info level messages", LOG_NOTICE);
    }
}


// JSON helper: escape a string value (handles NULL)
const char *js(const char *s) {
    return s ? s : "";
}


// Send command directly to RX-V1600
void send_cmd(const char *name) {
    const char *cmd = rxv.command(name);
    if( cmd ) {
        if( !rxvcomm.send(cmd) ) {
            snprintf(msg, sizeof(msg), "Busy, discarding command '%s'", name);
            slog(msg);
        }
    }
}


void send_cmd_value(const char *name, uint8_t value) {
    const char *cmd = rxv.command_value(name, value);
    if( cmd ) {
        if( !rxvcomm.send(cmd) ) {
            snprintf(msg, sizeof(msg), "Busy, discarding command '%s,%u'", name, value);
            slog(msg);
        }
    }
}


const char* afterLastSpace(const char* s) {
    if (!s) return nullptr;
    const char* p = strrchr(s, ' ');
    return p ? p + 1 : s;
}


// JSON state endpoint for UI polling
void send_state(AsyncWebServerRequest *request) {
    static char json[448];

    const char *power = js(rxv.report_value_string(0x20));
    const char *input = js(rxv.report_value_string(0x21));
    const char *pch = input;
    while (*pch && *pch != ' ') pch++;
    if (*pch == ' ') input = ++pch;
    const char *speaker_a = js(rxv.report_value_string(0x2e));
    const char *speaker_b = js(rxv.report_value_string(0x2f));
    const char *night = js(afterLastSpace(rxv.report_value_string(0x8B)));
    const char *mute = js(rxv.report_value_string(0x23));
    const char *volume = rxv.report_value_string(0x26);
    int vol_raw = volume ? atoi(volume) : -999;

    snprintf(json, sizeof(json),
        "{\"power\":\"%s\",\"input\":\"%s\","
        "\"spkA\":\"%s\",\"spkB\":\"%s\","
        "\"night\":\"%s\",\"mute\":\"%s\","
        "\"volume\":\"%s\",\"volDb\":%d,"
        "\"version\":\"" VERSION "\","
        "\"heap\":%u,"
        "\"started\":\"%s\","
        "\"built\":\"%s\"}",
        power, input, speaker_a, speaker_b,
        night, mute,
        volume ? volume : "", vol_raw,
        ESP.getFreeHeap(),
        start_time, IsoDate);

    request->send(200, "application/json", json);
}


// The single-page web app
const char MAIN_PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>RX-V1600</title>
<link href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAgMAAABinRfyAAAADFBMVEUqYbutnpTMuq/70SQgIef5AAAAVUlEQVQIHWOAAPkvDAyM3+Y7MLA7NV5g4GVqKGCQYWowYTBhapBhMGB04GE4/0X+M8Pxi+6XGS67XzzO8FH+iz/Dl/q/8gx/2S/UM/y/wP6f4T8QAAB3Bx3jhPJqfQAAAABJRU5ErkJggg==" rel="icon" type="image/x-icon"/>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;
 display:flex;flex-direction:column;align-items:center;padding:12px;overflow-x:hidden}
.w{width:100%;max-width:380px}
.hdr{display:flex;align-items:center;justify-content:space-between;padding:2px 4px;margin-bottom:4px}
.hdr span{font-size:.7em;text-transform:uppercase;color:#7a8ba8;letter-spacing:.04em}
.hdr em{font-style:normal;font-size:.8em;color:#a0c4ff}
.row{display:flex;gap:6px;margin-bottom:6px}
button{flex:1;padding:10px 6px;border:none;border-radius:10px;font-size:.9em;font-weight:600;
 cursor:pointer;color:#fff;background:#3a506b;-webkit-tap-highlight-color:transparent}
button:active{opacity:.7}
.on{background:#2d6a4f}.off{background:#9b2226}
.act{box-shadow:0 0 0 3px #a0c4ff;transform:scale(1.04)}
.dis{opacity:.3;pointer-events:none}
.vv{text-align:center;font-size:2.2em;font-weight:700;color:#a0c4ff;margin:8px 0;
 font-variant-numeric:tabular-nums}
.vs{width:100%;height:48px;-webkit-appearance:none;appearance:none;background:transparent;cursor:pointer;margin:4px 0}
.vs::-webkit-slider-runnable-track{height:8px;background:#3a506b;border-radius:4px}
.vs::-webkit-slider-thumb{-webkit-appearance:none;width:36px;height:36px;background:#a0c4ff;
 border-radius:50%;margin-top:-14px;border:3px solid #16213e}
.vs::-moz-range-track{height:8px;background:#3a506b;border-radius:4px;border:none}
.vs::-moz-range-thumb{width:36px;height:36px;background:#a0c4ff;border-radius:50%;border:3px solid #16213e}
.vb{display:flex;gap:6px}.vb button{font-size:1.3em;padding:12px;border-radius:10px}
.sys{display:none;gap:6px;margin-top:6px}.sys.show{display:flex}
.sys button{font-size:.8em;padding:8px;background:#6b3a3a}
.tog{margin-top:16px;font-size:.7em;color:#4a5568;cursor:pointer;text-align:center}
.tog:hover{color:#7a8ba8}
.info{font-size:.7em;color:#4a5568;text-align:center;margin-top:6px}
.info a{color:#7a8ba8}
</style>
</head>
<body>
<div class="w">

<div class="hdr"><span>Power</span><em id="st-power"></em></div>
<div class="row">
 <button id="b-pon" class="on" onclick="cmd('/power-on')">On</button>
 <button id="b-poff" class="off" onclick="cmd('/power-off')">Off</button>
</div>

<div id="ctrl">
<div class="hdr"><span>Input</span><em id="st-input"></em></div>
<div class="row">
 <button id="b-tv" onclick="cmd('/tv')">TV</button>
 <button id="b-bt" onclick="cmd('/bt')">Bluetooth</button>
</div>

<div class="hdr"><span>Speaker A</span><em id="st-spkA"></em></div>
<div class="row">
 <button id="b-aon" class="on" onclick="cmd('/a-on')">On</button>
 <button id="b-aoff" class="off" onclick="cmd('/a-off')">Off</button>
</div>

<div class="hdr"><span>Speaker B</span><em id="st-spkB"></em></div>
<div class="row">
 <button id="b-bon" class="on" onclick="cmd('/b-on')">On</button>
 <button id="b-boff" class="off" onclick="cmd('/b-off')">Off</button>
</div>

<div class="hdr"><span>Night</span><em id="st-night"></em></div>
<div class="row">
 <button id="b-day" class="on" onclick="cmd('/day')">Day</button>
 <button id="b-ngt" class="off" onclick="cmd('/night')">Night</button>
</div>

<div class="hdr"><span>Mute</span><em id="st-mute"></em></div>
<div class="row">
 <button id="b-umut" class="on" onclick="cmd('/mute-off')">Unmute</button>
 <button id="b-mut" class="off" onclick="cmd('/mute-on')">Mute</button>
</div>

<div class="vv" id="vol-value">-- dB</div>
<input class="vs" id="vol-slider" type="range" min="-80" max="16" step="1" value="-40">
<div class="vb">
 <button onclick="volStep(-1)">&#9660;</button>
 <button onclick="volStep(1)">&#9650;</button>
</div>
</div>

<div class="info" id="info"></div>

<div class="tog" onclick="document.getElementById('sys').classList.toggle('show')">&#9881; System</div>
<div class="sys" id="sys">
 <button onclick="if(confirm('Wipe WLAN config and restart?'))cmd('/wipe')">Wipe WLAN</button>
 <button onclick="if(confirm('Reset?'))cmd('/reset')">Reset</button>
 <button onclick="location.href='/update'">Update</button>
</div>
</div>

<script>
var busy=false,volBusy=false,volTimer=null;
function ajax(m,u,d,cb){
 var x=new XMLHttpRequest();
 x.onreadystatechange=function(){if(x.readyState==4)cb(x)};
 x.open(m,u);
 if(d){x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');x.send(d)}
 else x.send();
}
function cmd(u){
 if(busy)return;busy=true;
 ajax('POST',u,null,function(){busy=false;poll()});
}
function volStep(d){
 if(volBusy)return;volBusy=true;
 ajax('POST',d>0?'/vol-up':'/vol-down',null,function(){
  volBusy=false;if(volTimer)clearTimeout(volTimer);volTimer=setTimeout(poll,500);
 });
}
var sl=document.getElementById('vol-slider'),slBusy=false;
sl.oninput=function(){
 document.getElementById('vol-value').textContent=this.value+'.0 dB';
 if(!slBusy){slBusy=true;ajax('POST','/vol','v='+this.value,function(){slBusy=false})}
};
sl.onchange=function(){
 ajax('POST','/vol','v='+this.value,function(){slBusy=false;poll()});
};
function poll(){
 ajax('GET','/state',null,function(x){
  if(x.status!=200)return;
  try{var s=JSON.parse(x.responseText)}catch(e){return}
  document.getElementById('st-power').textContent=s.power;
  document.getElementById('st-input').textContent=s.input;
  document.getElementById('st-spkA').textContent=s.spkA;
  document.getElementById('st-spkB').textContent=s.spkB;
  document.getElementById('st-night').textContent=s.night;
  document.getElementById('st-mute').textContent=s.mute;
  function hi(a,b,v){var x=document.getElementById(a),y=document.getElementById(b);
   x.classList.toggle('act',v);y.classList.toggle('act',!v)}
  hi('b-pon','b-poff',/Main On/.test(s.power));
  var on=/Main On/.test(s.power);
  document.getElementById('ctrl').classList.toggle('dis',!on);
  hi('b-tv','b-bt',s.input==='Dtv'||s.input==='TV');
  hi('b-aon','b-aoff',s.spkA==='On');
  hi('b-bon','b-boff',s.spkB==='On');
  hi('b-day','b-ngt',s.night==='Off');
  hi('b-umut','b-mut',s.mute==='Off');
  if(s.volDb>-999){
   document.getElementById('vol-value').textContent=s.volume;
   if(!slBusy)sl.value=s.volDb;
  }
  document.getElementById('info').innerHTML=
   (s.started?'Started '+s.started:'')+(s.built?' &middot; Built '+s.built:'')+'<br>'+
   'v'+s.version+' &middot; '+(s.heap/1024|0)+'kB &middot; <a href="https://github.com/joba-1/Joba_RxV1600">Github</a>';
 });
}
poll();setInterval(poll,2000);
</script>
</body>
</html>)rawliteral";


void setup_webserver() {
    // Index page
    web_server.on("/", [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", MAIN_PAGE);
    });

    // State endpoint for polling
    web_server.on("/state", HTTP_GET, send_state);

    // Power
    web_server.on("/power-on", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("MainZonePower_On");
        request->send(200);
    });
    web_server.on("/power-off", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("MainZonePower_Off");
        request->send(200);
    });

    // Input
    web_server.on("/tv", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("Input_Dtv");
        request->send(200);
    });
    web_server.on("/bt", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("Input_Cbl-Sat");
        request->send(200);
    });

    // Speakers
    web_server.on("/a-on", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("SpeakerRelayA_On");
        request->send(200);
    });
    web_server.on("/a-off", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("SpeakerRelayA_Off");
        request->send(200);
    });
    web_server.on("/b-on", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("SpeakerRelayB_On");
        request->send(200);
    });
    web_server.on("/b-off", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("SpeakerRelayB_Off");
        request->send(200);
    });

    // Night mode
    web_server.on("/day", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("NightMode_Off");
        request->send(200);
    });
    web_server.on("/night", HTTP_POST, [](AsyncWebServerRequest *request) {
        const char *mode;
        uint8_t current_mode = rxv.report_value(0x8b);
        switch(current_mode) {
            case 0x10: case 0x20: mode = "NightMode_CinemaMid"; break;
            case 0x11: case 0x21: mode = "NightMode_CinemaHigh"; break;
            default: mode = "NightMode_CinemaLow"; break;
        }
        send_cmd(mode);
        request->send(200);
    });

    // Mute
    web_server.on("/mute-on", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("Mute_On");
        request->send(200);
    });
    web_server.on("/mute-off", HTTP_POST, [](AsyncWebServerRequest *request) {
        send_cmd("Mute_Off");
        request->send(200);
    });

    // Volume
    web_server.on("/vol-up", HTTP_POST, [](AsyncWebServerRequest *request) {
        first_vol = 1;
        send_cmd("MainVolume_Up");
        request->send(200);
    });
    web_server.on("/vol-down", HTTP_POST, [](AsyncWebServerRequest *request) {
        first_vol = -1;
        send_cmd("MainVolume_Down");
        request->send(200);
    });
    web_server.on("/vol", HTTP_POST, [](AsyncWebServerRequest *request) {
        String arg = request->arg("v");
        if (!arg.isEmpty()) {
            int volume = atoi(arg.c_str());
            send_cmd_value("MainVolumeSet", volume * 2 + 199);
        }
        request->send(200);
    });

    // Reset
    web_server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        slog("RESET ESP32", LOG_NOTICE);
        request->send(200, "text/plain", "Resetting...");
        delay(200);
        Serial1.end();
        ESP.restart();
    });

    web_server.on("/wipe", HTTP_POST, [](AsyncWebServerRequest *request) {
        slog("WIPE WLAN config and restart", LOG_NOTICE);
        request->send(200, "text/plain", "Wiping WLAN config...");
        delay(200);
        Serial1.end();
        WiFiManager wm;
        wm.resetSettings();
        ESP.restart();
    });

    // Firmware Update
    web_server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        static const char page[] =
            "<!doctype html><html><head>"
            "<meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>" PROGNAME " Update</title>"
            "<style>body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;display:flex;"
            "flex-direction:column;align-items:center;padding:40px}"
            "h1{color:#a0c4ff}form{margin:20px}input{margin:8px;padding:8px;border-radius:8px;border:none}"
            "input[type=submit]{background:#3a506b;color:white;cursor:pointer;padding:10px 24px}</style>"
            "</head><body>"
            "<h1>" PROGNAME " v" VERSION "</h1>"
            "<form method='POST' action='/update' enctype='multipart/form-data'>"
            "<input type='file' name='update'><input type='submit' value='Update'>"
            "</form></body></html>";
        request->send(200, "text/html", page);
    });

    web_server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        if( !Update.hasError() ) {
            shouldReboot = millis();
            if( shouldReboot == 0 ) shouldReboot--;
            delayReboot = 5000;
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if(!index) {
            Serial.printf("Update Start: %s\n", filename.c_str());
            if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
                Update.printError(Serial);
            }
        }
        if(!Update.hasError()) {
            if(Update.write(data, len) != len) {
                Update.printError(Serial);
            }
        }
        if(final) {
            if(Update.end(true)) {
                Serial.printf("Update Success: %uB\n", index+len);
            } else {
                Update.printError(Serial);
            }
        }
    });

    web_server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
    });

    web_server.begin();

    snprintf(msg, sizeof(msg), "Serving HTTP on port %d", WEBSERVER_PORT);
    slog(msg, LOG_NOTICE);
}


bool handle_wifi() {
    static const uint32_t reconnectInterval = 10000;
    static const uint32_t reconnectLimit = 60;
    static uint32_t reconnectPrev = 0;
    static uint32_t reconnectCount = 0;

    bool currConnected = WiFi.isConnected();

    if (currConnected) {
        reconnectCount = 0;
    }
    else {
        uint32_t now = millis();
        if (reconnectCount == 0 || now - reconnectPrev > reconnectInterval) {
            WiFi.reconnect();
            reconnectCount++;
            if (reconnectCount > reconnectLimit) {
                Serial.println("Failed to reconnect WLAN, about to reset");
                for (int i = 0; i < 20; i++) {
                    digitalWrite(LED_PIN, (i & 1) ? LOW : HIGH);
                    delay(100);
                }
                Serial1.end();
                ESP.restart();
                while (true);
            }
            reconnectPrev = now;
        }
    }

    return currConnected;
}


bool check_ntptime() {
    static bool have_time = false;

    bool valid_time = time(0) > 1582230020;

    if (!have_time && valid_time) {
        have_time = true;
        time_t now = time(NULL);
        strftime(start_time, sizeof(start_time), "%F %T", localtime(&now));
        snprintf(msg, sizeof(msg), "Got valid time at %s", start_time);
        slog(msg, LOG_NOTICE);
    }

    return have_time;
}


void print_reset_reason(int core) {
    switch (rtc_get_reset_reason(core)) {
        case 1  : slog("Vbat power on reset"); break;
        case 3  : slog("Software reset digital core"); break;
        case 4  : slog("Legacy watch dog reset digital core"); break;
        case 5  : slog("Deep Sleep reset digital core"); break;
        case 6  : slog("Reset by SLC module, reset digital core"); break;
        case 7  : slog("Timer Group0 Watch dog reset digital core"); break;
        case 8  : slog("Timer Group1 Watch dog reset digital core"); break;
        case 9  : slog("RTC Watch dog Reset digital core"); break;
        case 10 : slog("Instrusion tested to reset CPU"); break;
        case 11 : slog("Time Group reset CPU"); break;
        case 12 : slog("Software reset CPU"); break;
        case 13 : slog("RTC Watch dog Reset CPU"); break;
        case 14 : slog("for APP CPU, reseted by PRO CPU"); break;
        case 15 : slog("Reset when the vdd voltage is not stable"); break;
        case 16 : slog("RTC Watch dog reset digital core and rtc module"); break;
        default : slog("Reset reason unknown");
    }
}


void handle_reboot() {
    if (shouldReboot) {
        uint32_t now = millis();
        if (now - shouldReboot > delayReboot) {
            Serial1.end();
            ESP.restart();
        }
    }
}


void recvd(const char *resp, void *ctx) {
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
            snprintf(msg, sizeof(msg), "Got config while power is %s", power ? "on" : "off");
            slog(msg);
        }
        else if( rxv.decode(resp, id, guard, origin) ) {
            name = rxv.report_name(id);
            value = rxv.report_value_string(id);
            if( !value && (id == 0x26 || id == 0x27 || id == 0xa2) ) {
                snprintf(buf, sizeof(buf), "raw %u", rxv.report_value(id));
                value = buf;
            }
            snprintf(msg, sizeof(msg), "Report x%02X: %s = %s", id, name ? name : "invalid", value ? value : "invalid");
            slog(msg);

            // Speaker A Relay also controls DSP mode
            if( id == 0x2E ) {
                if( rxv.report_value(id) == 0x00 ) {
                    rxvcomm.send(rxv.command("DSP_2chStereo"));
                }
                else {
                    rxvcomm.send(rxv.command("DSP_Adventure"));
                }
            }

            // Double vol up/down: first changes by 0.5dB, further by 1dB
            if( id == 0x26 && first_vol != 0 ) {
                if( first_vol > 0 ) {
                    rxvcomm.send(rxv.command("MainVolume_Up"));
                }
                else {
                    rxvcomm.send(rxv.command("MainVolume_Down"));
                }
                first_vol = 0;
            }
        }
        else if( rxv.decodeText(resp, id, text) ) {
            name = rxv.display_name(id);
            snprintf(msg, sizeof(msg), "Display x%02X: %s = %s", id, name ? name : "invalid", text);
            slog(msg);
        }
        else {
            snprintf(msg, sizeof(msg), "Ignoring unknown response '%s'", resp);
            slog(msg);
        }
    }
    else {
        slog("TIMEOUT");
        shouldReboot = millis();
        if( shouldReboot == 0 ) shouldReboot--;
        delayReboot = 60000;
    }
}


const char *pin_changed(bool is_high) {
    static bool bt_has_powered_on = false;

    bool power = false;
    switch( rxv.report_value(0x20) ) {
        case 1: case 2: case 4: case 5: power = true;
    };

    if( is_high ) {
        const char *cmd = rxv.command("Input_Cbl-Sat");
        if( !power ) {
            rxvcomm.send(rxv.command("MainZonePower_On"));
            bt_has_powered_on = true;
            return cmd;
        }
        else {
            bt_has_powered_on = false;
            rxvcomm.send(cmd);
        }
    }
    else {
        rxvcomm.send(rxv.command("Input_Dtv"));
        if( bt_has_powered_on ) {
            bt_has_powered_on = false;
            return rxv.command("MainZonePower_Off");
        }
    }

    return NULL;
}


void handle_pin() {
    static const uint32_t debounce_ms = 10;
    static const uint32_t powered_ms = 5000;

    static const char *cmd = NULL;
    static uint32_t power_at = 0;
    static uint32_t changed = 0;
    static bool was_high = false;

    uint32_t now = millis();
    bool is_high = digitalRead(BTN_PIN) != LOW;
    if( is_high != was_high ) {
        if( !changed ) {
            changed = (now - 1) | 1;
        }
        else {
            if( now - changed > debounce_ms ) {
                cmd = pin_changed(is_high);
                if( cmd ) power_at = (now - 1) | 1;
                was_high = is_high;
            }
        }
    }
    else {
        changed = 0;
        if( cmd && power_at && now - power_at > powered_ms ) {
            rxvcomm.send(cmd);
            power_at = 0;
            cmd = NULL;
        }
    }
}


void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    Serial.begin(BAUDRATE);
    Serial.println("\nStarting " PROGNAME " v" VERSION " " __DATE__ " " __TIME__);

    WiFi.setHostname(HOSTNAME);
    WiFi.mode(WIFI_STA);

    // Syslog setup
    syslog.server(SYSLOG_SERVER, SYSLOG_PORT);
    syslog.deviceHostname(HOSTNAME);
    syslog.appName("Joba1");
    syslog.defaultPriority(LOG_KERN);

    digitalWrite(LED_PIN, LOW);

    WiFiManager wm;
    wm.setHostname(HOSTNAME);
    wm.setConfigPortalTimeout(180);
    if (!wm.autoConnect(HOSTNAME, HOSTNAME)) {
        Serial.println("Failed to connect WLAN, about to reset");
        for (int i = 0; i < 20; i++) {
            digitalWrite(LED_PIN, (i & 1) ? HIGH : LOW);
            delay(100);
        }
        Serial1.end();
        ESP.restart();
        while (true);
    }

    digitalWrite(LED_PIN, HIGH);

    snprintf(msg, sizeof(msg), "%s WLAN IP is %s",
        HOSTNAME, WiFi.localIP().toString().c_str());
    slog(msg, LOG_NOTICE);

    if (MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        slog("mDNS started", LOG_NOTICE);
    }

    configTime(3600, 3600, NTP_SERVER);

    setup_webserver();

    print_reset_reason(0);
    print_reset_reason(1);

    Serial1.begin(9600, SERIAL_8N1, 16, 17);
    rxvcomm.on_recv(recvd, NULL);
    rxvcomm.send(rxv.command("Ready"));
    Serial.println("Sent Ready message");

    pinMode(BTN_PIN, INPUT_PULLDOWN);

    digitalWrite(LED_PIN, LOW);
}


void loop() {
    rxvcomm.handle();
    check_ntptime();
    handle_pin();
    handle_wifi();
    handle_reboot();
}
