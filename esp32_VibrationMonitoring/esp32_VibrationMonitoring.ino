// ----------------------------------------------------------------------
//
//   Limit Switch Detection
//
//   Copyright (C) 2022  Shoestring and University of Cambridge
//
//   This program is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, version 3 of the License.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program.  If not, see https://www.gnu.org/licenses/.
//
//----------------------------------------------------------------------
#include "shoestring_lib.h"
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <Adafruit_ADXL345_U.h>
#include "arduinoFFT.h"
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <ArduinoJson.h>
#include "Adafruit_MCP9808.h"

// FFT settings
const uint16_t samples = 1024; // Must be a power of 2
const double samplingFrequency = 300; // Adjust to your needs
unsigned int sampling_period_us;
float vReal[samples]; // Buffer for FFT input
float acceleration_buffer[samples]; // Buffer to be filledt
float vImag[samples]; // Imaginary part (not used but required by some FFT library functions)
bool bufferFull = false; // Indicates when the buffer is ready for FFT
int buff_start;
int sampleCounter = 0;
StaticJsonDocument<6000> JSONbuffer;

float buffer_total;
float buffer_mean; 

ArduinoFFT<float> FFT = ArduinoFFT<float>(vReal, vImag, samples, samplingFrequency, true);

#define SCL_INDEX 0x00
#define SCL_TIME 0x01
#define SCL_FREQUENCY 0x02
#define SCL_PLOT 0x03

// Screen Settings
#define TEXT_SIZE 1
#define TFT_CS  7
#define TFT_DC 39
#define TFT_RST  40
#define TFT_BACKLITE  45
#define TFT_I2C_POWER  21

// Initialize hardware objects
ShoestringLib shlib;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
Adafruit_ST7789 oled_display = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Sensor status tracking
struct SensorStatus {
    bool accel_ok = false;
    bool temp_ok = false;
    unsigned long last_retry = 0;
    const unsigned long RETRY_INTERVAL = 60000; // Retry every 60 seconds
} sensor_status;

// Task handles for the two tasks
TaskHandle_t Task1;
TaskHandle_t Task2;

// Other Variables
volatile bool interrupt_flag = false;
volatile bool value = true;
volatile bool prev_value = true;
bool debounced_value = true;
bool prev_debounced_value = true;
unsigned long interrupt_time;
unsigned long trigger_time;

// Error display helper
void displaySensorError(const char* message) {
    oled_display.fillScreen(ST77XX_BLACK);
    oled_display.setCursor(0, 30);
    oled_display.setTextColor(ST77XX_RED);
    oled_display.setTextSize(2);
    oled_display.println(message);
    delay(3000);
}

// Sensor initialization functions
bool initializeAccelerometer() {
    if (!accel.begin()) {
        Serial.println("ADXL345 not detected. Check wiring!");
        displaySensorError("ADXL345 Error\nCheck wiring!");
        return false;
    }
    accel.setRange(ADXL345_RANGE_16_G);
    return true;
}

bool initializeTemperatureSensor() {
    if (!tempsensor.begin(0x18)) {
        Serial.println("MCP9808 not detected. Check wiring!");
        displaySensorError("MCP9808 Error\nCheck wiring!");
        return false;
    }
    tempsensor.setResolution(3);
    return true;
}

// Periodic sensor check
void checkSensors() { // Where is this called? doesn't ever seem to be.
    Serial.println("Checking sensors");
    unsigned long now = millis();
    if (now - sensor_status.last_retry >= sensor_status.RETRY_INTERVAL) {
        sensor_status.last_retry = now;
        
        if (!sensor_status.accel_ok) {
            sensor_status.accel_ok = initializeAccelerometer();
        }
        
        if (!sensor_status.temp_ok) {
            sensor_status.temp_ok = initializeTemperatureSensor();
        }
        
        // Update display with current status
        oled_display.fillScreen(ST77XX_BLACK);
        oled_display.setCursor(0, 0);
        oled_display.setTextColor(ST77XX_RED);
        oled_display.setTextSize(2);
        oled_display.print("Accel: ");
        oled_display.println(sensor_status.accel_ok ? "OK" : "ERROR");
        oled_display.print("Temp:  ");
        oled_display.println(sensor_status.temp_ok ? "OK" : "ERROR");
        delay(3000);
    }
}


void Task1code(void * pvParameters){
  sensors_event_t event;
  buff_start = millis();
  sampling_period_us = round(1000000*(1.0/samplingFrequency));  // freq typ 300 -> 3,333 us
  Serial.print("sampling_period_us set as ");
  Serial.println(sampling_period_us);


  int period_start = micros();
  for(;;){
    // Check if accelerometer is working
    if(!sensor_status.accel_ok) {
      delay(1000); // Wait a second before checking again
      continue;
    }

    if(!bufferFull){
      accel.getEvent(&event);
      acceleration_buffer[sampleCounter] = event.acceleration.z + event.acceleration.x + event.acceleration.y;
      vImag[sampleCounter] = 0;
      sampleCounter++;     

      if(sampleCounter >= samples){
        sampleCounter = 0;
        bufferFull = true;
        // Serial.print("Buffer Filled in ");
        // int buff_time = millis()-buff_start;
        // Serial.println(buff_time);
      }
    }else{
      if (millis() > 500) {  // Update every 500ms? Will always be true except just after booting or for a moment every month or 2.
        Serial.print("Buffer Full! ");
    }
    }
    while (micros() - period_start < sampling_period_us ){ // What about overflows? micros() overflows every ~70 minutes, 32bit unsigned. 
      // Wait until it has been at least sampling_period_us since period_start
    }
    period_start += sampling_period_us; // period_start is created only with int period_start = micros(); above. Will it also overflow in the same way?
  }
}

void Task2code(void * pvParameters){
  for(;;) {
    shlib.loop();
  }
}

void setup() {
  shlib.addConfig("debounce_time", 20);
  shlib.setup();
  Serial.begin(9600);
  Serial.print("Starting Up...");

  // Initialize display first for error messages
  delay(1000);
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);
  oled_display.init(135, 240);  // Init ST7789 240x135
  oled_display.setRotation(3);
  oled_display.fillScreen(ST77XX_BLACK);
  oled_display.setTextWrap(false);
  oled_display.fillScreen(ST77XX_BLACK);
  oled_display.setCursor(0, 30);
  oled_display.setTextColor(ST77XX_RED);
  oled_display.setTextSize(2);
  oled_display.print("Starting...");
  delay(1000);

  // Initialize sensors with error handling
  // Keep trying to initialize sensors until both are working
  while (!sensor_status.accel_ok || !sensor_status.temp_ok) {
    if (!sensor_status.accel_ok) {
      oled_display.fillScreen(ST77XX_BLACK);
      oled_display.setCursor(0, 30);
      oled_display.print("Connecting to accel...");
      delay(1000);
      sensor_status.accel_ok = initializeAccelerometer();
    }
    if (!sensor_status.temp_ok) {
      oled_display.fillScreen(ST77XX_BLACK); 
      oled_display.setCursor(0, 30);
      oled_display.print("Connecting to temp...");
      delay(1000);
      sensor_status.temp_ok = initializeTemperatureSensor();
    }
    oled_display.print("Sensors Connected!");
    Serial.println("SensorsConnected!");
    delay(2000);
  }

  // Create tasks only if sensors are working
  if (sensor_status.accel_ok) {
    xTaskCreatePinnedToCore(
      Task1code, /* Task function. */
      "Task1",   /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task1,    /* Task handle to keep track of created task */
      0);        /* pin task to core 0 */ 
  }

  xTaskCreatePinnedToCore(
    Task2code, /* Task function. */
    "Task2",   /* name of task. */
    80000,     /* Stack size of task */
    NULL,      /* parameter of the task */
    1,         /* priority of the task */
    &Task2,    /* Task handle to keep track of created task */
    1);        /* pin task to core 1 */

  shlib.set_loop_hook(loop_callback);
  delay(5);
}

void loop() {
}

bool loop_callback(StaticJsonDocument<3000>& JSONdoc) {

  if(bufferFull && sensor_status.accel_ok){
    /// Do analysis here
    for(int i = 0; i < samples; i++) {
      vReal[i] = acceleration_buffer[i];
    }

    bufferFull = false;
    buff_start = millis();
    // Serial.println("Buffer Emptied");

    int start = millis();
    removeOffset(vReal);
    int static_offset_time = millis()-start;
    // Serial.print("removing offset took: ");
    // Serial.println(static_offset_time);

    start = millis();
    JSONdoc["acceleration"] = calculateRMS(vReal);
    int RMS_time = millis()-start;
    // Serial.print("RMS took: ");
    // Serial.println(RMS_time);

    start = millis();
    FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();
    float x;
    if (JSONdoc["acceleration"] > 0.2){x = FFT.majorPeak();} else {x = 0.00;}
    JSONdoc["peakFrequency"] = x;
    downSample(vReal, samples, JSONdoc);
    int fft_time = millis()-start;
    // Serial.print("FFT took: ");
    // Serial.println(fft_time);

    // Add temperature if sensor is working
    if(sensor_status.temp_ok) {
      JSONdoc["temperature"] = tempsensor.readTempC();
    } else {
      JSONdoc["temperature"] = -999.99; // Error value
    }

    // Add sensor status to JSON
    JSONdoc["sensors"]["accelerometer"] = sensor_status.accel_ok;
    JSONdoc["sensors"]["temperature"] = sensor_status.temp_ok;
    
    return true;
  }
  return false;
}

float calculateRMS(float *vData) {
  float squareSum = 0.0;
  for (int i = 0; i<samples; i++){
    squareSum = squareSum + sq(vData[i]);
  }
  float rms = sqrt(squareSum/samples);
  return rms;
}

void removeOffset(float *vData){
  buffer_total = 0;
  for (int i = 0; i<samples; i++){
    buffer_total = buffer_total + vData[i];
  }
  buffer_mean = buffer_total / samples;

  for (int i = 0; i<samples; i++){
    vData[i] = vData[i] - buffer_mean;
  }

}

void downSample(float *vData, uint16_t bufferSize, StaticJsonDocument<3000>& JSONdoc){
  uint16_t freq_bands = 10; // Hz range per band
  uint16_t n_bands = (samplingFrequency*0.5)/freq_bands;
  uint16_t samples_per_band = 0.5*(bufferSize/n_bands);
  double downsampledData[n_bands];

  

  for (uint16_t i = 0; i < n_bands+1; i++) {
    int frequency = i * freq_bands;
    double mag_max = 0.0;
    for (uint16_t j = i * samples_per_band; j < (i + 1) * samples_per_band; j++) {\
      if (vData[j] > mag_max) {
          mag_max = vData[j];
      }    
    }
    int ascii_int;
    char tag_string[15];
    char freq_string[10];
    ascii_int = 65 + i;
    tag_string[0] = ascii_int;
    tag_string[1] = '\0';

    snprintf(freq_string, sizeof(freq_string), "-%d", frequency);
    strcat(tag_string, freq_string);
  
    JSONdoc["fft"][i]["frequency"] = tag_string;
    JSONdoc["fft"][i]["magnitude"] = mag_max;
    
    // Serial.println(tag_string);
    // Serial.print(frequency);
    // Serial.print("Hz ");
    // Serial.println(mag_max);
  }
}

void PrintVector(float *vData, uint16_t bufferSize, uint8_t scaleType) // where is this used?
{
  for (uint16_t i = 0; i < bufferSize; i++)
  {
    float abscissa;
    /* Print abscissa value */
    switch (scaleType)
    {
      case SCL_INDEX:
        abscissa = (i * 1.0);
	break;
      case SCL_TIME:
        abscissa = ((i * 1.0) / samplingFrequency);
	break;
      case SCL_FREQUENCY:
        abscissa = ((i * 1.0 * samplingFrequency) / samples);
	break;
    }
    Serial.print(abscissa, 6);
    if(scaleType==SCL_FREQUENCY)
      Serial.print("Hz");
    Serial.print(" ");
    Serial.println(vData[i], 4);
  }
  Serial.println();
}

char get_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long int milliseconds = (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
    return milliseconds;
}
