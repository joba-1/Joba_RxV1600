// Mqtt serial gateway for Yamaha RX-V1600 AV Receiver
// Also allow some RX-V1600 status changes via web page (e.g. Vol+/-)

#include <Arduino.h>


#define LED_PIN LED_BUILTIN
#define BTN_PIN 26


// Infrastructure
#include <Syslog.h>
#include <WiFiManager.h>


// Web Updater
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>


// Time sync
#include <time.h>


// Reset reason
#include "rom/rtc.h"


// Web status page and OTA updater
#define WEBSERVER_PORT 80

AsyncWebServer web_server(WEBSERVER_PORT);
uint32_t shouldReboot = 0;  // after updates or timeouts...
uint32_t delayReboot = 100;  // with a slight delay


// publish to mqtt broker
#include <PubSubClient.h>
#include <cstring>

WiFiClient wifiMqtt;
PubSubClient mqtt(wifiMqtt);


// Syslog
WiFiUDP logUDP;
Syslog syslog(logUDP, SYSLOG_PROTO_IETF);
char msg[512];  // one buffer for all syslog and json messages

char start_time[30];


#include <rxv1600.h>

RxV1600Comm rxvcomm(Serial1);
RxV1600 rxv;
auto help = RxV1600::end();


// define constant IsoDate as nicer variant of __DATE__ (from https://stackoverflow.com/a/64718070)
constexpr unsigned int compileYear = (__DATE__[7] - '0') * 1000 + (__DATE__[8] - '0') * 100 + (__DATE__[9] - '0') * 10 + (__DATE__[10] - '0');
constexpr unsigned int compileMonth = (__DATE__[0] == 'J') ? ((__DATE__[1] == 'a') ? 1 : ((__DATE__[2] == 'n') ? 6 : 7))    // Jan, Jun or Jul
                                : (__DATE__[0] == 'F') ? 2                                                              // Feb
                                : (__DATE__[0] == 'M') ? ((__DATE__[2] == 'r') ? 3 : 5)                                 // Mar or May
                                : (__DATE__[0] == 'A') ? ((__DATE__[2] == 'p') ? 4 : 8)                                 // Apr or Aug
                                : (__DATE__[0] == 'S') ? 9                                                              // Sep
                                : (__DATE__[0] == 'O') ? 10                                                             // Oct
                                : (__DATE__[0] == 'N') ? 11                                                             // Nov
                                : (__DATE__[0] == 'D') ? 12                                                             // Dec
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
        log_infos = false;  // log infos only for first 10 minutes
        slog("Switch off info level messages", LOG_NOTICE);
    }
}


void publish( const char *topic, const char *payload ) {
    if (mqtt.connected() && !mqtt.publish(topic, payload)) {
        slog("Mqtt publish failed");
    }
}


// check and report RSSI and BSSID changes
bool handle_wifi() {
    static const uint32_t reconnectInterval = 10000;  // try reconnect every 10s
    static const uint32_t reconnectLimit = 60;        // try restart after 10min
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
                while (true)
                    ;
            }
            reconnectPrev = now;
        }
    }

    return currConnected;
}


char web_msg[80] = "";  // main web page displays and then clears this
bool webpage_update = false;  // true if web page changed rxv1600 state -> refresh every 1s until rxv1600 responded
int first_vol = 0;  // double vol up or down web commands: first changes 0.5dB, following change 1dB

// Standard web page
const char *main_page() {
    static const char fmt[] =
        "<!doctype html>\n"
        "<html lang=\"en\">\n"
        " <head>\n"
        "  <style>\n"
        "   th, td { padding: 5px; }\n"
        "   input { width: 90%%; padding: 5px; margin: 5px; }\n"
        "   slider { height: 50px; margin: 5px; }\n"
        "  </style>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "  <link href=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAgMAAABinRfyAAAADFBMVEUqYbutnpTMuq/70SQgIef5AAAAVUlEQVQIHWOAAPkvDAyM3+Y7MLA7NV5g4GVqKGCQYWowYTBhapBhMGB04GE4/0X+M8Pxi+6XGS67XzzO8FH+iz/Dl/q/8gx/2S/UM/y/wP6f4T8QAAB3Bx3jhPJqfQAAAABJRU5ErkJggg==\" rel=\"icon\" type=\"image/x-icon\" />\n"
        "  <title>" PROGNAME " v" VERSION "</title>\n%s"
        " </head>\n"
        " <body>\n"
        "  <h1>" PROGNAME " v" VERSION "</h1>\n"
        "  <p>%s</p>\n"
        "  <table>\n"
        "   <tr>\n"
        "    <td><form method=\"POST\" action=\"/power-on\"><input type=\"submit\" value=\"Main On\"></form></td>\n"
        "    <td><form method=\"POST\" action=\"/power-off\"><input type=\"submit\" value=\"Main Off\"></form></td>\n"
        "    <td>&nbsp;</td><td>%8.8s</td>\n"
        "   </tr>\n"
        "   <tr>\n"
        "    <td><form method=\"POST\" action=\"/tv\"><input type=\"submit\" style=\"width: 90%%;\" value=\"TV\"></form></td>\n"
        "    <td><form method=\"POST\" action=\"/bt\"><input type=\"submit\" value=\"Bluetooth\"></form></td>\n"
        "    <td>&nbsp;</td><td>%s</td>\n"
        "   </tr>\n"
        "   <tr>\n"
        "    <td><form method=\"POST\" action=\"/a-on\"><input type=\"submit\" value=\"A On\"></form></td>\n"
        "    <td><form method=\"POST\" action=\"/a-off\"><input type=\"submit\" value=\"A Off\"></form></td>\n"
        "    <td>&nbsp;</td><td>%s</td>\n"
        "   </tr>\n"
        "   <tr>\n"
        "    <td><form method=\"POST\" action=\"/b-on\"><input type=\"submit\" value=\"B On\"></form></td>\n"
        "    <td><form method=\"POST\" action=\"/b-off\"><input type=\"submit\" value=\"B Off\"></form></td>\n"
        "    <td>&nbsp;</td><td>%s</td>\n"
        "   </tr>\n"
        "   <tr>\n"
        "    <td><form method=\"POST\" action=\"/mute-on\"><input type=\"submit\" value=\"Mute\"></form></td>\n"
        "    <td><form method=\"POST\" action=\"/mute-off\"><input type=\"submit\" value=\"Unmute\"></form></td>\n"
        "    <td>&nbsp;</td><td>%s</td>\n"
        "   </tr>\n"
        "   <tr>\n"
        "    <td><form method=\"POST\" action=\"/vol-down\"><input type=\"submit\" value=\"Vol -\"></form></td>\n"
        "    <td><form method=\"POST\" action=\"/vol-up\"><input type=\"submit\" value=\"Vol +\"></form></td>\n"
        "    <td>&nbsp;</td><td id=\"value\">%s</td>\n"
        "   </tr>\n"
        "   <tr>\n"
        "    <td colspan=\"4\"><input id=\"slider\" style=\"width: 90%%; height: 10px;\" type=\"range\" min=\"-40\" max=\"-10\" value=\"%d\"></td>\n"
        "   </tr>\n"
        "   <tr><td>Ladezeit</td><td colspan=\"2\">%s</td><td><form method=\"GET\" action=\"/\"><input type=\"submit\" value=\"Reload\"></form></td></tr>\n"
        "   <tr><td>Startzeit</td><td colspan=\"2\">%s</td><td><form method=\"POST\" action=\"/reset\"><input type=\"submit\" value=\"Reset\"></form></td></tr>\n"
        "   <tr><td>Compiled</td><td colspan=\"2\">%s " __TIME__ "</td><td><form method=\"GET\" action=\"/update\"><input type=\"submit\" value=\"Update\"></form></td></tr>\n"
        "   <tr><td><small>Author</small></td><td><small>Joachim Banzhaf</small></td><td><a href=\"https://github.com/joba-1/Joba_RxV1600\" target=\"_blank\"><small>Github</small></a></td></tr>\n"
        "  </table>\n"
        "  <script>\n"
        "   function ajax(url, data, on_response) {\n"
        "    q = new XMLHttpRequest();\n"
        "    q.onreadystatechange = function() {\n"
        "     if (q.readyState == 4) {\n"
        "      on_response();\n"
        "     }\n"
        "    }\n"
        "    q.open('POST', url);\n"
        "    q.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');\n"
        "    q.send(data);\n"
        "   }\n"        
        "   function sliderCallback(sliderId, valueId, callbackUrl) {\n"
        "    var slider = document.getElementById(sliderId);\n"
        "    var value = document.getElementById(valueId);\n"
        "    slider.oninput = function() {\n"
        "     value.innerHTML = this.value + '.0 dB';\n"
        "     if (!this.hasAttribute('data-busy')) {\n"
        "      this.setAttribute('data-busy', '');\n"
        "      ajax(callbackUrl, sliderId + '=' + this.value, function() {\n"
        "       slider.removeAttribute('data-busy');\n" 
        "      })"
        "     }\n"
        "    }\n"
        "    slider.onchange = function() {\n"
        "      ajax(callbackUrl, sliderId + '=' + this.value, function() {\n"
        "       slider.removeAttribute('data-busy');\n" 
        "      })"
        "    }\n"
        "   }\n"
        "   sliderCallback('slider', 'value', '/vol');\n"
        "  </script>\n"
        " </body>\n"
        "</html>\n";
    static char page[sizeof(fmt) + 500] = "";
    const char *power = rxv.report_value_string(0x20);
    const char *input = rxv.report_value_string(0x21);
    const char *pch = input;
    while (*pch && *pch != ' ') pch++;
    if (*pch == ' ') input = ++pch;  // skip first word (MultiChannel)
    const char *speaker_a = rxv.report_value_string(0x2e);
    const char *speaker_b = rxv.report_value_string(0x2f);
    const char *muted = rxv.report_value_string(0x23);
    const char *volume = rxv.report_value_string(0x26);
    int vol_db = volume ? atoi(volume) : 0;
    const char *refresh = webpage_update ? "  <meta http-equiv=\"refresh\" content=\"1; url=/\"> \n" : "";
    static char curr_time[30];
    time_t now;
    time(&now);
    strftime(curr_time, sizeof(curr_time), "%F %T", localtime(&now));
    snprintf(page, sizeof(page), fmt, refresh, web_msg, 
        power ? power : "unknown",
        input ? input : "unknown",
        speaker_a ? speaker_a : "unknown",
        speaker_b ? speaker_b : "unknown",
        muted ? muted : "unknown",
        volume ? volume : "unknown",
        vol_db, curr_time, start_time, IsoDate);
    *web_msg = '\0';
    return page;
}


// Define web pages for update, reset or for event infos
void setup_webserver() {
    // Call this page to reset the ESP
    web_server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        slog("RESET ESP32", LOG_NOTICE);
        request->send(200, "text/html",
                        "<html>\n"
                        " <head>\n"
                        "  <title>" PROGNAME " v" VERSION "</title>\n"
                        "  <meta http-equiv=\"refresh\" content=\"7; url=/\"> \n"
                        " </head>\n"
                        " <body>Resetting...</body>\n"
                        "</html>\n");
        delay(200);
        Serial1.end();
        ESP.restart();
    });

    // Index page
    web_server.on("/", [](AsyncWebServerRequest *request) { 
        request->send(200, "text/html", main_page());
    });

    // Power on
    web_server.on("/power-on", HTTP_POST, [](AsyncWebServerRequest *request) { 
        publish(MQTT_TOPIC "/cmd", "MainZonePower_On");
        webpage_update = true;
        request->redirect("/"); 
    });

    // Power off
    web_server.on("/power-off", HTTP_POST, [](AsyncWebServerRequest *request) { 
        publish(MQTT_TOPIC "/cmd", "MainZonePower_Off");
        webpage_update = true;
        request->redirect("/"); 
    });

    // Speaker A on
    web_server.on("/a-on", HTTP_POST, [](AsyncWebServerRequest *request) { 
        publish(MQTT_TOPIC "/cmd", "SpeakerRelayA_On");
        webpage_update = true;
        request->redirect("/"); 
    });

    // Speaker A off
    web_server.on("/a-off", HTTP_POST, [](AsyncWebServerRequest *request) { 
        publish(MQTT_TOPIC "/cmd", "SpeakerRelayA_Off");
        webpage_update = true;
        request->redirect("/"); 
    });

    // Speaker B on
    web_server.on("/b-on", HTTP_POST, [](AsyncWebServerRequest *request) { 
        webpage_update = true;
        publish(MQTT_TOPIC "/cmd", "SpeakerRelayB_On");
        request->redirect("/"); 
    });

    // Speaker B off
    web_server.on("/b-off", HTTP_POST, [](AsyncWebServerRequest *request) { 
        webpage_update = true;
        publish(MQTT_TOPIC "/cmd", "SpeakerRelayB_Off");
        request->redirect("/"); 
    });

    // Mute on
    web_server.on("/mute-on", HTTP_POST, [](AsyncWebServerRequest *request) { 
        webpage_update = true;
        publish(MQTT_TOPIC "/cmd", "Mute_On");
        request->redirect("/"); 
    });

    // Mute off
    web_server.on("/mute-off", HTTP_POST, [](AsyncWebServerRequest *request) { 
        webpage_update = true;
        publish(MQTT_TOPIC "/cmd", "Mute_Off");
        request->redirect("/"); 
    });

    // Volume up
    web_server.on("/vol-up", HTTP_POST, [](AsyncWebServerRequest *request) { 
        webpage_update = true;
        first_vol = 1;
        publish(MQTT_TOPIC "/cmd", "MainVolume_Up");
        request->redirect("/"); 
    });

    // Volume down
    web_server.on("/vol-down", HTTP_POST, [](AsyncWebServerRequest *request) { 
        webpage_update = true;
        first_vol = -1;
        publish(MQTT_TOPIC "/cmd", "MainVolume_Down");
        request->redirect("/"); 
    });

    // Set volume
    web_server.on("/vol", HTTP_POST, [](AsyncWebServerRequest *request) { 
        webpage_update = true;
        String arg = request->arg("slider");
        if (!arg.isEmpty()) {
            int volume = atoi(arg.c_str());
            char cmd[30];
            snprintf(cmd, sizeof(cmd), "MainVolumeSet,%d", volume * 2 + 199); 
            // slog("Set volume to %d", volume);
            publish(MQTT_TOPIC "/cmd", cmd);
        }
        request->redirect("/"); 
    });

    // TV input
    web_server.on("/tv", HTTP_POST, [](AsyncWebServerRequest *request) { 
        webpage_update = true;
        publish(MQTT_TOPIC "/cmd", "Input_Dtv");
        request->redirect("/"); 
    });

    // Bluetooth input
    web_server.on("/bt", HTTP_POST, [](AsyncWebServerRequest *request) { 
        webpage_update = true;
        publish(MQTT_TOPIC "/cmd", "Input_Cbl-Sat");
        request->redirect("/"); 
    });

    web_server.on("/wipe", HTTP_POST, [](AsyncWebServerRequest *request) {
        WiFiManager wm;
        wm.resetSettings();
        slog("Wipe WLAN config and reset ESP32", LOG_NOTICE);
        request->send(200, "text/html",
                        "<html>\n"
                        " <head>\n"
                        "  <title>" PROGNAME " v" VERSION "</title>\n"
                        "  <meta http-equiv=\"refresh\" content=\"7; url=/\"> \n"
                        " </head>\n"
                        " <body>Wipe WLAN config. Connect to AP '" HOSTNAME "' and connect to http://192.168.4.1</body>\n"
                        "</html>\n");
        delay(200);
        Serial1.end();
        ESP.restart();
    });

    // Firmware Update Form
    web_server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        static const char page[] =
            "<!doctype html>\n"
            "<html lang=\"en\">\n"
            " <head>\n"
            "  <meta charset=\"utf-8\">\n"
            "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
            "  <link href=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQAgMAAABinRfyAAAADFBMVEUqYbutnpTMuq/70SQgIef5AAAAVUlEQVQIHWOAAPkvDAyM3+Y7MLA7NV5g4GVqKGCQYWowYTBhapBhMGB04GE4/0X+M8Pxi+6XGS67XzzO8FH+iz/Dl/q/8gx/2S/UM/y/wP6f4T8QAAB3Bx3jhPJqfQAAAABJRU5ErkJggg==\" rel=\"icon\" type=\"image/x-icon\" />\n"
            "  <title>" PROGNAME " v" VERSION "</title>\n"
            " </head>\n"
            " <body>\n"
            "  <h1>" PROGNAME " v" VERSION "</h1>\n"
            "  <form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\">\n"
            "    <input type=\"file\" name=\"update\">\n"
            "    <input type=\"submit\" value=\"Update\">\n"
            "  </form>\n"
            " </body>\n"
            "</html>\n";
 
        request->send(200, "text/html", page);
    });

    web_server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        if( !Update.hasError() ) {
            shouldReboot = millis();
            if( shouldReboot == 0 ) {
                shouldReboot--;
            }
            delayReboot = 5000;
        }
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
    },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if(!index){
            Serial.printf("Update Start: %s\n", filename.c_str());
            if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
                Update.printError(Serial);
                snprintf(web_msg, sizeof(web_msg), "%s", Update.errorString());
                request->redirect("/");  
            }
        }
        if(!Update.hasError()){
            if(Update.write(data, len) != len){
                Update.printError(Serial);
                snprintf(web_msg, sizeof(web_msg), "%s", Update.errorString());
                request->redirect("/");  
            }
        }
        if(final){
            if(Update.end(true)){
                Serial.printf("Update Success: %uB\n", index+len);
                snprintf(web_msg, sizeof(web_msg), "Update Success: %u Bytes", index+len);
            } else {
                Update.printError(Serial);
                snprintf(web_msg, sizeof(web_msg), "%s", Update.errorString());
            }
            request->redirect("/"); 
        }
    });

    // Catch all page
    web_server.onNotFound( [](AsyncWebServerRequest *request) { 
        snprintf(web_msg, sizeof(web_msg), "%s", "<h2>page not found</h2>\n");
        request->send(404, "text/html", main_page()); 
    });

    web_server.begin();

    snprintf(msg, sizeof(msg), "Serving HTTP on port %d", WEBSERVER_PORT);
    slog(msg, LOG_NOTICE);
}


// check ntp status
// return true if time is valid
bool check_ntptime() {
    static bool have_time = false;

    bool valid_time = time(0) > 1582230020;

    if (!have_time && valid_time) {
        have_time = true;
        time_t now = time(NULL);
        strftime(start_time, sizeof(start_time), "%F %T", localtime(&now));
        snprintf(msg, sizeof(msg), "Got valid time at %s", start_time);
        slog(msg, LOG_NOTICE);
        if (mqtt.connected()) {
            publish(MQTT_TOPIC "/status/StartTime", start_time);
        }
    }

    return have_time;
}


// Reset reason can be quite useful...
// Messages from arduino core example
void print_reset_reason(int core) {
  switch (rtc_get_reset_reason(core)) {
    case 1  : slog("Vbat power on reset");break;
    case 3  : slog("Software reset digital core");break;
    case 4  : slog("Legacy watch dog reset digital core");break;
    case 5  : slog("Deep Sleep reset digital core");break;
    case 6  : slog("Reset by SLC module, reset digital core");break;
    case 7  : slog("Timer Group0 Watch dog reset digital core");break;
    case 8  : slog("Timer Group1 Watch dog reset digital core");break;
    case 9  : slog("RTC Watch dog Reset digital core");break;
    case 10 : slog("Instrusion tested to reset CPU");break;
    case 11 : slog("Time Group reset CPU");break;
    case 12 : slog("Software reset CPU");break;
    case 13 : slog("RTC Watch dog Reset CPU");break;
    case 14 : slog("for APP CPU, reseted by PRO CPU");break;
    case 15 : slog("Reset when the vdd voltage is not stable");break;
    case 16 : slog("RTC Watch dog reset digital core and rtc module");break;
    default : slog("Reset reason unknown");
  }
}


// Called on incoming mqtt messages
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    if (strcasecmp(MQTT_TOPIC "/cmd", topic) == 0) {
        snprintf(msg, sizeof(msg), "%.*s", length, (char *)payload);

        char *cmd_name = std::strtok(msg, ",");
        char *cmd_value = std::strtok(NULL, ",");
        bool cmd_excess = (std::strtok(NULL, ",") != NULL);
        const char *cmd = NULL;

        if( cmd_value ) {
            if( cmd_excess ) {
                snprintf(msg, sizeof(msg), "Discarding mqtt payload '%.*s': excess argument", length, (char *)payload);
                slog(msg);
                return;
            }
            size_t value_len = strlen(cmd_value);
            char *endp;
            unsigned long value = strtoul(cmd_value, &endp, 0);
            if( endp != &cmd_value[value_len] ) {
                snprintf(msg, sizeof(msg), "Discarding mqtt payload '%.*s': excess value", length, (char *)payload);
                slog(msg);
                return;
            }
            else if( value > 0xff ) {
                snprintf(msg, sizeof(msg), "Discarding mqtt payload '%.*s': value overflow", length, (char *)payload);
                slog(msg);
                return;
            }
            else {
                cmd = rxv.command_value(cmd_name, value);
            }
        }
        else {
            cmd = rxv.command(msg);
        }

        if( cmd ) {
            if( !rxvcomm.send(cmd) ) {
                snprintf(msg, sizeof(msg), "Discarding mqtt command '%s'", cmd);
                slog(msg);
            }
        }
        else {
            if( strcasecmp("help", msg) == 0 ) {
                help = RxV1600::begin();
            }
            else if( strcasecmp("reset", msg) == 0 ) {
                slog("RESET");
                delay(100);
                Serial1.end();
                ESP.restart();
            }
            else {
                snprintf(msg, sizeof(msg), "Ignore unknown mqtt payload '%.*s'", length, (char *)payload);
                slog(msg, LOG_WARNING);
            }
        }
    }
    else {
        snprintf(msg, sizeof(msg), "Ignore mqtt topic %s: '%.*s'", topic, length, (char *)payload);
        slog(msg, LOG_WARNING);
    }
}


bool handle_mqtt( bool time_valid ) {
    static const int32_t interval = 5000;  // if disconnected try reconnect this often in ms
    static uint32_t prev = -interval;      // first connect attempt without delay

    if (mqtt.connected()) {
        mqtt.loop();
        return true;
    }

    uint32_t now = millis();
    if (now - prev > interval) {
        prev = now;

        if (mqtt.connect(HOSTNAME, MQTT_TOPIC "/status/LWT", 0, true, "Offline")
            && mqtt.publish(MQTT_TOPIC "/status/LWT", "Online", true)
            && mqtt.publish(MQTT_TOPIC "/status/Hostname", HOSTNAME)
            && mqtt.publish(MQTT_TOPIC "/status/Version", VERSION)
            && (!time_valid || mqtt.publish(MQTT_TOPIC "/status/StartTime", start_time))
            && mqtt.subscribe(MQTT_TOPIC "/cmd")) {
            snprintf(msg, sizeof(msg), "Connected to MQTT broker %s:%d using topic %s", MQTT_SERVER, MQTT_PORT, MQTT_TOPIC);
            slog(msg, LOG_NOTICE);
            return true;
        }

        int error = mqtt.state();
        mqtt.disconnect();
        snprintf(msg, sizeof(msg), "Connect to MQTT broker %s:%d failed with code %d", MQTT_SERVER, MQTT_PORT, error);
        slog(msg, LOG_ERR);
    }

    return false;
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


void recvd( const char *resp, void *ctx ) {
    bool power;
    uint8_t id;
    RxV1600::guard_t guard;
    RxV1600::origin_t origin;
    char text[9];
    const char *name;
    const char *value;
    char buf[10];

    webpage_update = false;

    if( resp ) {
        if( rxv.decodeConfig(resp, power) ) {
            snprintf(msg, sizeof(msg), "Got config while power is %s", power ? "on" : "off");
            slog(msg);
            for( unsigned i=0; i<=0xff; i++ ) {
                name = rxv.report_name(i);
                value = rxv.report_value_string(i);
                if( name || value ) {
                    snprintf(msg, sizeof(msg), "Config x%02X: %s = %s", i, name ? name : "invalid", value ? value : "invalid");
                    slog(msg);
                    if( name ) {
                        snprintf(msg, sizeof(msg), MQTT_TOPIC "/status/%s", name);
                        mqtt.publish(msg, value);
                    }
                }
            }
        }
        else if( rxv.decode(resp, id, guard, origin) ) {
            if( guard != RxV1600::G_NONE ) {
                snprintf(msg, sizeof(msg), "Guard for report x%02X is %s", id, guard == RxV1600::G_SETTINGS ? "Settings" : "System");
                slog(msg);
            }

            name = rxv.report_name(id);
            value = rxv.report_value_string(id);
            if( !value && (id == 0x26 || id == 0x27 || id == 0xa2) ) {
                snprintf(buf, sizeof(buf), "raw %u", rxv.report_value(id));
                value = buf;
            }
            snprintf(msg, sizeof(msg), "Report x%02X: %s = %s", id, name ? name : "invalid", value ? value : "invalid");
            slog(msg);

            // "Speaker A Relay" only switches front left and right
            // Also silence center and back by switching to 2ch stereo effect
            if( id == 0x2E ) {
                // Speaker A Relais
                if( rxv.report_value(id) == 0x00 ) {  // Off
                    rxvcomm.send(rxv.command("DSP_2chStereo"));
                }
                else {  // On
                    rxvcomm.send(rxv.command("DSP_Adventure"));
                }
            }

            // Vol change is slow, and first request since 1s only reports current value
            // Double request: so first changes by 0.5dB and further changes by 1dB 
            if( id == 0x26 && first_vol != 0 ) {
                if( first_vol > 0 ) {
                    rxvcomm.send(rxv.command("MainVolume_Up"));
                }
                else {
                    rxvcomm.send(rxv.command("MainVolume_Down"));
                }
                first_vol = 0;
            }
            else if( name ) {
                snprintf(msg, sizeof(msg), MQTT_TOPIC "/status/%s", name);
                publish(msg, value);
            }
        }
        else if( rxv.decodeText(resp, id, text) ) {
            name = rxv.display_name(id);
            snprintf(msg, sizeof(msg), "Report x%02X: %s = %s", id, name ? name : "invalid", text);
            slog(msg);
            if( name ) {
                snprintf(msg, sizeof(msg), MQTT_TOPIC "/status/%s", name);
                mqtt.publish(msg, text);
            }
        }
        else {
            snprintf(msg, sizeof(msg), "Ignoring unknown response '%s'", resp);
            slog(msg);
        }
    }
    else {
        snprintf(msg, sizeof(msg), "TIMEOUT");
        slog(msg);
        shouldReboot = millis();
        if( shouldReboot == 0 ) {
            shouldReboot--;
        }
        delayReboot = 60000;
    }
}


const char *pin_changed( bool is_high ) {
    static bool bt_has_powered_on = false;

    bool power = false;
    switch( rxv.report_value(0x20) ) {
        case 1:
        case 2:
        case 4:
        case 5:
            power = true;
    };

    if( is_high ) {
        // My pico-w signals a BT connection. Switch receiver to its bluetooth input port
        const char *cmd = rxv.command("Input_Cbl-Sat");
        if( !power ) {
            rxvcomm.send(rxv.command("MainZonePower_On"));
            bt_has_powered_on = true;
            return cmd;  // call this later...
        }
        else {
            bt_has_powered_on = false;
            rxvcomm.send(cmd);
        }
    }
    else {
        // My pico-w signals BT connection lost. Switch receiver to its tv input port
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
        // pin state has changed
        if( !changed ) {
            // first time we notice the change: start debounce timer
            changed = (now - 1) | 1;
        }
        else {
            // waiting for debounce of pin state
            if( now - changed > debounce_ms ) {
                // debounced: react on pin change
                cmd = pin_changed(is_high);
                if( cmd ) {
                    power_at = (now - 1) | 1;
                }
                was_high = is_high;  // remember last known state
            }
        }
    }
    else {
        // pin state not changed or changed back within debounce period.
        changed = 0;  // reset debounce timer
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

    String host(HOSTNAME);
    host.toLowerCase();
    WiFi.hostname(host.c_str());
    WiFi.mode(WIFI_STA);

    // Syslog setup
    syslog.server(SYSLOG_SERVER, SYSLOG_PORT);
    syslog.deviceHostname(WiFi.getHostname());
    syslog.appName("Joba1");
    syslog.defaultPriority(LOG_KERN);

    digitalWrite(LED_PIN, LOW);

    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    if (!wm.autoConnect(WiFi.getHostname(), WiFi.getHostname())) {
        Serial.println("Failed to connect WLAN, about to reset");
        for (int i = 0; i < 20; i++) {
            digitalWrite(LED_PIN, (i & 1) ? HIGH : LOW);
            delay(100);
        }
        Serial1.end();
        ESP.restart();
        while (true)
            ;
    }

    digitalWrite(LED_PIN, HIGH);

    char msg[80];
    snprintf(msg, sizeof(msg), "%s WLAN IP is %s", 
        HOSTNAME, WiFi.localIP().toString().c_str());
    slog(msg, LOG_NOTICE);

    configTime(3600, 3600, NTP_SERVER);  // MEZ/MESZ

    setup_webserver();

    mqtt.setServer(MQTT_SERVER, MQTT_PORT);
    mqtt.setCallback(mqtt_callback);

    print_reset_reason(0);
    print_reset_reason(1);

    Serial1.begin(9600, SERIAL_8N1, 16, 17);  // chosen arbitrary rx, tx pins
    rxvcomm.on_recv(recvd, NULL);
    // Send ready to RX-V1600 to receive config
    rxvcomm.send(rxv.command("Ready"));
    Serial.println("Sent Ready message");

    pinMode(BTN_PIN, INPUT_PULLDOWN);  // on toggle switch rxv1600 input 

    digitalWrite(LED_PIN, LOW);
}


void loop() {
    static const uint32_t help_delay = 10;
    static uint32_t prev_help = 0;

    rxvcomm.handle();
    handle_mqtt(check_ntptime());
    handle_pin();
    handle_wifi();
    handle_reboot();
    if( help != RxV1600::end() ) {
        uint32_t now = millis();
        if( now - prev_help > help_delay ) {
            prev_help = now;
            publish(MQTT_TOPIC "/help", help->first);
            slog(help->first);
            help++;
        }
    }
}
