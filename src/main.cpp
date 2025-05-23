#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

// address 0x27 for I2C LCDs with PCF8574
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Initializing I2C LCD...");
  
  // SDA = GPIO 17, SCL = GPIO 18
  Wire.begin(17, 18); 
  
  lcd.init();      
  lcd.backlight();  

  lcd.setCursor(0, 0);
  lcd.print("Hello, ESP32-S3!");
  lcd.setCursor(0, 1);
  lcd.print("I2C LCD OK!");
}

void loop() {
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Running...");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hello again!");
}
