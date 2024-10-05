/*
  Version 1.0 (TESTED)
    - Changed to local time and location
    - Changed for LVGL v 9.1.0
  Version 1.1 (TESTED)
    - Changed Date Format
    - Added Time label
    - Added Timer timer callback
    - Added API call to worldtimeapi.org/api/timezone
	Version 1.2
		- Added configuration via JSON files
*/
/*  Rui Santos & Sara Santos - Random Nerd Tutorials - https://RandomNerdTutorials.com/esp32-cyd-lvgl-weather-station/
    THIS EXAMPLE WAS TESTED WITH THE FOLLOWING HARDWARE:
    1) ESP32-2432S028R 2.8 inch 240×320 also known as the Cheap Yellow Display (CYD): https://makeradvisor.com/tools/cyd-cheap-yellow-display-esp32-2432s028r/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/cyd-lvgl/
    2) REGULAR ESP32 Dev Board + 2.8 inch 240x320 TFT Display: https://makeradvisor.com/tools/2-8-inch-ili9341-tft-240x320/ and https://makeradvisor.com/tools/esp32-dev-board-wi-fi-bluetooth/
      SET UP INSTRUCTIONS: https://RandomNerdTutorials.com/esp32-tft-lvgl/
    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

/*  Install the "lvgl" library version 9.X by kisvegabor to interface with the TFT Display - https://lvgl.io/
    *** IMPORTANT: lv_conf.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE lv_conf.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd-lvgl/ or https://RandomNerdTutorials.com/esp32-tft-lvgl/   */
#include <lvgl.h>

/*  Install the "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
    *** IMPORTANT: User_Setup.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE User_Setup.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd-lvgl/ or https://RandomNerdTutorials.com/esp32-tft-lvgl/   */
#include <TFT_eSPI.h>

#include "weather_images.h"
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/*
http://api.open-meteo.com/v1/forecast response
{
  "latitude":-42.875,
  "longitude":147.375,
  "generationtime_ms":0.03695487976074219,
  "utc_offset_seconds":36000,
  "timezone":"Australia/Hobart",
  "timezone_abbreviation":"AEST",
  "elevation":90.0,
  "current_units":
  {
    "time":"iso8601",
    "interval":"seconds",
    "temperature_2m":"°C",
    "relative_humidity_2m":"%",
    "is_day":"",
    "precipitation":"mm",
    "rain":"mm",
    "weather_code": "wmo code"
    },
    "current":
    {
      "time":"2024-09-21T10:00",
      "interval":900,
      "temperature_2m":10.8,
      "relative_humidity_2m":51,
      "is_day":1,
      "precipitation":0.00,
      "rain":0.00,
      "weather_code":3
    }
  }
*/

struct Config{
  char ssid[32];
  char password[29];
  char name[32];
  char endpoint[48];
  int port;
};

struct WeatherConf{
	char latitude[32];
	char longitude[32];
	char location[32];
	char zone[32];
};

Config config;
WeatherConf weatherConf;

const char *cfgFile			    = "/etc/config.json";
const char *weatherFile			= "/etc/weather.json";

// Store date and time
String current_date;
String last_weather_update;
String temperature;
String humidity;
int is_day;
int weather_code = 0;
String weather_description;

String t_current_date;
String t_current_time;
String DayOfWeek;
String MonthOfYear;
int MonthDay;
int YearNum;

static int32_t hour;
static int32_t minute;
static int32_t second;
static bool AM;

String final_time_str;
String final_date_str;

String monthList[12]  = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
String dayList[7]     = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" }; // not currently used

bool sync_time_date = false;

bool sdConfigLoad = false;
bool sdWeatherLoad = false;

// SET VARIABLE TO 0 FOR TEMPERATURE IN FAHRENHEIT DEGREES
#define TEMP_CELSIUS 1

#if TEMP_CELSIUS
  String temperature_unit = "";
  const char degree_symbol[] = "\u00B0C";
#else
  String temperature_unit = "&temperature_unit=fahrenheit";
  const char degree_symbol[] = "\u00B0F";
#endif

#define SCREEN_WIDTH 320 //240 switch these over for lvgl 9.2.0
#define SCREEN_HEIGHT 240 //320 switch these over for lvgl 9.2.0

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// If logging is enabled, it will inform the user about what is happening in the library
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

static lv_obj_t * weather_image;
static lv_obj_t * text_label_date;
static lv_obj_t * text_label_time;
static lv_obj_t * text_label_temperature;
static lv_obj_t * text_label_humidity;
static lv_obj_t * text_label_weather_description;
static lv_obj_t * text_label_time_location;

static void timer_cb(lv_timer_t * timer){
  LV_UNUSED(timer);
  get_weather_data();
  get_weather_description(weather_code);
  //lv_label_set_text(text_label_date, current_date.c_str());
  lv_label_set_text(text_label_date, final_date_str.c_str());
  lv_label_set_text(text_label_temperature, String("      " + temperature + degree_symbol).c_str());
  lv_label_set_text(text_label_humidity, String("   " + humidity + "%").c_str());
  lv_label_set_text(text_label_weather_description, weather_description.c_str());
  lv_label_set_text(text_label_time_location, String("Last Update: " + last_weather_update + "  |  " + weatherConf.location).c_str());
}

static void timer_clock_cb(lv_timer_t * timer_clock){
  LV_UNUSED(timer_clock);
  second++;
  if(second > 59) {
    second = 0;
    minute++;
    if(minute > 59) {
      minute = 0;
      hour++;
      sync_time_date = true;
      Serial.println(sync_time_date);
      Serial.println("\n\n\n\n\n\n\n\n");
      if(hour > 23) {
        hour = 0;
      }
    }
  }

  String hour_time_f = format_time(hour);
  String minute_time_f = format_time(minute);
  String second_time_f = format_time(second);
  String AMPM = " PM";
  if(AM) { AMPM = " AM"; }
  String final_time_str = String(hour_time_f) + ":" + String(minute_time_f) + ":"  + String(second_time_f) + AMPM;
  String final_date_str = String(MonthDay) + " " + String(MonthOfYear) + ", " + String(YearNum);
  lv_label_set_text(text_label_time, final_time_str.c_str());
  lv_label_set_text(text_label_date, final_date_str.c_str());
}

void lv_create_main_gui(void) {
  LV_IMAGE_DECLARE(image_weather_sun);
  LV_IMAGE_DECLARE(image_weather_cloud);
  LV_IMAGE_DECLARE(image_weather_rain);
  LV_IMAGE_DECLARE(image_weather_thunder);
  LV_IMAGE_DECLARE(image_weather_snow);
  LV_IMAGE_DECLARE(image_weather_night);
  LV_IMAGE_DECLARE(image_weather_temperature);
  LV_IMAGE_DECLARE(image_weather_humidity);

  // Get the weather data from open-meteo.com API
  get_weather_data();

  weather_image = lv_image_create(lv_screen_active());
  lv_obj_align(weather_image, LV_ALIGN_CENTER, -80, -20);
  
  get_weather_description(weather_code);

  //date
  text_label_date = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_date, final_date_str.c_str());
  lv_obj_align(text_label_date, LV_ALIGN_CENTER, 0, -80); //70, -70
  lv_obj_set_style_text_font((lv_obj_t*) text_label_date, &lv_font_montserrat_26, 0);
  lv_obj_set_style_text_color((lv_obj_t*) text_label_date, lv_palette_main(LV_PALETTE_TEAL), 0);

  //time
  text_label_time = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_time, "00:00 PM");
  lv_obj_align(text_label_time, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_time, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color((lv_obj_t*) text_label_time, lv_color_hex(0x000000), 0);

  //weather image temperature gauge
  lv_obj_t * weather_image_temperature = lv_image_create(lv_screen_active());
  lv_image_set_src(weather_image_temperature, &image_weather_temperature);
  lv_obj_align(weather_image_temperature, LV_ALIGN_CENTER, 30, -25); //30, -25

  //temperature
  text_label_temperature = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_temperature, String("      " + temperature + degree_symbol).c_str());
  lv_obj_align(text_label_temperature, LV_ALIGN_CENTER, 70, -25);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_temperature, &lv_font_montserrat_22, 0);

  //humidity
  lv_obj_t * weather_image_humidity = lv_image_create(lv_screen_active());
  lv_image_set_src(weather_image_humidity, &image_weather_humidity);
  lv_obj_align(weather_image_humidity, LV_ALIGN_CENTER, 30, 20);
  text_label_humidity = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_humidity, String("   " + humidity + "%").c_str());
  lv_obj_align(text_label_humidity, LV_ALIGN_CENTER, 70, 20);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_humidity, &lv_font_montserrat_22, 0);

  //weather description
  text_label_weather_description = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_weather_description, weather_description.c_str());
  lv_obj_align(text_label_weather_description, LV_ALIGN_BOTTOM_MID, 0, -40);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_weather_description, &lv_font_montserrat_18, 0);

  //last update and location name
  // Create a text label for the time and timezone aligned center in the bottom of the screen
  text_label_time_location = lv_label_create(lv_screen_active());
  lv_label_set_text(text_label_time_location, String("Last Update: " + last_weather_update + "  |  " + weatherConf.location).c_str());
  lv_obj_align(text_label_time_location, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_text_font((lv_obj_t*) text_label_time_location, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color((lv_obj_t*) text_label_time_location, lv_palette_main(LV_PALETTE_GREY), 0);

  //timers
  //weather update timer
  lv_timer_t * timer = lv_timer_create(timer_cb, 600000, NULL);
  lv_timer_ready(timer);
  //clock update timer
  lv_timer_t * timer_clock = lv_timer_create(timer_clock_cb, 1000, NULL);
  lv_timer_ready(timer_clock);
}

/*
  WMO Weather interpretation codes (WW)- Code	Description
  0	Clear sky
  1, 2, 3	Mainly clear, partly cloudy, and overcast
  45, 48	Fog and depositing rime fog
  51, 53, 55	Drizzle: Light, moderate, and dense intensity
  56, 57	Freezing Drizzle: Light and dense intensity
  61, 63, 65	Rain: Slight, moderate and heavy intensity
  66, 67	Freezing Rain: Light and heavy intensity
  71, 73, 75	Snow fall: Slight, moderate, and heavy intensity
  77	Snow grains
  80, 81, 82	Rain showers: Slight, moderate, and violent
  85, 86	Snow showers slight and heavy
  95 *	Thunderstorm: Slight or moderate
  96, 99 *	Thunderstorm with slight and heavy hail
*/
void get_weather_description(int code) {
  switch (code) {
    case 0:
      if(is_day==1) { lv_image_set_src(weather_image, &image_weather_sun); }
      else { lv_image_set_src(weather_image, &image_weather_night); }
      weather_description = "CLEAR SKY";
      break;
    case 1: 
      if(is_day==1) { lv_image_set_src(weather_image, &image_weather_sun); }
      else { lv_image_set_src(weather_image, &image_weather_night); }
      weather_description = "MAINLY CLEAR";
      break;
    case 2: 
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "PARTLY CLOUDY";
      break;
    case 3:
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "OVERCAST";
      break;
    case 45:
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "FOG";
      break;
    case 48:
      lv_image_set_src(weather_image, &image_weather_cloud);
      weather_description = "DEPOSITING RIME FOG";
      break;
    case 51:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "DRIZZLE LIGHT INTENSITY";
      break;
    case 53:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "DRIZZLE MODERATE INTENSITY";
      break;
    case 55:
      lv_image_set_src(weather_image, &image_weather_rain); 
      weather_description = "DRIZZLE DENSE INTENSITY";
      break;
    case 56:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "FREEZING DRIZZLE LIGHT";
      break;
    case 57:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "FREEZING DRIZZLE DENSE";
      break;
    case 61:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN SLIGHT INTENSITY";
      break;
    case 63:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN MODERATE INTENSITY";
      break;
    case 65:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN HEAVY INTENSITY";
      break;
    case 66:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "FREEZING RAIN LIGHT INTENSITY";
      break;
    case 67:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "FREEZING RAIN HEAVY INTENSITY";
      break;
    case 71:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW FALL SLIGHT INTENSITY";
      break;
    case 73:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW FALL MODERATE INTENSITY";
      break;
    case 75:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW FALL HEAVY INTENSITY";
      break;
    case 77:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW GRAINS";
      break;
    case 80:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN SHOWERS SLIGHT";
      break;
    case 81:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN SHOWERS MODERATE";
      break;
    case 82:
      lv_image_set_src(weather_image, &image_weather_rain);
      weather_description = "RAIN SHOWERS VIOLENT";
      break;
    case 85:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW SHOWERS SLIGHT";
      break;
    case 86:
      lv_image_set_src(weather_image, &image_weather_snow);
      weather_description = "SNOW SHOWERS HEAVY";
      break;
    case 95:
      lv_image_set_src(weather_image, &image_weather_thunder);
      weather_description = "THUNDERSTORM";
      break;
    case 96:
      lv_image_set_src(weather_image, &image_weather_thunder);
      weather_description = "THUNDERSTORM SLIGHT HAIL";
      break;
    case 99:
      lv_image_set_src(weather_image, &image_weather_thunder);
      weather_description = "THUNDERSTORM HEAVY HAIL";
      break;
    default: 
      weather_description = "UNKNOWN WEATHER CODE";
      break;
  }
}

void get_weather_data() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Construct the API endpoint
    String url = String("http://api.open-meteo.com/v1/forecast?latitude=" + (String)weatherConf.latitude + "&longitude=" + (String)weatherConf.longitude + "&current=temperature_2m,relative_humidity_2m,is_day,precipitation,rain,weather_code" + temperature_unit + "&timezone=" + (String)weatherConf.zone + "&forecast_days=1");
    http.begin(url);
    int httpCode = http.GET(); // Make the GET request

    if (httpCode > 0) {
      // Check for the response
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        //Serial.println("Request information:");
        //Serial.println(payload);
        // Parse the JSON to extract the time
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          const char* datetime = doc["current"]["time"];
          temperature = String(doc["current"]["temperature_2m"]);
          humidity = String(doc["current"]["relative_humidity_2m"]);
          is_day = String(doc["current"]["is_day"]).toInt();
          weather_code = String(doc["current"]["weather_code"]).toInt();
          /*Serial.println(temperature);
          Serial.println(humidity);
          Serial.println(is_day);
          Serial.println(weather_code);
          Serial.println(String(weatherConf.timezone));*/
          // Split the datetime into date and time
          String datetime_str = String(datetime);
          int splitIndex = datetime_str.indexOf('T');
          current_date = datetime_str.substring(0, splitIndex);
          parseDate(current_date);
          last_weather_update = datetime_str.substring(splitIndex + 1, splitIndex + 9); // Extract time portion
        } else {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
        }
      }
    } else {
      Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end(); // Close connection
  } else {
    Serial.println("Not connected to Wi-Fi");
  }
}

void get_date_and_time() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Construct the API endpoint
    String url = String("http://worldtimeapi.org/api/timezone/") + (String)weatherConf.zone;
    http.begin(url);
    int httpCode = http.GET(); // Make the GET request

    if (httpCode > 0) {
      // Check for the response
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        //Serial.println("Time information:");
        //Serial.println(payload);
        // Parse the JSON to extract the time
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          const char* datetime = doc["datetime"];          
          // Split the datetime into date and time
          String datetime_str = String(datetime);
          int splitIndex = datetime_str.indexOf('T');
          t_current_date = datetime_str.substring(0, splitIndex);
          t_current_time = datetime_str.substring(splitIndex + 1, splitIndex + 9); // Extract time portion
          hour = t_current_time.substring(0, 2).toInt();
		  if(hour < 13){
			  AM = true; 
		  } else { 
			  AM = false; 
			  hour -= 12;
		  }
          minute = t_current_time.substring(3, 5).toInt();
          second = t_current_time.substring(6, 8).toInt();		  
		      DayOfWeek =  dayList[String(doc["day_of_week"]).toInt()];
		      parseDate(t_current_date);  		  
        } else {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
        }
      }
    } else {
      Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
      sync_time_date = true;
    }
    http.end(); // Close connection
  } else {
    Serial.println("Not connected to Wi-Fi");
  }
}

String format_time(int time) {
  return (time < 10) ? "0" + String(time) : String(time);
}

void parseDate(String dateString){
  int fInst = dateString.indexOf("-");
  YearNum = String(dateString.substring(0, fInst)).toInt();
  int nInst = dateString.indexOf("-", fInst + 1);
  MonthOfYear = monthList[String(dateString.substring(fInst + 1, nInst)).toInt() - 1];
  int dInst = dateString.indexOf("-", nInst);
  MonthDay = dateString.substring(dInst+1,dateString.length()+1).toInt();

  String hour_time_f = format_time(hour);
  String minute_time_f = format_time(minute);
  String second_time_f = format_time(second);
  String AMPM = " PM";
  if(AM) { AMPM = " AM"; }
  final_time_str = String(hour_time_f) + ":" + String(minute_time_f) + ":"  + String(second_time_f) + AMPM;
  final_date_str = String(MonthDay) + " " + String(MonthOfYear) + ", " + String(YearNum);
  //Serial.print("Time: "); Serial.println(final_time_str);
  //Serial.print("Date: "); Serial.println(final_date_str);
}

void loadConfiguration(Config &config) {
  String data = "";
  File file = SD.open(cfgFile, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  while (file.available()) {
    data = file.readString();
  }
  file.close();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  strlcpy(config.ssid, doc["ssid"], sizeof(config.ssid));
  strlcpy(config.password, doc["pass"], sizeof(config.password));
  strlcpy(config.name, doc["name"], sizeof(config.name));
	//strlcpy(config.endpoint, doc["endpoint"], sizeof(config.endpoint));
  strlcpy(config.endpoint, "192.168.1.10", sizeof(config.endpoint));
  config.port = String(doc["port"]).toInt();

  //printConfig(config);
}

void loadWeather(WeatherConf &weatherConf) {
  String data = "";
  File file = SD.open(weatherFile, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }
  while (file.available()) {
    data = file.readString();
  }
  file.close();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  strlcpy(weatherConf.latitude, doc["latitude"], sizeof(weatherConf.latitude));
	strlcpy(weatherConf.longitude, doc["longitude"], sizeof(weatherConf.longitude));
	strlcpy(weatherConf.location, doc["location"], sizeof(weatherConf.location));
	strlcpy(weatherConf.zone, doc["zone"], sizeof(weatherConf.zone));
}

void sd_init() {
  SD.begin();
  if(!SD.begin()){
    Serial.println("Card Mount Failure");
    sdConfigLoad = false;
    return;
  }

  Serial.println("Initializing SD card ... ");
  if(!SD.begin()){
    Serial.println("ERROR - SD card initialization failed!");
    sdConfigLoad = false;
    return;
  }
  
  File file = SD.open(cfgFile);
  if(!file){
    Serial.println("File doesn't exist");
  } else {
    Serial.println("Configuration file found");
    sdConfigLoad = true;
  }
  file.close();

  file = SD.open(weatherFile);
  if(!file){
    Serial.println("Weather File doesn't exist");
  } else {
    Serial.println("Weather file found");
    sdWeatherLoad = true;
  }
  file.close();

  if(sdConfigLoad)
  {
      Serial.println(F("Loading WiFi configuration..."));
      loadConfiguration(config);
  }
	
	if(sdWeatherLoad)
  {
      Serial.println(F("Loading Weather Configuration ..."));
      loadWeather(weatherConf);
  }
}

void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

	sd_init();

  // Connect to Wi-Fi
  WiFi.begin(config.ssid, config.password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to Wi-Fi network with IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Start LVGL
  lv_init();
  // Register print function for debugging
  lv_log_register_print_cb(log_print);

  // Create a display object
  lv_display_t * disp;
  // Initialize the TFT display using the TFT_eSPI library
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  //lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270); // for LVGL 9.2.0
  
  get_date_and_time();

  // Function to draw the GUI
  lv_create_main_gui();
}

void loop() {
  if(sync_time_date) {
    sync_time_date = false;
    get_date_and_time();
    while(hour==0 && minute==0 && second==0) {
      get_date_and_time();
    }
  }
  lv_task_handler();  // let the GUI do its work
  lv_tick_inc(5);     // tell LVGL how much time has passed
  delay(5);           // let this time pass
}