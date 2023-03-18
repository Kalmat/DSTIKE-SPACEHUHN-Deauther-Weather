/**The MIT License (MIT)

Copyright (c) 2018 by Daniel Eichhorn - ThingPulse

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at https://thingpulse.com
*/

#include <Arduino.h>

#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
#include <JsonListener.h>

// time
#ifdef RTC_DS3231
  #include "src/DS3231-1.0.3/DS3231.h"
#endif // ifdef RTC_DS3231
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()

#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "src/esp8266-oled-ssd1306-4.1.0/OLEDDisplayUi.h"

//declaring prototypes
void weatherConfig(OLEDDisplay *display);
void connectWifi(OLEDDisplay *display, bool lastSeen1);
void updateData(OLEDDisplay *display);
void drawClockWeather(OLEDDisplay *display, bool forceDraw, bool forceUpdate);
void drawSetClockW(OLEDDisplay *display);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void getTime();
void getTimeRTC();
void getTimeNTP();
void setTimeRTCfromNTP(bool updateTime);
void weatherUP();
void weatherDOWN();
void weatherRotate();

/***************************
 * 
 * Begin Settings
 **************************/

// WIFI
#define KEYS_IN_SEPARATE_FILE   true

// To avoid sharing your keys, create a "WifiKeys.h" file and define WIFI_SSID1, WIFI_PWD1, WIFI_SSID2, WIFI_PWD2 and OPEN_WEATHER_MAP_APP_ID in there
// Or define them here
// Anyway, DON'T SHARE YOUR KEYS!!!
#ifdef KEYS_IN_SEPARATE_FILE
  #include "WifiKeys.h"
#else // #ifdef KEYS_IN_SEPARATE_FILE
  #define WIFI_SSID1       ""
  #define WIFI_PWD1        ""
  // Alternate WiFi (e.g. your phone AP to allow updating data even though you're away from home!)
  #define WIFI_SSID2       ""
  #define WIFI_PWD2        ""
#endif // #ifdef KEYS_IN_SEPARATE_FILE

// CLOCK
#define TZ              0       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

// Setup
#define UPDATE_INTERVAL_SECS  20 * 60 // Update every 20 minutes

// OpenWeatherMap Settings
// Sign up here to get an API key:
// https://docs.thingpulse.com/how-tos/openweathermap-key/
#ifndef KEYS_IN_SEPARATE_FILE
  #define OPEN_WEATHER_MAP_APP_ID ""
#endif // #ifndef KEYS_IN_SEPARATE_FILE
/*
Go to https://openweathermap.org/find?q= and search for a location. Go through the
result set and select the entry closest to the actual location you want to display 
data for. It'll be a URL like https://openweathermap.org/city/2657896. The number
at the end is what you assign to the constant below.
 */
#define OPEN_WEATHER_MAP_LOCATION_ID  "3117735"

// Pick a language code from this list:
// Arabic - ar, Bulgarian - bg, Catalan - ca, Czech - cz, German - de, Greek - el,
// English - en, Persian (Farsi) - fa, Finnish - fi, French - fr, Galician - gl,
// Croatian - hr, Hungarian - hu, Italian - it, Japanese - ja, Korean - kr,
// Latvian - la, Lithuanian - lt, Macedonian - mk, Dutch - nl, Polish - pl,
// Portuguese - pt, Romanian - ro, Russian - ru, Swedish - se, Slovak - sk,
// Slovenian - sl, Spanish - es, Turkish - tr, Ukrainian - ua, Vietnamese - vi,
// Chinese Simplified - zh_cn, Chinese Traditional - zh_tw.
#define OPEN_WEATHER_MAP_LANGUAGE   "es"
#define MAX_FORECASTS               4

#define IS_METRIC                   true

// Adjust according to your language
#define WDAY_NAMES_T                {"DOM", "LUN", "MAR", "MIE", "JUE", "VIE", "SAB"}
#define MONTH_NAMES_T               {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGo", "SEP", "OCT", "NOV", "DIC"}

// UI
#define TARGETFPS                   30

/***************************
 * End Settings
 **************************/
