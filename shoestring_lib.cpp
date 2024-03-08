#include "shoestring_lib.h"


// General Variables
const char* ntpServer = "pool.ntp.org";
unsigned long lastTriggerTime;

#define I2C2_SDA_PIN 16
#define I2C2_SCL_PIN 15

WiFiClient espClient;
PubSubClient client(espClient);
ConfigDisplay display;


void ShoestringLib::setup() {
  //set up serial communication
  Serial.begin(115200);
#ifdef WAIT_FOR_SERIAL
  while (true) {
    if (Serial.available() >= 2) {
      char in = Serial.read();
      if (in == 'g') {
        in = Serial.read();
        if (in == 'o')
          break;
      }
      Serial.print(in);
    }
  }
#endif
  Serial.println("+++++++++++START+++++++++++");
  display.initialise();

  //set up config manager
  cm.register_item(ConfigItem("mqtt_url", "127.0.0.1"));
  cm.register_item(ConfigItem("mqtt_port", 1883));
  cm.register_item(ConfigItem("mqtt_topic", "equipment_monitoring/count_trigger"));
  cm.register_item(ConfigItem("identifier", "machine_1"));

  // set up wifi using wifi manager
  WifiManager wm(&cm);
  wm.setup();

  // set up time
  configTime(0, 0, ntpServer);
  printLocalTime();
  lastTriggerTime = 0;

  // setup client
  client.setBufferSize(4000);

}

void ShoestringLib::reconnect() {
  long now = millis();
  if (current_mqtt_server_addr != cm.getString("mqtt_url") || current_mqtt_server_port != cm.getInt("mqtt_port")) {
    current_mqtt_server_addr = cm.getString("mqtt_url");
    current_mqtt_server_port = cm.getInt("mqtt_port");
    display.setMQTTIP(current_mqtt_server_addr+":"+current_mqtt_server_port);
    client.setServer(current_mqtt_server_addr.c_str(), current_mqtt_server_port);
    Serial.println("Attempting MQTT connection to " + current_mqtt_server_addr + ":" + current_mqtt_server_port + "...");
    mqttConnectTimestamp = now - 16000;  //force retry
  }

  if (now - mqttConnectTimestamp > 15000) {
    display.setMQTTStatus("Connecting...");
    const String status_topic = "status/"+cm.getString("identifier")+"/alive";
    if (client.connect(cm.getString("identifier").c_str(),status_topic.c_str(),1,true,"{\"connected\":false}")) {  //todo randomise
      Serial.println("ONLINE");
      display.setMQTTStatus("Connected");
      const char* connected_message = "{\"connected\":true}";
      client.publish(status_topic.c_str(),connected_message,true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 15 seconds");
      display.setMQTTStatus("Failed");
    }
    mqttConnectTimestamp = now;
  }
}

void ShoestringLib::loop() {
  
  cm.step_loop();
  if (!client.connected()) {
    reconnect();
    delay(29);
  } else {
    client.loop();

    StaticJsonDocument<3000> JSONdoc;
    bool result = this->callback(JSONdoc);

    if (result) {    
      int start_2 = millis();
      get_timestamp();

      JSONdoc["timestamp"] = String(timestamp_buffer);
      JSONdoc["id"] = cm.getString("identifier");
      
      // print to serial
      // serializeJson(JSONdoc, Serial);
      // Serial.println();
      // Serial.print("Size of payload: ");
      // Serial.println(sizeof(JSONdoc));

      //send over MQTT
      char JSONmessageBuffer[4000];
      serializeJson(JSONdoc, JSONmessageBuffer, sizeof(JSONmessageBuffer));
      String topic = cm.getString("mqtt_topic") + "/" + cm.getString("identifier");
      client.publish(topic.c_str(), JSONmessageBuffer);

      int total_2 = millis()-start_2;
      Serial.print("Send MQTT took: ");
      Serial.println(total_2);
    }
  }
  // yield();
  
}


void ShoestringLib::printLocalTime() {
  get_timestamp();
  Serial.print("Time > ");
  Serial.println(timestamp_buffer);
}

void ShoestringLib::get_timestamp() {
  struct tm timeinfo;   
  struct timeval tv; 
  char ms_buffer[10];

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  gettimeofday(&tv, NULL);
  int milliseconds = tv.tv_usec / 1000;

  strftime(timestamp_buffer, sizeof timestamp_buffer, "%Y-%m-%dT%H:%M:%S", &timeinfo);
  snprintf(ms_buffer, sizeof(ms_buffer), ".%d", milliseconds);
  strcat(timestamp_buffer, ms_buffer);
  strcat(timestamp_buffer, "+00:00");
}

// void ShoestringLib::get_timestamp_ms() {
//     struct timeval tv;
//     gettimeofday(&tv, NULL);
//     unsigned long long int milliseconds = (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
//     Serial.print("ms :");
//     Serial.println(milliseconds);
//     snprintf(timestamp_buffer, sizeof(timestamp_buffer), "%llu", milliseconds);
//     Serial.print("ts buffer :");
//     Serial.println(timestamp_buffer);
// }

