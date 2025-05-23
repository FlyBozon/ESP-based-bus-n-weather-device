#include "weather.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

extern LiquidCrystal_I2C lcd;
extern unsigned long lastInteractionTime;
extern DisplayMode currentMode;

extern const char* weatherApiKey;
extern const char* city;
extern const char* units;

enum WeatherViewMode {
  DAILY_VIEW,      
  HOURLY_VIEW      
};

const int MAX_DAYS = 5;          
const int MAX_HOURS_PER_DAY = 8; 

struct DailyForecast {
  String day;          // day of week (Mon, Tue, etc.)
  String date;         //date (DD/MM)
  float tempMin;      
  float tempMax;       
  String mainWeather;  //main weather condition (Cloudy, Rain, etc.)
  String weatherIcon; 
};

struct HourlyForecast {
  String time;         // time (HH:MM)
  float temp;         
  String description; 
  String icon;        
};

DailyForecast dailyForecasts[MAX_DAYS];
HourlyForecast hourlyForecasts[MAX_DAYS][MAX_HOURS_PER_DAY];
int hourlyForecastCount[MAX_DAYS] = {0}; 
int dailyForecastCount = 0;             
int currentDayIndex = 0;               
int currentHourIndex = 0;           

WeatherViewMode currentViewMode = DAILY_VIEW;

unsigned long lastScrollTime = 0; 

bool autoScrollEnabled = false;  

const char* daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

void fetchWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.reconnect();
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi reconnection failed");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Error");
      return;
    }
  }
  
  dailyForecastCount = 0;
  for (int i = 0; i < MAX_DAYS; i++) {
    hourlyForecastCount[i] = 0;
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Getting weather");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
  
  HTTPClient http;
  
  //5-day forecast API with 3-hour steps
  String forecastURL = "http://api.openweathermap.org/data/2.5/forecast?q=" + String(city) + "&units=" + String(units) + "&appid=" + String(weatherApiKey);
  
  Serial.println("Fetching 5-day forecast...");
  http.begin(forecastURL);
  http.setTimeout(15000); 
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.print("Forecast payload length: ");
    Serial.println(payload.length());
    
    if (processWeatherData(payload)) {
      Serial.println("Weather data processed successfully");
      
      currentViewMode = DAILY_VIEW;
      currentDayIndex = 0;
      currentHourIndex = 0;
      
      displayDailyForecast(currentDayIndex);
    } else {
      Serial.println("Failed to process weather data");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error parsing");
      lcd.setCursor(0, 1);
      lcd.print("weather data");
    }
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Weather error");
    lcd.setCursor(0, 1);
    lcd.print("HTTP: " + String(httpCode));
  }
  
  http.end();
  
  WiFi.disconnect(false);
}

bool processWeatherData(String payload) {
  if (payload.length() < 100 || payload.indexOf("\"list\":[") == -1) {
    Serial.println("Invalid weather data response");
    return false;
  }
  
  int listStart = payload.indexOf("\"list\":[");
  if (listStart == -1) {
    Serial.println("Error: No list array found in forecast data");
    return false;
  }
  
  int pos = listStart + 8; 
  
  String currentDate = "";
  int dayIndex = -1;
  
  while (pos < payload.length() && dailyForecastCount < MAX_DAYS) {
    int itemStart = payload.indexOf("{", pos);
    if (itemStart == -1) break;
    
    int itemEnd = payload.indexOf("},", itemStart);
    if (itemEnd == -1) {
      itemEnd = payload.indexOf("}]", itemStart);
      if (itemEnd == -1) break;
    }
    
    String item = payload.substring(itemStart, itemEnd + 1);
    
    int timePos = item.indexOf("\"dt\":");
    if (timePos == -1) {
      pos = itemEnd + 2;
      continue;
    }
    
    timePos += 5; 
    int timeEnd = item.indexOf(",", timePos);
    if (timeEnd == -1) {
      pos = itemEnd + 2;
      continue;
    }
    
    long timestamp = item.substring(timePos, timeEnd).toInt();
    
    time_t time = timestamp;
    struct tm timeinfo;
    gmtime_r(&time, &timeinfo);
    timeinfo.tm_hour += 1;
    
    char dateStr[15];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
    String forecastDate = String(dateStr);
    
    float temp = 0.0;
    int mainPos = item.indexOf("\"main\":{");
    if (mainPos != -1) {
      int tempPos = item.indexOf("\"temp\":", mainPos);
      if (tempPos != -1) {
        tempPos += 7;  
        int tempEnd = item.indexOf(",", tempPos);
        if (tempEnd != -1) {
          temp = item.substring(tempPos, tempEnd).toFloat();
        }
      }
      
      if (currentDate != forecastDate) {
        if (dailyForecastCount < MAX_DAYS) {
          dayIndex = dailyForecastCount;
          dailyForecastCount++;
          currentDate = forecastDate;
          
          char dayOfWeekStr[4];
          strftime(dayOfWeekStr, sizeof(dayOfWeekStr), "%a", &timeinfo);
          
          char dateDayStr[6];
          strftime(dateDayStr, sizeof(dateDayStr), "%d/%m", &timeinfo);
          
          dailyForecasts[dayIndex].day = String(dayOfWeekStr);
          dailyForecasts[dayIndex].date = String(dateDayStr);
          dailyForecasts[dayIndex].tempMin = temp;
          dailyForecasts[dayIndex].tempMax = temp;
          
          hourlyForecastCount[dayIndex] = 0;
        }
      } else {
        if (dayIndex >= 0) {
          dailyForecasts[dayIndex].tempMin = min(dailyForecasts[dayIndex].tempMin, temp);
          dailyForecasts[dayIndex].tempMax = max(dailyForecasts[dayIndex].tempMax, temp);
        }
      }
      
      int weatherPos = item.indexOf("\"weather\":[{");
      if (weatherPos != -1) {
        int mainWeatherPos = item.indexOf("\"main\":\"", weatherPos);
        if (mainWeatherPos != -1) {
          mainWeatherPos += 8; 
          int mainWeatherEnd = item.indexOf("\"", mainWeatherPos);
          if (mainWeatherEnd != -1) {
            String mainWeather = item.substring(mainWeatherPos, mainWeatherEnd);
            
            if (dayIndex >= 0 && hourlyForecastCount[dayIndex] == 0) {
              dailyForecasts[dayIndex].mainWeather = mainWeather;
            }
          }
        }
        
        int iconPos = item.indexOf("\"icon\":\"", weatherPos);
        if (iconPos != -1) {
          iconPos += 8; 
          int iconEnd = item.indexOf("\"", iconPos);
          if (iconEnd != -1) {
            String icon = item.substring(iconPos, iconEnd);
            
            if (dayIndex >= 0 && hourlyForecastCount[dayIndex] == 0) {
              dailyForecasts[dayIndex].weatherIcon = icon;
            }
            
            int descPos = item.indexOf("\"description\":\"", weatherPos);
            if (descPos != -1) {
              descPos += 15;
              int descEnd = item.indexOf("\"", descPos);
              if (descEnd != -1) {
                String desc = item.substring(descPos, descEnd);
                
                if (dayIndex >= 0 && hourlyForecastCount[dayIndex] < MAX_HOURS_PER_DAY) {
                  int hourIndex = hourlyForecastCount[dayIndex];
                  
                  char timeStr[6];
                  strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
                  
                  hourlyForecasts[dayIndex][hourIndex].time = String(timeStr);
                  hourlyForecasts[dayIndex][hourIndex].temp = temp;
                  hourlyForecasts[dayIndex][hourIndex].description = desc;
                  hourlyForecasts[dayIndex][hourIndex].icon = icon;
                  
                  hourlyForecastCount[dayIndex]++;
                }
              }
            }
          }
        }
      }
    }
    
    pos = itemEnd + 2;
  }
  
  Serial.printf("Processed %d days of forecast\n", dailyForecastCount);
  
  for (int i = 0; i < dailyForecastCount; i++) {
    Serial.printf("Day %d (%s): %d hourly forecasts\n", 
                 i, dailyForecasts[i].day.c_str(), hourlyForecastCount[i]);
  }
  
  return dailyForecastCount > 0;
}

void displayDailyForecast(int dayIndex) {
  if (dailyForecastCount == 0 || dayIndex < 0 || dayIndex >= dailyForecastCount) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No forecast data");
    return;
  }
  
  lcd.clear();
  
  //first line: day, date, and position
  lcd.setCursor(0, 0);
  lcd.print(dailyForecasts[dayIndex].day);
  lcd.print(" ");
  lcd.print(dailyForecasts[dayIndex].date);
  lcd.setCursor(13, 0);
  lcd.print(String(dayIndex + 1) + "/" + String(dailyForecastCount));
  
  //second line: temp range and weather
  lcd.setCursor(0, 1);
  lcd.print(String(dailyForecasts[dayIndex].tempMin, 1));
  lcd.print("-");
  lcd.print(String(dailyForecasts[dayIndex].tempMax, 1));
  lcd.print("C ");
  
  String weather = dailyForecasts[dayIndex].mainWeather;
  int usedChars = 7; 
  int maxLen = 16 - usedChars;
  
  if (weather.length() > maxLen) {
    weather = weather.substring(0, maxLen - 3) + "...";
  }
  
  lcd.print(weather);
}

void displayHourlyForecast(int dayIndex, int hourIndex) {
  if (dayIndex < 0 || dayIndex >= dailyForecastCount || 
      hourlyForecastCount[dayIndex] == 0 || 
      hourIndex < 0 || hourIndex >= hourlyForecastCount[dayIndex]) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No hourly data");
    return;
  }
  
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print(dailyForecasts[dayIndex].day);
  lcd.print(" ");
  lcd.print(hourlyForecasts[dayIndex][hourIndex].time);
  lcd.setCursor(11, 0);
  lcd.print(String(hourIndex + 1) + "/" + String(hourlyForecastCount[dayIndex]));
  
  lcd.setCursor(0, 1);
  lcd.print(String(hourlyForecasts[dayIndex][hourIndex].temp, 1));
  lcd.print("C ");
  
  String desc = hourlyForecasts[dayIndex][hourIndex].description;
  int usedChars = 5; 
  int maxLen = 16 - usedChars;
  
  if (desc.length() > maxLen) {
    desc = desc.substring(0, maxLen - 3) + "...";
  }
  
  lcd.print(desc);
}

void handleWeatherPress() {
  if (currentViewMode == DAILY_VIEW) {
    if (dailyForecastCount > 0 && hourlyForecastCount[currentDayIndex] > 0) {
      currentViewMode = HOURLY_VIEW;
      currentHourIndex = 0;
      displayHourlyForecast(currentDayIndex, currentHourIndex);
    }
  } else {
    currentViewMode = DAILY_VIEW;
    displayDailyForecast(currentDayIndex);
  }
}

void autoScroll() {
  if (!autoScrollEnabled || millis() - lastScrollTime <= 2500) {
    return;
  }
  
  lastScrollTime = millis();
  
  if (currentViewMode == DAILY_VIEW) {
    if (dailyForecastCount > 0) {
      currentDayIndex = (currentDayIndex + 1) % dailyForecastCount;
      displayDailyForecast(currentDayIndex);
    }
  } else {
    if (hourlyForecastCount[currentDayIndex] > 0) {
      currentHourIndex = (currentHourIndex + 1) % hourlyForecastCount[currentDayIndex];
      displayHourlyForecast(currentDayIndex, currentHourIndex);
    }
  }
}

void weatherManualScroll(int yMovement) {
  if (yMovement > 3000) { 
    lastInteractionTime = millis();
    
    if (currentViewMode == DAILY_VIEW) {
      if (dailyForecastCount > 0) {
        currentDayIndex = (currentDayIndex + 1) % dailyForecastCount;
        displayDailyForecast(currentDayIndex);
      }
    } else {
      if (hourlyForecastCount[currentDayIndex] > 0) {
        currentHourIndex = (currentHourIndex + 1) % hourlyForecastCount[currentDayIndex];
        displayHourlyForecast(currentDayIndex, currentHourIndex);
      }
    }
    
    delay(200); 
  } else if (yMovement < 1000) { 
    lastInteractionTime = millis();
    
    if (currentViewMode == DAILY_VIEW) {
      if (dailyForecastCount > 0) {
        currentDayIndex = (currentDayIndex - 1 + dailyForecastCount) % dailyForecastCount;
        displayDailyForecast(currentDayIndex);
      }
    } else {
      if (hourlyForecastCount[currentDayIndex] > 0) {
        currentHourIndex = (currentHourIndex - 1 + hourlyForecastCount[currentDayIndex]) % hourlyForecastCount[currentDayIndex];
        displayHourlyForecast(currentDayIndex, currentHourIndex);
      }
    }
    
    delay(200); 
  }
}

void toggleAutoScroll() {
  autoScrollEnabled = !autoScrollEnabled;
  lastScrollTime = millis();
  
  Serial.print("Auto Scroll: ");
  Serial.println(autoScrollEnabled ? "Enabled" : "Disabled");
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Auto-scroll:");
  lcd.setCursor(0, 1);
  lcd.print(autoScrollEnabled ? "Enabled" : "Disabled");
  delay(1000);
  
  if (currentViewMode == DAILY_VIEW) {
    displayDailyForecast(currentDayIndex);
  } else {
    displayHourlyForecast(currentDayIndex, currentHourIndex);
  }
}

void handleWeatherMode(int yMovement, bool buttonPressed) {
  if (buttonPressed) {
    if (autoScrollEnabled) {
      toggleAutoScroll();
    } else {
      handleWeatherPress();
    }
  }
  
  weatherManualScroll(yMovement);
  autoScroll();
}

void initWeatherMode() {
  currentViewMode = DAILY_VIEW;
  currentDayIndex = 0;
  currentHourIndex = 0;
  autoScrollEnabled = false;
  fetchWeatherData();
}

void parseWeatherData(String payload) {
  processWeatherData(payload);
}

void displayWeather() {
  if (currentViewMode == DAILY_VIEW) {
    displayDailyForecast(currentDayIndex);
  } else {
    displayHourlyForecast(currentDayIndex, currentHourIndex);
  }
}

String formatTime(long timestamp) {
  time_t t = timestamp;
  struct tm timeinfo;
  
  gmtime_r(&t, &timeinfo);
  
  timeinfo.tm_hour += 1; 
  
  char buffer[10];
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  return String(buffer);
}

String formatDate(long timestamp) {
  time_t t = timestamp;
  struct tm timeinfo;
  
  gmtime_r(&t, &timeinfo);
  
  char buffer[15];
  strftime(buffer, sizeof(buffer), "%d/%m", &timeinfo);
  return String(buffer);
}