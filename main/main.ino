#define ENABLE_GxEPD2_GFX 0

#include <WiFi.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <time.h>

#define DC_PIN   (4)
#define RS_PIN   (5)
#define BUSY_PIN (6)

#define US_PER_SEC (1000000)
#define SECONDS_PER_MIN (60)
#define MIN_PER_HOUR SECONDS_PER_MIN
#define TIME_SYNC_INTERVAL (12 * MIN_PER_HOUR)

#define MAX_CONNECTION_ATTEMPTS (20)

const char *ssid = "<wifi ssid>";
const char *password = "<wifi password>";

const char *ntpServer = "time.nist.gov";
const char *timeZone = "MST7MDT,M3.2.0,M11.1.0";

bool wifiConnected = false;

unsigned int timeSyncIntervalCounter = 0;

GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(GxEPD2_290_BS(SS, DC_PIN, RS_PIN, BUSY_PIN));

void setTimezone(){
    setenv("TZ",timeZone, 1);
    tzset();
}

void initTime() {
    struct tm timeinfo;

    configTime(0, 0, ntpServer);

    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    setTimezone();
}

void initDisplay() {
    Serial.println("Initializing display...");
    display.init(115200, true, 50, false);
    Serial.println("Display initialized. Configuring...");

    display.setRotation(3);
    display.clearScreen();
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
}

void initWifi() {
    display.setFont(&FreeSans18pt7b);
    display.setCursor(10, 30);
    display.setTextSize(1);
    display.print("Connecting...");
    display.display(true);

    Serial.println("Connecting to network...");
    WiFi.begin(ssid, password);

    wl_status_t wifiStatus;

    int attempt = 0;
    while (wifiStatus != WL_CONNECTED && attempt < MAX_CONNECTION_ATTEMPTS) {
        wifiStatus = WiFi.status();
        attempt++;
        Serial.print(".");
        delay(1000);
    }

    display.fillScreen(GxEPD_WHITE);
    display.setCursor(10,30);
    display.setTextSize(1);

    if (wifiStatus == WL_CONNECTED) {
        wifiConnected = true;
        display.print("Connected.");
        Serial.println("\nSuccessfully connected to network");
    } else {
        display.print("Failed to connect.");
        Serial.println("Failed to connect to network");
    }

    display.display(true);
}

bool reconnectWifiIfDisconnected() {
    wl_status_t wifiStatus;

    wifiStatus = WiFi.status();

    if (wifiStatus == WL_CONNECTED) return true;

    Serial.println("Reconnecting to network...");
    WiFi.reconnect();

    int attempt = 0;
    while (wifiStatus != WL_CONNECTED && attempt < MAX_CONNECTION_ATTEMPTS) {
        wifiStatus = WiFi.status();
        attempt++;
        Serial.print(".");
        delay(1000);
    }

    if (wifiStatus != WL_CONNECTED) {
        Serial.println("Failed to reconnect.");
        WiFi.disconnect(); // fully disconnect
    }

    return wifiStatus == WL_CONNECTED;
}

bool retrieveTime(struct tm *timeinfo) {
    timeSyncIntervalCounter++;

    if (timeSyncIntervalCounter >= TIME_SYNC_INTERVAL) {
        bool result = reconnectWifiIfDisconnected();

        // we failed to connect to the network; do nothing
        if (!result) return false;

        // we successfully connected; reset the interval
        timeSyncIntervalCounter = 0;
        initTime();
    }

    bool timeRetrievalSuccess = getLocalTime(timeinfo);
    if (!timeRetrievalSuccess) {
        Serial.println("Failed to obtain time");
    }

    return timeRetrievalSuccess;
}

void setup() {
    Serial.begin(115200);

    initDisplay();
    initWifi();

    if (!wifiConnected) return;

    initTime();
}

void loop() {
    if (!wifiConnected) return;

    struct tm timeinfo;

    retrieveTime(&timeinfo);
    displayTime(&timeinfo);

    int seconds_to_sleep = SECONDS_PER_MIN - timeinfo.tm_sec;
    esp_sleep_enable_timer_wakeup(US_PER_SEC * seconds_to_sleep);
    esp_light_sleep_start();
}

void displayTime(struct tm *timeinfo) {
    char amPmString[4];
    char timeString[8];
    char dateString[16];

    strftime(timeString, sizeof(timeString), "%I:%M", timeinfo);
    strftime(dateString, sizeof(dateString), "%D", timeinfo);
    strftime(amPmString, sizeof(amPmString), "%p", timeinfo);

    Serial.println(timeString);

    display.fillScreen(GxEPD_WHITE);

    display.setCursor(20, 85);
    display.setTextSize(3);
    display.setFont(&FreeSans18pt7b);
    display.print(timeString);

    display.setTextSize(1);
    display.setFont(&FreeSans12pt7b);

    display.setCursor(25, 115);
    display.print(dateString);

    display.setCursor(235, 115);
    display.print(amPmString);

    display.display(true);
}
