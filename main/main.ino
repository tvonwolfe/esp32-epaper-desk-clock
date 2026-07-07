#define ENABLE_GxEPD2_GFX 0

#include <WiFi.h>
#include <NTP3.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans18pt7b.h>
#include <time.h>

#define DC_PIN   (4)
#define RS_PIN   (5)
#define BUSY_PIN (6)

#define MS_PER_SEC (1000)
#define SECONDS_PER_MIN (60)
#define MINUTES_PER_HOUR SECONDS_PER_MIN
#define HOURS_PER_DAY (24)

#define TZ_OFFSET_HOURS (-7)
#define TZ_OFFSET_MINUTES (TZ_OFFSET_HOURS * MINUTES_PER_HOUR)

const char *ssid = "<wifi ssid>";
const char *password = "<wifi password>";

const char *ntpServer = "time.nist.gov";

WiFiUDP ntpUDP;
NTP3 ntp(ntpUDP);

time_t current_time;

GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(GxEPD2_290_BS(SS, DC_PIN, RS_PIN, BUSY_PIN));

void setup() {
  Serial.begin(115200);

  Serial.println("Initializing display...");
  display.init(115200, true, 50, false);
  Serial.println("Display initialized. Configuring...");

  display.setRotation(3);
  display.clearScreen();
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeSans18pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setTextSize(3);

  Serial.println("Configured.");

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  wl_status_t wifiStatus;

  while (wifiStatus != WL_CONNECTED) {
    wifiStatus = WiFi.status();
    Serial.print(wifiStatus);
    delay(1000);
  }

  Serial.println("Connected.");

  // transition to DST on the second sunday in march at 2am
  ntp.ruleDST("MDT", Second, Sun, Mar, 2, TZ_OFFSET_MINUTES + MINUTES_PER_HOUR);
  // transation off DST on first sunday in november at 2am
  ntp.ruleSTD("MST", First, Sun, Nov, 2, TZ_OFFSET_MINUTES);

  ntp.begin(ntpServer);
}

void loop() {
  ntp.update();

  displayCurrentTime();

  int seconds_to_sleep = SECONDS_PER_MIN - ntp.seconds();
  delay(seconds_to_sleep * MS_PER_SEC);
}

void displayCurrentTime() {
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(15, 95);

  int hours = ntp.hours();
  int minutes = ntp.minutes();

  if (hours > 12) hours -= 12;

  char buf[6];

  sprintf(buf, "%02d:%02d\0", hours, minutes);

  display.print(buf);
  display.display(true);
}
