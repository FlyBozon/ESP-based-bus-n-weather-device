#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>

class LiquidCrystal_I2C;
class HTTPClient;

enum DisplayMode {
  BUS_MODE,
  WEATHER_MODE,
  DISPLAY_OFF
};

extern LiquidCrystal_I2C lcd;
extern unsigned long lastInteractionTime;
extern DisplayMode currentMode;

void fetchWeatherData();
bool processWeatherData(String payload);
void displayDailyForecast(int dayIndex);
void displayHourlyForecast(int dayIndex, int hourIndex);
void handleWeatherPress();
void autoScroll();
void weatherManualScroll(int yMovement);
void toggleAutoScroll();
void handleWeatherMode(int yMovement, bool buttonPressed);
void initWeatherMode();

void parseWeatherData(String payload);
void displayWeather();
String formatTime(long timestamp);
String formatDate(long timestamp);

#endif // WEATHER_H