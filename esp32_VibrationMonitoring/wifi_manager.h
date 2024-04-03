#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "config_manager.h"
#include "temp_ap.h"

/**********************************************************
 * Attempts to connect to wifi with the stored credentials
 * and creates a temporary access point to receive new 
 * credentials via the config manager if unable to connect.
 * 
 * This code follows the following state machine:
 *                  
 *       has creds  ┌─────┐  no creds
 *  ┌───────────────┤Start├─────────┐
 *  │               └─────┘         │
 *  │                               │
 *  │                               │
 * ┌▼───────────┐   Fail      ┌─────▼───┐
 * │Try Existing├─────────────► AP Mode │
 * │            │   Timeout   │         │
 * │            ◄─────────────┤         │
 * │            │   Timeout   │         │
 * │            ├─────────────►         │
 * └─┬──────────┘             └─────┬─▲─┘
 *   │Success             New Creds │ │ Timeout
 * ┌─▼───────────┐  Success   ┌─────▼─┴─┐
 * │Connected    ◄────────────┤Try New  │
 * └─────────────┘            └─────────┘
 *
 * Timeouts from AP Mode to Try Existing only occur if
 * there is an existing set of credentials to try.
 **/

enum class STA_STATE {none,start, try_existing, ap, try_new, pre_connected_delay, connected};

class WifiManager;

class WifiManager{
  public:
    WifiManager(ConfigManager* cm_ptr);
    ~WifiManager();
    void setup();

  private:
    ConfigManager* config_manager_ptr;

    STA_STATE state;
    STA_STATE last_state;

    String new_ssid;
    String new_password;
    String current_ssid;
    String current_password;

    bool retry_mode = false;

    void step_loop(TempAP &ap);

    void print_state();
    void led_delay();
    void flash_ip(String ip);

    STA_STATE handle_start(bool on_entry);
    STA_STATE handle_try_existing(bool on_entry);
    STA_STATE handle_ap(bool on_entry, TempAP &ap);
    STA_STATE handle_try_new(bool on_entry);
    STA_STATE handle_retry(bool on_entry);
};

#endif