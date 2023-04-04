#include "A_config.h"
#include "WeatherStation.h"

#ifdef RTC_DS3231
  DS3231 myClock;
  bool h12;
  bool PM_time;
  bool century;
#endif // ifdef RTC_DS3231
time_t now;
struct tm* timeInfo;

int clockHour = 6;
int clockMinute = 0;
int clockSecond = 0;
int clockDoW = 1;
int clockDate = 1;
int clockMonth = 1;
int clockYear = 1970;

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;
OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

OLEDDisplayUi* ui;

// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[3] = { drawDateTime, drawCurrentWeather, drawForecast };
uint8_t numberOfFrames = 3;
OverlayCallback overlays[1] = { drawHeaderOverlay };
uint8_t numberOfOverlays = 1;

String WDAY_NAMES[7] = WDAY_NAMES_T;
String MONTH_NAMES[12] = MONTH_NAMES_T;

bool readyForWeatherUpdate = true;
String lastUpdate = "--";
long timeSinceLastWUpdate = 0;

bool lastSeen1 = true;

void weatherConfig(OLEDDisplay *display) {

  ui = new OLEDDisplayUi(display);

  ui->setTargetFPS(TARGETFPS);

  ui->setActiveSymbol(activeSymbole);
  ui->setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui->setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui->setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui->setFrameAnimation(SLIDE_LEFT);

  ui->setFrames(frames, numberOfFrames);
  ui->setOverlays(overlays, numberOfOverlays);
}

void drawClockWeather(OLEDDisplay *display, bool forceDraw, bool forceUpdate) {

  if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_SECS) || forceUpdate) {
    Serial.println("Setting readyForUpdate to true");
    readyForWeatherUpdate = true;
  }

  if ((readyForWeatherUpdate && ui->getUiState()->frameState == FIXED)) {
    readyForWeatherUpdate = false;
    timeSinceLastWUpdate = millis();
    updateData(display);
  }
  display->flipScreenVertically();

  if (forceDraw) {
      ui->switchToFrame(1);
      ui->enableAutoTransition();
  }

  int remainingTimeBudget = ui->update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your time budget
    delay(remainingTimeBudget);
  }
}

void drawSetClockW(OLEDDisplay *display) {

  char buff[16];

  getTime();
 
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 5, "UP/DOWN to set Clock");

  display->setFont(ArialMT_Plain_24);
  sprintf_P(buff, PSTR("%02d:%02d"), clockHour, clockMinute);
  display->drawString(64, 15, String(buff));
}

void connectWifi(OLEDDisplay *display, bool lastSeen1) {
  
  uint8_t counter = 0;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected. Connecting to WiFi " + String(lastSeen1));

    if (lastSeen1) {
      WiFi.begin(WIFI_SSID1, WIFI_PWD1);
    } else {
      WiFi.begin(WIFI_SSID2, WIFI_PWD2);
    }

    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->setFont(ArialMT_Plain_10);
  
    while (WiFi.status() != WL_CONNECTED && counter < 10) {
      delay(500);
      Serial.println("Connecting to WIFI " + String(lastSeen1) + " ... " + (String)counter + String(WiFi.status()));
      display->clear();
      display->drawString(64, 10, "Connecting to WiFi " + String(lastSeen1));
      display->drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
      display->drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
      display->drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
      display->display();
  
      counter++;
    }
  }
}

void updateData(OLEDDisplay *display) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected. Connecting to WiFi...");

    connectWifi(display, lastSeen1);
    if (WiFi.status() != WL_CONNECTED) {
      lastSeen1 = !lastSeen1;
      connectWifi(display, lastSeen1);
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to personal WiFi to update Weather data!");
    display->clear();
  
    drawProgress(display, 10, "Updating time...");
    getTime();
    if (clockYear <= 2000) {
      Serial.println("Updating Internet Time & Date...");
      configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
      uint8_t counter = 0;
      while (clockYear <= 2000 && counter < 6) {
        Serial.println("Waiting for NTP server reply... " + String(counter) + " " + String(clockYear) + " " + String(myClock.getYear()));
        delay(500);
        getTime();
        counter++;
      }
    }
    drawProgress(display, 30, "Updating weather...");
    currentWeatherClient.setMetric(IS_METRIC);
    currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
    currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);
  
    drawProgress(display, 50, "Updating forecasts...");
    forecastClient.setMetric(IS_METRIC);
    forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
    uint8_t allowedHours[] = {12};
    forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
    forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);

    drawProgress(display, 100, "Done...");
    delay(1000);
  } else {
    Serial.println("NOT connected to personal WiFi. No Weather data will be availabe");
    drawProgress(display, 100, "WiFi unavailable");
    delay(1000);
  }
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->flipScreenVertically();
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void getTimeRTC() {
  clockHour = myClock.getHour(h12, PM_time);
  clockMinute = myClock.getMinute();
  clockSecond = myClock.getSecond();
  clockDoW = myClock.getDoW();
  clockDate = myClock.getDate();
  clockMonth = myClock.getMonth(century);
  clockYear = myClock.getYear() + 2000;
}

void getTimeNTP() {
  now = time(nullptr);
  timeInfo = localtime(&now);
  clockHour = timeInfo->tm_hour;
  clockMinute = timeInfo->tm_min;
  clockSecond = timeInfo->tm_sec;
  clockDoW = timeInfo->tm_wday;
  clockDate = timeInfo->tm_mday;
  clockMonth = timeInfo->tm_mon+1;
  clockYear = timeInfo->tm_year + 1900;
}

void setTimeRTCfromNTP(bool updateTime) {
  if (updateTime) {
    getTimeNTP();
  }
  myClock.setHour(clockHour);
  myClock.setMinute(clockMinute);
  myClock.setSecond(clockSecond);
  myClock.setDate(clockDate);
  myClock.setMonth(clockMonth);
  myClock.setYear(clockYear - 2000);
}

void getTime() {

  #ifdef RTC_DS3231
    getTimeRTC();
    if (clockYear <= 2000) {
      getTimeNTP();
      if (clockYear > 1900) {
        setTimeRTCfromNTP(false);
      }
    }
  #else // #ifdef RTC_DS3231
    getTimeNTP();
  #endif // #ifdef RTC_DS3231
}

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {

  char buff[16];

  getTime();
 
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);

  sprintf_P(buff, PSTR("%s, %02d/%02d/%04d"), WDAY_NAMES[clockDoW].c_str(), clockDate, clockMonth, clockYear);
  display->drawString(64 + x, 5 + y, String(buff));
  display->setFont(ArialMT_Plain_24);

  sprintf_P(buff, PSTR("%02d:%02d:%02d"), clockHour, clockMinute, clockSecond);
  display->drawString(64 + x, 15 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, currentWeather.description);

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(currentWeather.temp, 1) + "°"; // + (IS_METRIC ? "°C" : "°F");
  int initX = temp.length() > 6 ? 60 : 68;  // Leaving room to negative "-" sign
  display->drawString(initX + x, 5 + y, temp);

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(32 + x, 0 + y, currentWeather.iconMeteoCon);
}

void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  timeInfo = localtime(&observationTimestamp);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  String temp = String(forecasts[dayIndex].temp, 0) + "°"; // + (IS_METRIC ? "°C" : "°F");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {

  char buff[14];

  sprintf_P(buff, PSTR("%02d:%02d"), clockHour, clockMinute);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.temp, 1) + "°"; // + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}

void weatherUP() {
  ui->nextFrame();
  ui->disableAutoTransition();
}

void weatherDOWN() {
  ui->previousFrame();
  ui->disableAutoTransition();
}

void weatherRotate() {
  ui->enableAutoTransition();
}
