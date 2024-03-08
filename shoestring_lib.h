#ifndef SHOESTRING_LIB_H
#define SHOESTRING_LIB_H

// #define WAIT_FOR_SERIAL

#include "wifi_manager.h"
#include "config_manager.h"
#include "config_display.h"

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Wire.h>

class MapEntry {
  public:
    MapEntry(){};
    MapEntry(String key, String value):key(key), value(value){};
    String key;
    String value;
};

class OutputMap {
  public:
    OutputMap(){};
    void add_entry(String key,String value){
      this->items.push_back(MapEntry(key,value));
    };
    std::vector<MapEntry> items = {};
};

class ShoestringLib {
public:
  ShoestringLib(){};
  void setup();
  void loop();
  void set_loop_hook(std::function<bool(ArduinoJson::StaticJsonDocument<3000>&)> callback) {
    this->callback = callback;
  };

  void printLocalTime();
  void get_timestamp();
  void get_timestamp_ms();


  int getInt(String key){return cm.getInt(key);};
  String getString(String key){return cm.getString(key);};
  void addConfig(String key,String value){cm.register_item(ConfigItem(key, value));}
  void addConfig(String key,int value){cm.register_item(ConfigItem(key, value));}

private:
  ConfigManager cm;
  std::function<bool(ArduinoJson::StaticJsonDocument<3000>&)> callback;
  String current_mqtt_server_addr = "";
  int current_mqtt_server_port = 0;
  char timestamp_buffer[80];
  long mqttConnectTimestamp = 0;

  void reconnect();
};


#endif