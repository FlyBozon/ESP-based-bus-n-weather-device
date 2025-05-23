#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

#include "credentials.h"
#include "weather.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;      // GMT+1 for Poland
const int daylightOffset_sec = 3600;  // 1 hour daylight saving time

const int MAX_BUSES = 10;  //max number of buses to track
const int DATA_REFRESH_INTERVAL = 180000;  //refresh bus data every 3 minutes

const int joystickButtonPin = 3;
const int joystickXPin = 2;       
const int joystickYPin = 1;    

struct BusInfo {
  String busLine;
  long departureTime;  // Unix 
  int minutesRemaining;  //until departure
};

BusInfo busList[MAX_BUSES];
int busCount = 0;
int currentBusIndex = 0;
unsigned long lastDataFetchTime = 0;
unsigned long lastButtonPressTime = 0;
unsigned long lastInteractionTime = 0;
unsigned long lastMinuteUpdateTime = 0;
bool displayOn = true;
DisplayMode currentMode = BUS_MODE;
bool doubleClickDetected = false;

void setupDisplay();
void setupTime();
void setupJoystick();
void enterDeepSleep();
void fetchBusData();
bool extractBusInfo(String &json);
void updateBusTimes();
void displayBusInfo(int index);
void handleJoystick();
void checkInactivity();
void switchToWeatherMode();
void debugLog(String message);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== Bus Station IoT Device Starting ===");
  
  Wire.begin(17, 18);  // SDA = GPIO 17, SCL = GPIO 18
  setupDisplay();
  setupJoystick();
  
  WiFi.begin(ssid, password);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(attempts % 16, 1);
    lcd.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    delay(1000);
  } else {
    Serial.println("\nWiFi connection failed!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    delay(2000);
    enterDeepSleep();
    return;
  }
  
  setupTime();
  
  Serial.println("Fetching initial bus data...");
  fetchBusData();
  
  lastDataFetchTime = millis();
  lastInteractionTime = millis();
  lastMinuteUpdateTime = millis();
  
  if (busCount > 0) {
    displayBusInfo(currentBusIndex);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No buses found");
  }
}

void loop() {
  checkInactivity();
  
  if (displayOn) {
    handleJoystick();
    
    if (currentMode == BUS_MODE && millis() - lastMinuteUpdateTime >= 60000) {
      updateBusTimes();
      displayBusInfo(currentBusIndex);
      lastMinuteUpdateTime = millis();
    }
    
    if (currentMode == BUS_MODE && millis() - lastDataFetchTime >= DATA_REFRESH_INTERVAL) {
      fetchBusData();
      lastDataFetchTime = millis();
    }
    
    if (currentMode == WEATHER_MODE) {
      autoScroll();
    }
  }
  
  delay(10);
}

void setupDisplay() {
  Serial.println("Initializing display...");
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bus Tracker");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1000);
}

void setupJoystick() {
  Serial.println("Setting up joystick...");
  pinMode(joystickButtonPin, INPUT_PULLUP);
}

void setupTime() {
  Serial.println("Setting up time synchronization...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  int attempts = 0;
  struct tm timeinfo;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setting time...");
  
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(attempts, 1);
    lcd.print(".");
    attempts++;
  }
  
  if (getLocalTime(&timeinfo)) {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    Serial.printf("Time set: %s\n", timeStr);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time set:");
    lcd.setCursor(0, 1);
    lcd.print(timeStr);
    delay(1000);
  } else {
    Serial.println("Failed to set time");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time setup failed");
    delay(2000);
  }
}

void fetchBusData() {
  Serial.println("Fetching bus data...");
  
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
      return;
    }
  }
  
  time_t now;
  time(&now);
  
  String url = "https://maps.googleapis.com/maps/api/directions/json";
  url += "?origin=" + String(originStopLatLng);  
  url += "&destination=" + String(destStopLatLng); 
  url += "&mode=transit";                  
  url += "&transit_mode=bus";             
  url += "&alternatives=true";            
  url += "&departure_time=" + String(now); 
  url += "&key=" + String(apiKey);  // API key from credentials.h
  
  HTTPClient http;
  http.begin(url);
  http.setTimeout(15000);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Response received");
    
    busCount = 0;
    
    extractBusInfo(payload);
    
    for (int i = 0; i < busCount - 1; i++) {
      for (int j = 0; j < busCount - i - 1; j++) {
        if (busList[j].departureTime > busList[j + 1].departureTime) {
          BusInfo temp = busList[j];
          busList[j] = busList[j + 1];
          busList[j + 1] = temp;
        }
      }
    }
    
    time(&now);
    for (int i = 0; i < busCount; i++) {
      busList[i].minutesRemaining = (busList[i].departureTime - now) / 60;
    }
    
    currentBusIndex = 0;
    
    Serial.printf("Found %d buses\n", busCount);
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
  }
  
  http.end();
}

bool extractBusInfo(String &json) {
  Serial.println("Extracting bus information...");
  
  time_t now;
  time(&now);
  
  int pos = 0;
  
  while (pos < json.length() && busCount < MAX_BUSES) {
    int transitTypePos = json.indexOf("\"type\" : \"BUS\"", pos);
    if (transitTypePos == -1) break;
    
    int lineStartPos = json.lastIndexOf("\"short_name\" : \"", transitTypePos);
    if (lineStartPos == -1) {
      pos = transitTypePos + 10;
      continue;
    }
    
    lineStartPos += 15; 
    int lineEndPos = json.indexOf("\"", lineStartPos);
    String busLine = json.substring(lineStartPos, lineEndPos);
    
    int departTimeStartPos = json.lastIndexOf("\"value\" : ", transitTypePos);
    if (departTimeStartPos == -1) {
      pos = transitTypePos + 10;
      continue;
    }
    
    departTimeStartPos += 10; 
    int departTimeEndPos = json.indexOf(",", departTimeStartPos);
    if (departTimeEndPos == -1) {
      departTimeEndPos = json.indexOf("}", departTimeStartPos);
    }
    
    if (departTimeEndPos == -1) {
      pos = transitTypePos + 10;
      continue;
    }
    
    long departureTime = json.substring(departTimeStartPos, departTimeEndPos).toInt();
    
    if (departureTime < now - 60) {
      pos = transitTypePos + 10;
      continue;
    }
    
    Serial.printf("Found bus: Line %s, Departure: %ld\n", busLine.c_str(), departureTime);
    
    busList[busCount].busLine = busLine;
    busList[busCount].departureTime = departureTime;
    busList[busCount].minutesRemaining = (departureTime - now) / 60;
    busCount++;
    
    pos = transitTypePos + 10;
  }
  
  return busCount > 0;
}

void updateBusTimes() {
  time_t now;
  time(&now);
  
  for (int i = 0; i < busCount; i++) {
    busList[i].minutesRemaining = (busList[i].departureTime - now) / 60;
  }
  
  int newCount = 0;
  for (int i = 0; i < busCount; i++) {
    if (busList[i].minutesRemaining >= -1) { //keep buses that haven't departed or just departed
      if (i != newCount) {
        busList[newCount] = busList[i];
      }
      newCount++;
    }
  }
  
  busCount = newCount;
  
  if (busCount == 0) {
    currentBusIndex = 0;
  } else if (currentBusIndex >= busCount) {
    currentBusIndex = busCount - 1;
  }
}

void displayBusInfo(int index) {
  if (busCount == 0 || index < 0 || index >= busCount) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No buses found");
    return;
  }
  
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("Bus " + busList[index].busLine);
  
  lcd.setCursor(0, 1);
  if (busList[index].minutesRemaining <= 0) {
    lcd.print("Departing now!");
  } else {
    lcd.print("In " + String(busList[index].minutesRemaining) + " min");
  }
  
  lcd.setCursor(13, 0);
  lcd.print(String(index + 1) + "/" + String(busCount));
}

void handleJoystick() {
  bool buttonPressed = false;
  
  if (digitalRead(joystickButtonPin) == LOW) {
    delay(50); //debounce
    if (digitalRead(joystickButtonPin) == LOW) {
      buttonPressed = true;
      unsigned long currentTime = millis();
      
      if (currentTime - lastButtonPressTime < 300) {
        doubleClickDetected = true;
        
        if (currentMode == BUS_MODE) {
          switchToWeatherMode();
        } else if (currentMode == WEATHER_MODE) {
          currentMode = BUS_MODE;
          displayBusInfo(currentBusIndex);
        }
      } else {
        displayOn = true;
        lcd.backlight();
      }
      
      lastButtonPressTime = currentTime;
      lastInteractionTime = currentTime;
      
      while (digitalRead(joystickButtonPin) == LOW) {
        delay(10);
      }
    }
  }
  
  int yValue = analogRead(joystickYPin);
  
  if (currentMode == BUS_MODE) {
    if (yValue > 3000) {
      if (currentBusIndex < busCount - 1) {
        currentBusIndex++;
        displayBusInfo(currentBusIndex);
        lastInteractionTime = millis();
      }
      delay(200);
    } else if (yValue < 1000) {
      if (currentBusIndex > 0) {
        currentBusIndex--;
        displayBusInfo(currentBusIndex);
        lastInteractionTime = millis();
      }
      delay(200); 
    }
  } else if (currentMode == WEATHER_MODE) {
    handleWeatherMode(yValue, buttonPressed);
  }
}

void checkInactivity() {
  if (millis() - lastInteractionTime > 60000) {
    Serial.println("Inactivity timeout, entering sleep mode...");
    lcd.noBacklight();
    displayOn = false;
    
    if (millis() - lastInteractionTime > 180000) {
      enterDeepSleep();
    }
  }
}

void enterDeepSleep() {
  Serial.println("Entering deep sleep mode...");
  lcd.clear();
  lcd.noBacklight();
  
  esp_sleep_enable_ext0_wakeup((gpio_num_t)joystickButtonPin, LOW); 
  
  esp_deep_sleep_start();
}

void switchToWeatherMode() {
  Serial.println("Switching to weather mode...");
  currentMode = WEATHER_MODE;
  
  initWeatherMode();
  
  lastInteractionTime = millis();
}

void debugLog(String message) {
  Serial.println(message);
}