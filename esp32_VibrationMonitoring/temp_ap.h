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

#ifndef WCM_TEMP_AP_H
#define WCM_TEMP_AP_H

#include <DNSServer.h>
#include <WiFi.h>


class TempAP;

class TempAP{
  public:
    TempAP();
    ~TempAP();
    void start();
    void stop();
    void step_loop();
    
    String getSSID(){return ap_ssid;}
    
  private:
    DNSServer dnsServer;

    String ap_ssid;
    String ap_password;

    const IPAddress local_ip;
    const IPAddress gateway;
    const IPAddress subnet;
};

#endif