# ESP-based-bus-n-weather-device

ESP32-S3 based IoT bus station and weather display for your home.

The goal of this project was to create an IoT device that shows you the next buses from the closest bus stop on an LCD display. The next part was integrating weather forecasting by pressing the joystick twice, just because I could do that, and it was a practical solution.

## Features

- **Bus Tracking**: Real-time bus arrival information from your local bus stop
- **Weather Forecasting**: 5-day weather forecast with detailed hourly breakdown
- **Power Efficient**: Automatic sleep mode after inactivity to save battery
- **Intuitive Navigation**: Simple joystick-based interface
- **Dual Display Modes**: Easy switching between bus and weather information

## Hardware Requirements

- **ESP32-S3 Development Board** (ESP32-S3-DevKitC-1 or similar)
- **16x2 LCD Display with I2C Backpack** (HD44780 compatible)
- **Analog Joystick Module** (KY-023 or similar)
- **Jumper Wires** for connections
- **Breadboard** (optional, for prototyping)
- **Power Supply** (USB or battery pack)

## Hardware Connections

| Component | ESP32-S3 Pin | Notes |
|-----------|--------------|-------|
| LCD SDA | GPIO 17 | I2C Data |
| LCD SCL | GPIO 18 | I2C Clock |
| LCD VCC | 3.3V/5V | Power |
| LCD GND | GND | Ground |
| Joystick VRx | GPIO 1 | Y-axis (Up/Down) |
| Joystick VRy | GPIO 2 | X-axis (Left/Right) |
| Joystick SW | GPIO 3 | Button Press |
| Joystick VCC | 3.3V | Power |
| Joystick GND | GND | Ground |

## Software Requirements

- **PlatformIO** or **Arduino IDE**
- **ESP32 Board Package** (version 2.0.0 or higher)

### Required Libraries

Add these to your `platformio.ini` file:

```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

monitor_speed = 115200
lib_deps = 
  marcoschwartz/LiquidCrystal_I2C @ ^1.1.4
  bblanchon/ArduinoJson @ ^6.20.0

build_flags =
  -DCORE_DEBUG_LEVEL=5
```

## Project Structure

```
├── src/
│   ├── main.cpp           # Main application logic
│   ├── weather.cpp        # Weather functionality
│   ├── weather.h          # Weather header file
│   └── credentials.h      # API keys and WiFi credentials
├── platformio.ini         # PlatformIO configuration
└── README.md             # This file
```

## How to Configure Google Cloud

### Step 1: Create a Google Cloud Account
1. Go to [Google Cloud Console](https://console.cloud.google.com/)
2. Sign in with your Google account
3. Accept the terms of service

### Step 2: Create a New Project
1. Click on the project dropdown at the top of the page
2. Click "New Project"
3. Enter a name for your project (e.g., "ESP32-Bus-Tracker")
4. Click "Create"

### Step 3: Enable the Directions API
1. In the Google Cloud Console, go to "APIs & Services" > "Library"
2. Search for "Directions API"
3. Click on the Directions API result
4. Click "Enable"

### Step 4: Set Up Billing
The Google Directions API requires billing to be enabled:
1. Go to "Billing" in the left navigation menu
2. Link a payment method (credit card)
3. New accounts get $300 in free credits for 90 days
4. Set up billing alerts to monitor usage

### Step 5: Create API Credentials
1. Go to "APIs & Services" > "Credentials"
2. Click "Create Credentials" > "API Key"
3. Copy your API key immediately
4. **Important**: Store this key securely

### Step 6: Secure Your API Key (Recommended)
1. Click on your newly created API key to edit it
2. Under "API restrictions," select "Restrict key"
3. Select "Directions API" from the dropdown
4. Under "Application restrictions," you can optionally restrict by IP address
5. Click "Save"

### Step 7: Find Your Bus Stop Coordinates
1. Open [Google Maps](https://maps.google.com/)
2. Navigate to your bus stop
3. Right-click on the exact bus stop location
4. Select "What's here?"
5. Copy the coordinates (latitude, longitude) that appear
6. You'll need coordinates for both your origin and destination bus stops

## Open Weather API

### Step 1: Create an OpenWeatherMap Account
1. Go to [OpenWeatherMap](https://openweathermap.org/api)
2. Click "Sign Up" to create a free account
3. Verify your email address

### Step 2: Get Your API Key
1. After logging in, go to your [API keys page](https://home.openweathermap.org/api_keys)
2. Copy your default API key
3. **Note**: New API keys may take up to 10 minutes to activate

### Step 3: Choose Your Plan
- **Free Plan**: 60 calls/minute, 1,000 calls/day, 5-day forecast
- **Paid Plans**: Higher limits and additional features
- For this project, the free plan is sufficient

## Configuration Setup

Update Your Coordinates in `credentials.h`

Replace the example coordinates with your actual bus stop locations:
- `originStopLatLng`: Your primary bus stop
- `destStopLatLng`: A nearby bus stop (the "hack" to get bus schedule data)

## Usage Instructions

### Bus Mode (Default)
- **Joystick Up/Down**: Navigate through upcoming buses
- **Display Shows**: Bus number, minutes until arrival, and position in list
- **Single Click**: Wake up display if sleeping
- **Data Refresh**: Every 3 minutes when active

### Weather Mode
- **Double Click Joystick**: Switch to weather mode
- **Daily View**: Shows 5-day forecast with day, date, min/max temps, weather
- **Joystick Up/Down**: Navigate through days
- **Single Click**: View hourly details for selected day
- **Hourly View**: Shows 3-hour intervals with time, temperature, detailed weather
- **Single Click Again**: Return to daily view

### Power Management
- **Display Off**: After 1 minute of inactivity
- **Deep Sleep**: After 3 minutes of total inactivity
- **Wake Up**: Press joystick button

### Auto-Scroll Feature
- **In Weather Mode**: Single click when not viewing details toggles auto-scroll
- **Auto-scroll**: Cycles through days or hours every 2.5 seconds

## API Usage and Costs

### Google Directions API
- **Free Tier**: $200 monthly credit (≈40,000 requests)
- **Cost**: $5 per 1,000 requests after free tier
- **This Project**: ~2,880 requests/month (every 3 minutes, 24/7)
- **Estimated Cost**: Free tier should cover normal usage

### OpenWeatherMap API
- **Free Tier**: 1,000 calls/day
- **This Project**: ~288 calls/day (every 5 minutes when weather active)
- **Cost**: Free for this usage pattern

## Troubleshooting

### Common Issues

**"No buses found"**
- Check your Google API key is valid and active
- Verify billing is enabled on Google Cloud
- Ensure your coordinates are correct
- Check if there's actual bus service between your selected stops

**"No weather data"**
- Verify your OpenWeatherMap API key is active (can take 10 minutes)
- Check your city name spelling
- Ensure internet connectivity

**WiFi Connection Issues**
- Verify SSID and password in credentials.h
- Check signal strength at device location
- Try adding WiFi.mode(WIFI_STA) before WiFi.begin()

**Stack Overflow Errors**
- Reduce MAX_FORECASTS in weather.cpp
- Check available memory with ESP.getFreeHeap()

### Debug Mode
Enable detailed logging by setting `DEBUG = true` in the code and monitoring the serial output at 115200 baud.

## Power Optimization Features

- **Smart Data Fetching**: Only updates when display is active
- **WiFi Management**: Disconnects between requests
- **Deep Sleep**: Reduces power consumption during inactivity
- **Efficient Parsing**: Memory-optimized JSON processing

## Contributing

Feel free to submit issues, fork the repository, and create pull requests for any improvements.

## License

This project is open source. Please respect the terms of service for Google Maps API and OpenWeatherMap API when using this code.

---

**Note**: This project is for educational and personal use. Always comply with API terms of service and rate limits.