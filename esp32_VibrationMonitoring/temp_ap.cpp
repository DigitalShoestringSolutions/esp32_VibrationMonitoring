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

#include "temp_ap.h"

TempAP::TempAP()
  : local_ip(192, 168, 0, 1), gateway(192, 168, 0, 1), subnet(255, 255, 255, 0), ap_ssid("shoestring-esp32-setup"), ap_password("") {
  Serial.println("*** Temporary Config AP created ***");
}

TempAP::~TempAP() {
  Serial.println("*** Temporary Config AP removed ***");
}

void TempAP::start() {
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);

  Serial.print("[+] AP Created with Gateway IP ");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(53, "*", WiFi.softAPIP());

  Serial.print("[+] Device Mac ");
  Serial.println(WiFi.macAddress());

  Serial.println("*** Temporary Config AP started ***");
}

void TempAP::stop() {
  dnsServer.stop();
  WiFi.softAPdisconnect();

  Serial.println("*** Temporary Config AP stopped ***");
}

void TempAP::step_loop() {
  dnsServer.processNextRequest();
}
