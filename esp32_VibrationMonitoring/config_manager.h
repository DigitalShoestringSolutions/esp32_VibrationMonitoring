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

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <vector>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFi.h>

enum class WIFI_CREDS_STATE { idle_invalid,
                              entering_new,
                              checking,
                              valid };

static Preferences preferences;

#define RO_MODE true
#define RW_MODE false

#undef RESET_WIFI

/*
* NOTICE: the name must be <=15 characters for the preferences library to work correctly
*/
class ConfigItem {
public:
  ConfigItem(String name, String default_value = "") {
    this->name = name;
    preferences.begin("app_config", RO_MODE);
    this->string_value = preferences.getString(name.c_str(), default_value);
    this->is_string = true;
    preferences.end();
  }
  ConfigItem(String name, int default_value = 0) {
    this->name = name;
    preferences.begin("app_config", RO_MODE);
    this->int_value = preferences.getInt(name.c_str(), default_value);
    this->is_string = false;
    preferences.end();
  }
  String name;
  String getString() {
    return string_value;
  }
  int getInt() {
    return int_value;
  }
  bool isString() {
    return is_string;
  }
  size_t setString(String value) {
    this->string_value = value;
    preferences.begin("app_config", RW_MODE);
    size_t written = preferences.putString(this->name.c_str(), value);
    preferences.end();
    return written;
  }

  size_t setInt(int value) {
    this->int_value = value;
    preferences.begin("app_config", RW_MODE);
    size_t written = preferences.putInt(this->name.c_str(), value);
    preferences.end();
    return written;
  }
private:
  bool is_string;
  int int_value;
  String string_value;
};

class ConfigManager;

class ConfigManager {
public:
  ConfigManager()
    : server(80), wifi_creds_state(WIFI_CREDS_STATE::idle_invalid) {
    Serial.println("*** Config Manager online ***");
  }

  void setup_wifi() {
    preferences.begin("wifi_creds", RO_MODE);
#ifdef RESET_WIFI
    ssid = "";
    password = "";
#else
    ssid = preferences.getString("ssid");
    password = preferences.getString("password");
#endif
    Serial.println("ssid: " + ssid);
    preferences.end();
  }

  void setup() {
    if (!started) {
      Serial.println("*** Config Manager Setup ***");
      server.on("/", std::bind(&ConfigManager::handle_home, this));
      server.on("/wifi", std::bind(&ConfigManager::handle_wifi_form, this));
      server.on("/config", std::bind(&ConfigManager::handle_config_form, this));
      server.on("/submit_config", HTTP_POST, std::bind(&ConfigManager::handle_config_submit, this));
      server.on("/submit_wifi", HTTP_POST, std::bind(&ConfigManager::handle_wifi_submit, this));
      server.on("/status_wifi", std::bind(&ConfigManager::handle_wifi_status_check, this));
      server.onNotFound(std::bind(&ConfigManager::handle_NotFound, this));
      server.begin();
      started = true;
    }
  }

  void set_wifi_creds() {
    this->ssid = this->new_ssid;
    this->password = this->new_password;

    //save to "EEPROM"
    preferences.begin("wifi_creds", RW_MODE);
    size_t s_res = preferences.putString("ssid", new_ssid);
    size_t p_res = preferences.putString("password", new_password);

    if(s_res != new_ssid.length() || p_res!= new_password.length()){
      Serial.println("<><><><><> BAD CRED WRITE <><><><><><>");
    }

    preferences.end();
    this->wifi_creds_state = WIFI_CREDS_STATE::valid;

    Serial.println("!!! New WiFi credentials for " + new_ssid + " saved !!!");
  }

  void creds_failed() {
    this->wifi_creds_state = WIFI_CREDS_STATE::idle_invalid;
  }

  void register_item(ConfigItem item) {
    this->items.push_back(item);
  }

  String getString(String key) {
    for (ConfigItem item : this->items) {
      if (item.name == key) {
        return item.getString();
      }
    }
    return "";
  }

  int getInt(String key) {
    for (ConfigItem item : this->items) {
      if (item.name == key) {
        return item.getInt();
      }
    }
    return -1;
  }

  String getWiFiSSID() {
    return ssid;
  }
  String getWiFiPassword() {
    return password;
  }

  void step_loop() {
    server.handleClient();
  }

  WIFI_CREDS_STATE get_state() {
    return this->wifi_creds_state;
  }
  void get_new_credentials(String &ssid, String &password) {
    ssid = this->new_ssid;
    password = this->new_password;
  }

private:
  WebServer server;
  std::vector<ConfigItem> items = {};
  String ssid;
  String password;
  String new_ssid;
  String new_password;
  WIFI_CREDS_STATE wifi_creds_state;
  bool started = false;

  void handle_home() {
    String html = R"(
        <!DOCTYPE html>
        <html>
        <body>
          <h1>Configuration</h1>
          <button onclick="window.location.href='/wifi';">
            Set up WiFi
          </button>
          <button onclick="window.location.href='/config';">
            Set up Service Module Config
          </button>
        </body>
        </html>
      )";

    server.send(200, "text/html", html);
  }
  void handle_config_form() {
    String html_header = R"(
        <!DOCTYPE html>
        <html>
        <body>
        <h1>Configuration</h1>
          <form action="/submit_config" method="post">)";

    String html_footer = R"(
            <input type="submit" value="Submit">
          </form> 

        </body>
        </html>
      )";

    String contents = "";
    for (ConfigItem item : this->items) {
      contents += "<label for=\"" + item.name + "\">" + item.name + ":</label><br>";
      if (item.isString()) {
        contents += "<input type=\"text\" id=\"" + item.name + "\" name=\"" + item.name + "\" value=\"" + item.getString() + "\"><br>";
      } else {
        contents += "<input type=\"number\" id=\"" + item.name + "\" name=\"" + item.name + "\" value=\"" + item.getInt() + "\"><br>";
      }
    }
    server.send(200, "text/html", html_header + contents + html_footer);
  }

  void handle_wifi_form() {
    this->wifi_creds_state = WIFI_CREDS_STATE::entering_new;

    String html_header = R"(
        <!DOCTYPE html>
        <html>
        <body>
        <h1>Wifi Configuration</h1>
        <form action="/submit_wifi" method="post">)";
    String contents = "";
    contents += "<label for=\"ssid\">SSID:</label><br>";
    contents += "<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"" + ssid + "\"><br>";
    contents += "<label for=\"password\">Password:</label><br>";
    contents += "<input type=\"text\" id=\"password\" name=\"password\" value=\"" + password + "\"><br><br>";

    String html_footer = R"(
          <input type="submit" value="Submit">
        </form> 

        </body>
        </html>
      )";
    server.send(200, "text/html", html_header + contents + html_footer);
  }

  void handle_NotFound() {
    if (WiFi.status() != WL_CONNECTED) {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString());
      server.send(302, "text/plain", "Found");
      Serial.println("redirect to http://" + WiFi.softAPIP().toString());
    } else {
      Serial.println("Not Found");
      server.send(404, "text/plain", "Not Found");
    }
  }
  void handle_config_submit() {
    for (int i = 0; i < this->items.size(); i++) {
      String new_value = server.arg(this->items.at(i).name);

      if (this->items.at(i).isString()) {
        size_t written = this->items.at(i).setString(new_value);
        Serial.println(this->items.at(i).name + " <S< " + this->items.at(i).getString()+" ["+written+"]");
      } else {
        size_t written = this->items.at(i).setInt(new_value.toInt());
        Serial.println(this->items.at(i).name + " <I< " + this->items.at(i).getInt()+" ["+written+"]");
      }
    }
    server.send(200, "text/html", "Saved");
  }

  void handle_wifi_submit() {
    this->new_ssid = server.arg("ssid");
    this->new_password = server.arg("password");
    this->wifi_creds_state = WIFI_CREDS_STATE::checking;

    String html = R"(
        <!DOCTYPE html>
        <html>
        <body>
        <h1>Status:</h2>
        <h2 id="status">Please wait</h2>
        <script>
          async function check_status() {
            setTimeout(check_status, 500);
            const post = await fetch("/status_wifi").then((res) => res.json());
            let val = "...loading...";
            switch(post.status) {
              case "pending": val = "Trying to connect to WiFi Network"; break;
              case "invalid": val = "Unable to connect to WiFi Network, please enter valid credentials"; break;
              case "valid": val = "Connected! This temporary access point will now close"; break;
              case "waiting": val = "Waiting for form input"; break;
            }
            document.getElementById("status").innerText = val;
          }
          setTimeout(check_status, 1);
        </script>
        </body>
        </html>
      )";

    server.send(200, "text/html", html);
  }

  void handle_wifi_status_check() {
    String value = "";
    switch (this->wifi_creds_state) {
      case WIFI_CREDS_STATE::valid: value = "valid"; break;
      case WIFI_CREDS_STATE::checking: value = "pending"; break;
      case WIFI_CREDS_STATE::idle_invalid: value = "invalid"; break;
      default: value = "waiting";
    }
    // Serial.println("status check: "+value);
    server.send(200, "text/json", "{\"status\":\"" + value + "\"}");
  }
};

#endif