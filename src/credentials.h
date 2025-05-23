// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Google API settings
const char* apiKey = "YOUR_API_KEY";
// Replace with your bus station coordinates
const char* busStopLatLng = "latitude, longitude"; 

const char* originStopLatLng = "latitude, longitude"; //Your bus stop
// Destination bus stop coordinates (a nearby stop you'd typically go to -
// e.g. if all busses that departue from your nearest bus stop next go to a specific bus stop, then u should place coordinates of that bus stop there)
// i hadn't found any other ways to do that, so if u odnt have such bus stop, then part of the busses wouldnt be mentioned on your schedule as the system 
// wouldnt be able to track them
const char* destStopLatLng = "latitude, longitude"; //Next stop

// OpenWeatherMap API details
const char* weatherApiKey = "YOUR_OPENWEATHERMAP_API_KEY";
const char* city = "City";  // Your city name
const char* units = "metric"; // metric, imperial, or kelvin
