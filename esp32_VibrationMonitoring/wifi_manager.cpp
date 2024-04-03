// ----------------------------------------------------------------------
//
//   Config Manager for ESP32 devices
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
// ----------------------------------------------------------------------

#include "wifi_manager.h"
#include "config_display.h"
#include <WiFi.h>

extern ConfigDisplay display;

WifiManager::WifiManager(ConfigManager *cm_ptr)
  : state(STA_STATE::start), last_state(STA_STATE::none), config_manager_ptr(cm_ptr) {
  Serial.println("*** WIFI Manager online ***");
}

WifiManager::~WifiManager() {
  Serial.println("*** WIFI Manager offline ***");
}

void WifiManager::setup() {
  Serial.println("*** WIFI Manager Setup ***");
  WiFi.mode(WIFI_AP_STA);
  WiFi.mode(WIFI_AP);
  TempAP ap;

  pinMode(13, OUTPUT);
  bool led = false;
  long last = millis();

  while (state != STA_STATE::connected) {
    step_loop(ap);
    led_delay();
  }

  Serial.print("Connected at:");
  Serial.println(WiFi.localIP());
  digitalWrite(13, false);

  display.setWiFiSSID(config_manager_ptr->getWiFiSSID());
  display.setWiFiStatus("Connected");
  display.setIP(WiFi.localIP().toString());
  // flash_ip(WiFi.localIP().toString());
}

void WifiManager::flash_ip(String ip) {
  auto pulse = [](int duration) {
    digitalWrite(13, true);
    delay(duration / 2);
    digitalWrite(13, false);
    delay(duration / 2);
  };
  digitalWrite(13, false);
  delay(2000);
  for (uint8_t i = 0; i < ip.length(); i++) {
    char c = ip.charAt(i);
    if (c == '.') {
      delay(1000);
      Serial.print(".");
    } else {
      uint8_t val = c - '0' + 1;
      Serial.println(val);
      for (val; val > 0; val--) {
        pulse(500);
        Serial.print("*");
      }
      delay(500);
    }
  }
}

void WifiManager::print_state() {
  switch (state) {
    case STA_STATE::start:
      Serial.println("><> Start");
      display.setWiFiStatus("Started");
      break;
    case STA_STATE::ap:
      Serial.println("><> AP");
      display.setWiFiStatus("AP mode");
      break;
    case STA_STATE::try_existing:
      Serial.println("><> Try Exist");
      display.setWiFiStatus("Connecting...");
      break;
    case STA_STATE::try_new:
      Serial.println("><> Try New");
      display.setWiFiStatus("New credentials");
      break;
    case STA_STATE::pre_connected_delay:
      Serial.println("><> Connected-Delay");
      break;
    case STA_STATE::connected:
      Serial.println("><> Connected");
      break;
  }
}

//counts 0 - 10 with 100ms delay
void WifiManager::led_delay() {
  static bool value = false;
  static int counter = 0;
  switch (state) {
    case STA_STATE::start:
      digitalWrite(13, true);
      break;
    case STA_STATE::ap:
      if (counter == 0) {
        digitalWrite(13, value);
        value = !value;
      }
      break;
    case STA_STATE::try_existing:
      digitalWrite(13, true);
      break;
    case STA_STATE::try_new:  // on, on, off, off, on, on, ...
      digitalWrite(13, (counter / 2) % 2 == 0);
      break;
    case STA_STATE::connected:
      digitalWrite(13, false);
      break;
  }
  delay(100);
  counter++;
  if (counter >= 10) {
    counter = 0;
  }
}

void WifiManager::step_loop(TempAP &ap) {
  bool on_entry = state != last_state;
  if (on_entry) {
    print_state();
  }
  last_state = state;
  STA_STATE next_state = state;

  switch (state) {
    case STA_STATE::start:
      next_state = handle_start(on_entry);
      break;
    case STA_STATE::try_existing:
      next_state = handle_try_existing(on_entry);
      break;
    case STA_STATE::ap:
      next_state = handle_ap(on_entry, ap);
      break;
    case STA_STATE::try_new:
      next_state = handle_try_new(on_entry);
      break;
    case STA_STATE::pre_connected_delay:
      static int count = 0;
      if (count < 300) {
        count++;
        next_state = STA_STATE::pre_connected_delay;
      } else {
        ap.stop();
        WiFi.mode(WIFI_STA);
        next_state = STA_STATE::connected;
      }
      break;
  }

  config_manager_ptr->step_loop();

  state = next_state;
}

STA_STATE WifiManager::handle_start(bool on_entry) {
  this->config_manager_ptr->setup_wifi();
  current_ssid = this->config_manager_ptr->getWiFiSSID();
  current_password = this->config_manager_ptr->getWiFiPassword();

  if (current_ssid == "" || current_password == "") {
    return STA_STATE::ap;
  } else {
    return STA_STATE::try_existing;
  }
}

STA_STATE WifiManager::handle_try_existing(bool on_entry) {
  static int try_counter = 0;

  if (on_entry) {
    if (!WiFi.disconnect()) {
      Serial.println("Wifi Failed to disconnect properly");
    };
    if (!WiFi.softAPdisconnect()) {
      Serial.println("Wifi AP Failed to disconnect properly");
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(current_ssid.c_str(), current_password.c_str());
    display.setWiFiSSID(current_ssid);
    Serial.println("[*] Attempting connection with existing credentials");
    try_counter = 0;
    this->config_manager_ptr->setup();
  }

  if (WiFi.status() == WL_CONNECTED) {
    return STA_STATE::connected;
    Serial.println("\n[+] Wifi connected with existing credentials");
  } else if (try_counter < 300) {  //try connect for 30 seconds
    if (try_counter % 10 == 0) { Serial.print("."); }
    try_counter++;
    return STA_STATE::try_existing;
  }

  Serial.println("\n[+] Unable to connect - openning AP mode while retrying");
  retry_mode = true;
  return STA_STATE::ap;
}

STA_STATE WifiManager::handle_ap(bool on_entry, TempAP &ap) {
  static int try_counter = 0;

  if (on_entry) {
    if (!WiFi.disconnect()) {
      Serial.println("Wifi Failed to disconnect properly");
    };
    WiFi.mode(WIFI_AP_STA);
    ap.start();
    display.setWiFiSSID(ap.getSSID());
    try_counter = 0;
    this->config_manager_ptr->setup();
  }

  ap.step_loop();

  if (config_manager_ptr->get_state() == WIFI_CREDS_STATE::checking) {
    this->config_manager_ptr->get_new_credentials(new_ssid, new_password);
    return STA_STATE::try_new;
  } else if (current_ssid == "" || current_password == "") {
    return STA_STATE::ap;                                                                                                   //No existing credentials - continue to wait as AP
  } else {                                                                                                                  //Existing credentials - try again when timeout occurs
    if (try_counter < 1800 || (config_manager_ptr->get_state() == WIFI_CREDS_STATE::entering_new && try_counter < 3000)) {  // 3 min timeout or 5 min if form entry ongoing
      try_counter++;
      return STA_STATE::ap;
    } else {
      ap.stop();
      return STA_STATE::try_existing;
    }
  }
}

STA_STATE WifiManager::handle_try_new(bool on_entry) {
  static int try_counter = 0;

  if (on_entry) {
    if (!WiFi.disconnect()) {
      Serial.println("Wifi Failed to disconnect properly");
    };
    display.setWiFiSSID(new_ssid);
    WiFi.begin(new_ssid.c_str(), new_password.c_str());
    Serial.println("\n[*] Trying to connect to new WiFi network: " + new_ssid);
    try_counter = 0;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[+] Wifi connected with new credentials");
    config_manager_ptr->set_wifi_creds();
    return STA_STATE::pre_connected_delay;
  }

  if (try_counter < 600)  //try for 1 minute
  {
    if (try_counter % 10 == 0) { Serial.print("-"); }
    try_counter++;
    return STA_STATE::try_new;
  }

  Serial.println("\n[+] Wifi connection Failed with new credentials");
  config_manager_ptr->creds_failed();
  return STA_STATE::ap;
}
