#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <sys/time.h>
#include "config.h"

// Create instances
WiFiUDP udp;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Variables for non-blocking timing
unsigned long lastDisplayUpdate = 0;

// Buffer for sensor data
struct SensorData {
  float x;
  float y;
  float z;
  float magnitude;  // Combined magnitude of acceleration
} sensorData;

// Task frequency measurement
struct TaskFrequency {
  unsigned long count;
  unsigned long lastResetTime;
  float frequency;
  unsigned long lastMicros;  // For precise timing
} sensorTaskFreq = {0, 0, 0.0, 0}, udpTaskFreq = {0, 0, 0.0, 0};

// Mutex for protecting sensor data and frequency measurements
SemaphoreHandle_t sensorDataMutex;

// Display states
enum DisplayState {
  CONNECTING,
  CONNECTED,
  SENSOR_ERROR,
  SENDING_DATA
};

DisplayState currentState = CONNECTING;

// Pre-allocate string buffer for UDP transmission
char udpBuffer[32];



void updateDisplay() {
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setCursor(0, 0);
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(2);
  
  switch(currentState) {
    case CONNECTING:
      tft.println("Connecting...");
      break;
    case CONNECTED:
      tft.setTextColor(SUCCESS_COLOR);
      tft.println("Connected!");
      tft.setTextColor(TEXT_COLOR);
      tft.println();
      tft.println("IP:");
      tft.println(WiFi.localIP());
      break;
    case SENSOR_ERROR:
      tft.setTextColor(WARNING_COLOR);
      tft.println("Sensor Error!");
      break;
    case SENDING_DATA:
      tft.println("Accelerometer");
      tft.println();
      tft.print("Magnitude: ");
      tft.println(sensorData.magnitude, 1);
      tft.println();
      tft.print("Sensor Hz: ");
      tft.println(sensorTaskFreq.frequency, 1);
      tft.print("UDP Hz: ");
      tft.println(udpTaskFreq.frequency, 1);
      break;
  }
}

// Task for reading sensor data
void sensorTask(void *pvParameters) {
  const TickType_t xFrequency = pdMS_TO_TICKS(1000 / SAMPLE_RATE);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  
  while(1) {
    unsigned long startMicros = micros();
    
    sensors_event_t event;
    accel.getEvent(&event);
    
    // Take mutex before updating shared data
    if (xSemaphoreTake(sensorDataMutex, portMAX_DELAY) == pdTRUE) {
      sensorData.magnitude = abs(event.acceleration.x) + abs(event.acceleration.y) + abs(event.acceleration.z);
      
      // Update frequency measurement with microsecond precision
      sensorTaskFreq.count++;
      unsigned long currentTime = millis();
      if (currentTime - sensorTaskFreq.lastResetTime >= DISPLAY_UPDATE_INTERVAL) {
        // Calculate actual frequency (samples per second)
        unsigned long elapsedMicros = startMicros - sensorTaskFreq.lastMicros;
        sensorTaskFreq.frequency = (float)sensorTaskFreq.count * 1000000.0 / (float)elapsedMicros;
        sensorTaskFreq.lastMicros = startMicros;
        sensorTaskFreq.count = 0;
        sensorTaskFreq.lastResetTime = currentTime;
      }
      
      xSemaphoreGive(sensorDataMutex);
    }
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// Task for sending UDP data
void udpTask(void *pvParameters) {
  const TickType_t xFrequency = pdMS_TO_TICKS(SEND_INTERVAL);
  TickType_t xLastWakeTime = xTaskGetTickCount();
  
  while(1) {
    unsigned long startMicros = micros();
    float currentMagnitude;
    
    // Take mutex to read sensor data
    if (xSemaphoreTake(sensorDataMutex, portMAX_DELAY) == pdTRUE) {
      currentMagnitude = sensorData.magnitude;
      
      
      // Update frequency measurement with microsecond precision
      udpTaskFreq.count++;
      unsigned long currentTime = millis();
      if (currentTime - udpTaskFreq.lastResetTime >= DISPLAY_UPDATE_INTERVAL) {
        // Calculate actual frequency (samples per second)
        unsigned long elapsedMicros = startMicros - udpTaskFreq.lastMicros;
        udpTaskFreq.frequency = (float)udpTaskFreq.count * 1000000.0 / (float)elapsedMicros;
        udpTaskFreq.lastMicros = startMicros;
        udpTaskFreq.count = 0;
        udpTaskFreq.lastResetTime = currentTime;
      }
      
      xSemaphoreGive(sensorDataMutex);
    }
    
    // Format and send data with timestamp
    snprintf(udpBuffer, sizeof(udpBuffer), "%.2f", currentMagnitude);
    // Serial.printf("UDP Buffer contents: %s\n", udpBuffer);  // Debug print buffer contents
    
    udp.beginPacket(SERVER_IP, SERVER_PORT);
    udp.print(udpBuffer);
    udp.endPacket();
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Create mutex for protecting sensor data
  sensorDataMutex = xSemaphoreCreateMutex();

  // Initialize display
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);
  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(BACKGROUND_COLOR);
  updateDisplay();

  // Initialize WiFi with optimized settings
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    updateDisplay();
  }
  
  currentState = CONNECTED;
  updateDisplay();
  
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize ADXL345 with optimized settings
  if (!accel.begin()) {
    Serial.println("Could not find a valid ADXL345 sensor, check wiring!");
    currentState = SENSOR_ERROR;
    updateDisplay();
    while (1);
  }
  
  // Set range and data rate for maximum performance
  accel.setRange(ADXL345_RANGE_16_G);
  accel.setDataRate(ADXL345_DATARATE_800_HZ); // Increased to 800Hz to ensure we can achieve 400Hz sampling
  
  Serial.println("ADXL345 initialized");
  currentState = SENDING_DATA;
  updateDisplay();

  // Initialize frequency measurement timers
  sensorTaskFreq.lastResetTime = millis();
  udpTaskFreq.lastResetTime = millis();
  sensorTaskFreq.lastMicros = micros();
  udpTaskFreq.lastMicros = micros();

  // Create tasks
  xTaskCreatePinnedToCore(
    sensorTask,    // Task function
    "SensorTask", // Task name
    4096,         // Stack size
    NULL,         // Task parameters
    2,            // Priority (higher number = higher priority)
    NULL,         // Task handle
    0             // Core to run on (Core 0)
  );

  xTaskCreatePinnedToCore(
    udpTask,      // Task function
    "UDPTask",    // Task name
    4096,         // Stack size
    NULL,         // Task parameters
    1,            // Priority (lower than sensor task)
    NULL,         // Task handle
    1             // Core to run on (Core 1)
  );
}

void loop() {
  unsigned long currentTime = millis();

  // Update display at specified interval
  if (currentTime - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = currentTime;
  }

  // Small delay to prevent watchdog timer issues
  vTaskDelay(pdMS_TO_TICKS(10));
} 