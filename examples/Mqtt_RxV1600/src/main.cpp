/// Print RX-V1600 configuration and status changes as text

#include <Arduino.h>


#define LED_PIN LED_BUILTIN


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
bool shouldReboot = false;  // after updates...


// publish to mqtt broker
#include <PubSubClient.h>

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

// Standard web page
const char *main_page() {
    static const char fmt[] =
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
        "  <p>%s</p>\n"
        "  <table>\n"
        "   <tr><td>Startzeit</td><td>%s</td></tr>\n"
        "   <tr><td>Ladezeit</td><td>%s</td></tr>\n"
        "   <tr><td>Update URL</td><td><a href=\"http://" HOSTNAME "/update\">Update</a></td></tr>\n"
        "   <tr><td>Reset ESP</td><td><a href=\"http://" HOSTNAME "/reset\">Reset</a></td></tr>\n"
        "  </table>\n"
        " </body>\n"
        "</html>\n";
    static char page[sizeof(fmt) + 500] = "";
    static char curr_time[30];
    time_t now;
    time(&now);
    strftime(curr_time, sizeof(curr_time), "%FT%T", localtime(&now));
    snprintf(page, sizeof(page), fmt, web_msg, start_time, curr_time);
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
        ESP.restart();
    });

    // Index page
    web_server.on("/", [](AsyncWebServerRequest *request) { 
        request->send(200, "text/html", main_page());
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
        shouldReboot = !Update.hasError();
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
        strftime(start_time, sizeof(start_time), "%FT%T", localtime(&now));
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
        const char *cmd = rxv.command(msg);
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
    static const int32_t reboot_delay = 1000;  // if should_reboot wait this long in ms
    static uint32_t start = 0;                 // first detected should_reboot

    if (shouldReboot) {
        uint32_t now = millis();
        if (!start) {
            start = now;
        }
        else {
            if (now - start > reboot_delay) {
                ESP.restart();
            }
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
            if( name ) {
                snprintf(msg, sizeof(msg), MQTT_TOPIC "/status/%s", name);
                mqtt.publish(msg, value);
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
        delay(60000);
        ESP.restart();
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

    digitalWrite(LED_PIN, LOW);
}


void loop() {
    static const uint32_t help_delay = 20;
    static uint32_t prev_help = 0;

    rxvcomm.handle();
    handle_mqtt(check_ntptime());
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
