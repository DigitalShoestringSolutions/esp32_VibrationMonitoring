#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
const char* WIFI_SSID = "VM7772780";
const char* WIFI_PASSWORD = "nph3ytywFbtx";

// UDP Server Configuration
const char* SERVER_IP = "192.168.0.101";  // Replace with your server's IP
const int SERVER_PORT = 50000;            // Replace with your desired port

// Sensor Configuration
const int SAMPLE_RATE = 400;  // Hz
const int SEND_INTERVAL = 2; // ms (1000ms/400Hz = 2.5ms, rounded down to 2ms for stability)

// Display Configuration
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;
#define TEXT_SIZE 1
#define TFT_CS  7
#define TFT_DC 39
#define TFT_RST  40
#define TFT_BACKLIGHT  45
#define TFT_I2C_POWER  21

// Display Colors
#define BACKGROUND_COLOR ST77XX_BLACK
#define TEXT_COLOR      ST77XX_WHITE
#define WARNING_COLOR   ST77XX_RED
#define SUCCESS_COLOR   ST77XX_GREEN

#endif 