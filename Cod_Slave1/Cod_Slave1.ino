#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 

const int PIN_POMPA = 26;      
#define sensorPower 4          
#define sensorPin_umiditate_sol 36  
#define sensorPin_water_level 39      

uint8_t adresaMaster[] = {0xB0, 0xCB, 0xD8, 0xE6, 0x1A, 0x48}; 

bool masterGasit = false;
int canalCurent = 1;
esp_now_peer_info_t infoMaster = {};

void onDateTrimise(const uint8_t *mac_addr, esp_now_send_status_t status) {
  masterGasit = (status == ESP_NOW_SEND_SUCCESS);
}

// --- COMANDA PENTRU POMPĂ ---
void onDatePrimite(const esp_now_recv_info_t *info, const uint8_t *date, int lungime) {
  char buffer[lungime + 1];
  memcpy(buffer, date, lungime);
  buffer[lungime] = '\0';
  String comanda = String(buffer);

  Serial.println("Comandă primită: " + comanda);

  if (comanda == "PORNESTE_UDARE") {
    pinMode(PIN_POMPA, OUTPUT);
    digitalWrite(PIN_POMPA, LOW);  
    Serial.println(">>> POMPA PORNIȚĂ");
  } 
  else if (comanda == "OPRESTE_UDARE") {
    pinMode(PIN_POMPA, INPUT);     
    Serial.println(">>> POMPA OPRITĂ");
  }
}

void cautaMaster() {
  while (!masterGasit) {
    canalCurent++;
    if (canalCurent > 13) canalCurent = 1;
    esp_wifi_set_channel(canalCurent, WIFI_SECOND_CHAN_NONE);
    esp_now_del_peer(adresaMaster);
    infoMaster.channel = canalCurent;
    esp_now_add_peer(&infoMaster);
    String ping = "ping2"; 
    esp_now_send(adresaMaster, (uint8_t *) ping.c_str(), ping.length());
    delay(150); 
    if (masterGasit) break; 
  }
}

void setup() {
  Serial.begin(9600);
  
  // --- PORNIREA SISTEMULUI  ---
  // Setăm pinul ca INPUT de la bun început ca releul să stea oprit
  pinMode(PIN_POMPA, INPUT); 

  pinMode(sensorPower, OUTPUT);
  digitalWrite(sensorPower, LOW);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb((esp_now_send_cb_t)onDateTrimise);
  esp_now_register_recv_cb((esp_now_recv_cb_t)onDatePrimite);

  memcpy(infoMaster.peer_addr, adresaMaster, 6);
  infoMaster.encrypt = false;
  cautaMaster(); 
}

void loop() {
  if (masterGasit) {
    digitalWrite(sensorPower, HIGH);
    delay(50); 
    int rawUmiditate = analogRead(sensorPin_umiditate_sol);
    int rawNivel = analogRead(sensorPin_water_level);
    digitalWrite(sensorPower, LOW);

    String stareSol = "";
    if(rawUmiditate >= 4000) {
      stareSol = "Senzorul nu este in sol";
    }
    else if(rawUmiditate < 4000 && rawUmiditate >= 2400) { 
      stareSol = "Solul este USCAT";
    }
    else if(rawUmiditate < 2400 && rawUmiditate >= 1480) {
      stareSol = "Solul este UMED"; 
    }
    else if(rawUmiditate < 1480) {
      stareSol = "Senzorul este in APA";
    }

    Serial.println(stareSol);
   
    // --- CALCUL PROCENTAJE PENTRU FLUTTER ---
    int procentNivel = map(rawNivel, 0, 2100, 0, 100); //de aici reglez sa arate 100%
    // Am aliniat funcția map() cu pragurile tale pentru o acuratețe mai bună
    int procentUmiditate = map(rawUmiditate, 4000, 1480, 0, 100);

    procentNivel = constrain(procentNivel, 0, 100);
    procentUmiditate = constrain(procentUmiditate, 0, 100);

    // --- TRIMITERE DATE CĂTRE MASTER ---
    String mesaj = "L:" + String(procentNivel) + ",U:" + String(procentUmiditate);
    esp_now_send(adresaMaster, (uint8_t *) mesaj.c_str(), mesaj.length());
    
    Serial.println("Trimis: " + mesaj);
    Serial.println("-------------------------");
    
    delay(3000); 
  } else {
    cautaMaster();
  }
}