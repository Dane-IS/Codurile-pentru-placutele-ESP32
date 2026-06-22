#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>

const char* ssid = "Nume_retea";
const char* password = "Parola_retea";
const char* mqtt_server = "broker.emqx.io"; 

const char* topic_slave1_date = "esp32/slave1/date";
const char* topic_slave2_date = "esp32/slave2/date";
const char* topic_slave1_cmd = "esp32/slave1/comenzi";
const char* topic_slave2_cmd = "esp32/slave2/comenzi";
const char* topic_slave1_auto = "esp32/slave1/auto"; // Adăugat topic auto Slave 1
const char* topic_slave2_auto = "esp32/slave2/auto"; 
const char* topic_istoric = "esp32/istoric"; 

uint8_t adresaSlave1[] = {0x70, 0x4B, 0xCA, 0x27, 0x87, 0xB0};
uint8_t adresaSlave2[] = {0x1C, 0xC3, 0xAB, 0xF9, 0xEC, 0xD4};

WiFiClient espClient;
PubSubClient clientMQTT(espClient);
Preferences preferinte;

// --- VARIABILE PENTRU AMBELE ZONE ---
bool autoSlave1Active = false;
bool seUdaAcum1 = false; 

bool autoSlave2Active = false;
bool seUdaAcum2 = false; 

void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  String mesaj = "";
  for (int i = 0; i < length; i++) mesaj += (char)payload[i];
  
  //  mod AUTO pentru SLAVE 1
  if (String(topic) == topic_slave1_auto) {
    autoSlave1Active = (mesaj == "1"); 
    preferinte.putBool("auto1", autoSlave1Active); 
  }
  //  mod AUTO pentru SLAVE 2
  else if (String(topic) == topic_slave2_auto) {
    autoSlave2Active = (mesaj == "1"); 
    preferinte.putBool("auto2", autoSlave2Active); 
  }
  // Comenzi manuale Slave 1 & Slave 2
  else if (String(topic) == topic_slave1_cmd) {
    esp_now_send(adresaSlave1, (uint8_t *) mesaj.c_str(), mesaj.length());
  } 
  else if (String(topic) == topic_slave2_cmd) {
    esp_now_send(adresaSlave2, (uint8_t *) mesaj.c_str(), mesaj.length());
  }
}

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  char buffer[len + 1];
  memcpy(buffer, incomingData, len);
  buffer[len] = '\0';
  String mesajText = String(buffer);

  // --- LOGICA PENTRU  UDARE AUTOMATĂ SLAVE 2 ---
  if (memcmp(info->src_addr, adresaSlave2, 6) == 0) {
    clientMQTT.publish(topic_slave2_date, mesajText.c_str());

    int lPos = mesajText.indexOf("L:");
    int uPos = mesajText.indexOf("U:");
    if (lPos != -1 && uPos != -1) {
      int nivelApa = mesajText.substring(lPos + 2, mesajText.indexOf(",", lPos)).toInt();
      int umiditate = mesajText.substring(uPos + 2).toInt();

      if (autoSlave2Active && nivelApa >= 20) {
        if (umiditate <= 30 && !seUdaAcum2) { 
          String cmd = "PORNESTE_UDARE";
          esp_now_send(adresaSlave2, (uint8_t *) cmd.c_str(), cmd.length());
          seUdaAcum2 = true;
          clientMQTT.publish(topic_istoric, "START_2"); 
        } 
        else if (umiditate >= 80 && seUdaAcum2) { 
          String cmd = "OPRESTE_UDARE";
          esp_now_send(adresaSlave2, (uint8_t *) cmd.c_str(), cmd.length());
          seUdaAcum2 = false;
        }
      }
      if (nivelApa < 20) { 
        String cmd = "OPRESTE_UDARE";
        esp_now_send(adresaSlave2, (uint8_t *) cmd.c_str(), cmd.length());
        seUdaAcum2 = false;
      }
    }
  }

  // --- LOGICA PENTRU UDARE AUTOMATĂ SLAVE 1 ---
  else if (memcmp(info->src_addr, adresaSlave1, 6) == 0) {
    clientMQTT.publish(topic_slave1_date, mesajText.c_str());

    int lPos = mesajText.indexOf("L:");
    int uPos = mesajText.indexOf("U:");
    if (lPos != -1 && uPos != -1) {
      int nivelApa = mesajText.substring(lPos + 2, mesajText.indexOf(",", lPos)).toInt();
      int umiditate = mesajText.substring(uPos + 2).toInt();

      if (autoSlave1Active && nivelApa >= 20) {
        if (umiditate <= 30 && !seUdaAcum1) { 
          String cmd = "PORNESTE_UDARE";
          esp_now_send(adresaSlave1, (uint8_t *) cmd.c_str(), cmd.length());
          seUdaAcum1 = true;
          clientMQTT.publish(topic_istoric, "START_1"); // Trimitem istoric pentru 1
        } 
        else if (umiditate >= 80 && seUdaAcum1) { 
          String cmd = "OPRESTE_UDARE";
          esp_now_send(adresaSlave1, (uint8_t *) cmd.c_str(), cmd.length());
          seUdaAcum1 = false;
        }
      }
      if (nivelApa < 20) { 
        String cmd = "OPRESTE_UDARE";
        esp_now_send(adresaSlave1, (uint8_t *) cmd.c_str(), cmd.length());
        seUdaAcum1 = false;
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  preferinte.begin("sistemIrigatii", false); 
  
  autoSlave1Active = preferinte.getBool("auto1", false); 
  autoSlave2Active = preferinte.getBool("auto2", false); 
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  clientMQTT.setServer(mqtt_server, 1883);
  clientMQTT.setCallback(callbackMQTT);
  
  esp_now_init();
  esp_now_register_recv_cb((esp_now_recv_cb_t)onDataRecv);
  
  esp_now_peer_info_t peerInfo = {};
  peerInfo.channel = WiFi.channel();
  peerInfo.encrypt = false;
  
  // Adăugăm AMBELE plăci Slave
  memcpy(peerInfo.peer_addr, adresaSlave1, 6);
  esp_now_add_peer(&peerInfo);
  
  memcpy(peerInfo.peer_addr, adresaSlave2, 6);
  esp_now_add_peer(&peerInfo);
}

void loop() {
  if (!clientMQTT.connected()) {
    if (clientMQTT.connect("MasterAlex")) {
      clientMQTT.subscribe(topic_slave1_cmd);
      clientMQTT.subscribe(topic_slave2_cmd);
      clientMQTT.subscribe(topic_slave1_auto); // Abonare la comutatorul 1
      clientMQTT.subscribe(topic_slave2_auto); // Abonare la comutatorul 2
    }
  }
  clientMQTT.loop();
}
