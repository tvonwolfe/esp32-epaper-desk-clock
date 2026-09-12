#define ENABLE_GxEPD2_GFX 0

#include <WiFi.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <time.h>

extern "C" {
#include "esp_sntp.h"
}

#define DC_PIN (4)
#define RS_PIN (5)
#define BUSY_PIN (6)

#define US_PER_SEC (1000000)
#define SECONDS_PER_MIN (60)

#define MAX_CONNECTION_ATTEMPTS (20)

typedef GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display_t;

const char *ip = WIFI_IPADDR;
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

const char *ntpServer = NTPSERV;
const char *timeZone = TZ;

struct device_state_t {
    int wifiConnectionAttemptFailures;
    bool performedInitialBoot;
    struct tm timeinfo;

    struct tm *getCurrentTime() {
        if (!getLocalTime(&timeinfo)) {
            Serial.println("Failed to obtain time");
        }

        return &timeinfo;
    }
};

RTC_DATA_ATTR display_t display(GxEPD2_290_BS(SS, DC_PIN, RS_PIN, BUSY_PIN));
RTC_DATA_ATTR device_state_t device_state = {
    .wifiConnectionAttemptFailures = 0,
    .performedInitialBoot = false,
};

void setTimezone() {
    setenv("TZ", timeZone, 1);
    tzset();
}

void initTime() {
    configTime(0, 0, ntpServer);
    setTimezone();
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_time_sync_notification_cb([](struct timeval *tv) {
        WiFi.disconnect();
    });
}

bool isTopOfHour(device_state_t *device_state) {
    return device_state->getCurrentTime()->tm_min == 0;
}

void initDisplay(device_state_t *device_state, display_t *display) {
    bool shouldFullyInit = !device_state->performedInitialBoot || isTopOfHour(device_state);
    display->init(115200, shouldFullyInit, 50, false);

    display->setRotation(3);
    display->setTextColor(GxEPD_BLACK);

    if (!device_state->performedInitialBoot) {
        display->clearScreen();
        display->fillScreen(GxEPD_WHITE);
    }
}

bool connectWifi(device_state_t *device_state) {
    Serial.println("Connecting to network...");
    WiFi.config(ip);
    WiFi.begin(ssid, password);

    wl_status_t wifiStatus;
    int attempt = 0;
    bool connected;
    while (!(connected = (wifiStatus == WL_CONNECTED)) && attempt < MAX_CONNECTION_ATTEMPTS) {
        wifiStatus = WiFi.status();
        attempt++;
        Serial.print(".");
        delay(1000);
    }

    if (connected) {
        Serial.println("\nSuccessfully connected to network");
    } else if (device_state->performedInitialBoot) {
        device_state->wifiConnectionAttemptFailures++;
        Serial.println("\nFailed to re-connect to network");
    }

    return connected;
}

bool performInitialBoot(device_state_t *device_state, display_t *display) {
    display->setFont(&FreeSans18pt7b);
    display->setCursor(10, 30);
    display->setTextSize(1);
    display->print("Connecting...");
    display->display(true);

    bool connected = connectWifi(device_state);

    display->fillScreen(GxEPD_WHITE);
    display->setCursor(10, 30);
    display->setTextSize(1);

    if (connected) {
        display->print("Connected.");
        initTime();
    } else {
        display->print("Failed to connect.");
        Serial.println("Failed to connect to network");
    }

    device_state->performedInitialBoot = true;
    display->display(true);

    return connected;
}

void displayTime(device_state_t *device_state, display_t *display) {
    struct tm *timeinfo = device_state->getCurrentTime();
    char amPmString[4];
    char timeString[8];
    char dateString[16];

    strftime(timeString, sizeof(timeString), "%I:%M", timeinfo);
    strftime(dateString, sizeof(dateString), "%D", timeinfo);
    strftime(amPmString, sizeof(amPmString), "%p", timeinfo);

    Serial.println(timeString);

    display->fillScreen(GxEPD_WHITE);

    display->setCursor(20, 83);
    display->setTextSize(3);
    display->setFont(&FreeSans18pt7b);
    display->print(timeString);

    display->setTextSize(1);
    display->setFont(&FreeSans12pt7b);

    display->setCursor(25, 115);
    display->print(dateString);

    display->setCursor(235, 115);
    display->print(amPmString);

    display->display(true);
    display->hibernate();
}

void performTimeSync(device_state_t *device_state) {
    connectWifi(device_state);
    initTime();
}

void setup() {
    initDisplay(&device_state, &display);

    if (!device_state.performedInitialBoot) {
        bool wifiSuccess = performInitialBoot(&device_state, &display);

        if (!wifiSuccess) return;
    }

    setTimezone();
    displayTime(&device_state, &display);

    if (isTopOfHour(&device_state)) performTimeSync(&device_state);

    int seconds_to_sleep = SECONDS_PER_MIN - device_state.getCurrentTime()->tm_sec;
    esp_sleep_enable_timer_wakeup(US_PER_SEC * seconds_to_sleep);
    esp_deep_sleep_start();
}

void loop() {}
