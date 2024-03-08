#ifndef CONFIG_DISPLAY_H
#define CONFIG_DISPLAY_H

#include <Wire.h>

#define S_DISPLAY

#ifdef S_DISPLAY
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#endif

#define TEXT_SIZE 1
#define TFT_CS  7
#define TFT_DC 39
#define TFT_RST  40
#define TFT_BACKLITE  45
#define TFT_I2C_POWER  21

class ConfigDisplay {
public:
  ConfigDisplay()
#ifdef S_DISPLAY
    : oled_display(Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST))
#endif
  {
  }
  void initialise() {
    delay(1000);
    pinMode(TFT_BACKLITE, OUTPUT);
    digitalWrite(TFT_BACKLITE, HIGH);
    pinMode(TFT_I2C_POWER, OUTPUT);
    digitalWrite(TFT_I2C_POWER, HIGH);
    delay(10);
#ifdef S_DISPLAY
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
#endif
  }
  void setWiFiStatus(String state) {
    // Serial.println("setwifi");
    this->wifi_state = state;
    refresh();
  };
  void setWiFiSSID(String ssid) {
    // Serial.println("setwifi");
    this->wifi_ssid = ssid;
    refresh();
  };
  void setIP(String ip) {
    // Serial.println("setip");
    this->ip = ip;
    refresh();
  }
  void setMQTTStatus(String mqtt_status) {
    // Serial.println("setmqtt");
    this->mqtt_state = mqtt_status;
    refresh();
  }
  void setMQTTIP(String ip) {
    // Serial.println("setip");
    this->mqtt_ip = ip;
    refresh();
  }


private:
#ifdef S_DISPLAY
  Adafruit_ST7789 oled_display;
#endif
  String wifi_state = "";
  String wifi_ssid = "";
  String ip = "";
  String mqtt_state = "";
  String mqtt_ip = "";

  void refresh() {
    // Serial.println("refresh");
#ifdef S_DISPLAY
    oled_display.fillScreen(ST77XX_BLACK);
    gotoline(0);
    oled_display.print("WIFI: ");
    oled_display.println(wifi_state);
    gotoline(1);
    oled_display.print("SSID: ");
    oled_display.println(wifi_ssid);
    gotoline(2);
    oled_display.print("IP:   ");
    oled_display.println(ip);
    gotoline(4);
    oled_display.print("MQTT: ");
    oled_display.println(mqtt_state);
    gotoline(5);
    oled_display.print("@");
    oled_display.println(mqtt_ip);
#endif
  }

  void gotoline(int y) {
#ifdef S_DISPLAY
    oled_display.setCursor(0, y * TEXT_SIZE * 16);
#endif
  }
};

#endif