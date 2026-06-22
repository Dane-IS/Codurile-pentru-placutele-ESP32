#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> 

// --- 1. CONFIGURARE PINI ---
const int PIN_POMPA = 26;      
#define sensorPower 4          
#define sensorPin_umiditate_sol 36  
#define sensorPin_water_level 39      

// Adresa MAC a Masterului
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

  Serial.println("Comandă primită pentru Slave 1: " + comanda);

  if (comanda == "PORNESTE_UDARE") {
    pinMode(PIN_POMPA, OUTPUT);
    digitalWrite(PIN_POMPA, LOW);  
    Serial.println(">>> POMPA 1 PORNIȚĂ");
  } 
  else if (comanda == "OPRESTE_UDARE") {
    pinMode(PIN_POMPA, INPUT);     
    Serial.println(">>> POMPA 1 OPRITĂ");
  }
}

// --- FUNCȚIA DE CĂUTARE MASTER ---
void cautaMaster() {
  while (!masterGasit) {
    canalCurent++;
    if (canalCurent > 13) canalCurent = 1;
    esp_wifi_set_channel(canalCurent, WIFI_SECOND_CHAN_NONE);
    esp_now_del_peer(adresaMaster);
    infoMaster.channel = canalCurent;
    esp_now_add_peer(&infoMaster);
    
    String ping = "ping1"; 
    esp_now_send(adresaMaster, (uint8_t *) ping.c_str(), ping.length());
    
    delay(150); 
    if (masterGasit) break; 
  }
}

void setup() {
  Serial.begin(9600);
  
  // Pompa stă oprită 
  pinMode(PIN_POMPA, INPUT); 

  pinMode(sensorPower, OUTPUT);
  digitalWrite(sensorPower, LOW);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb((esp_now_send_cb_t)onDateTrimise);
  esp_now_register_recv_cb((esp_now_recv_cb_t)onDatePrimite);

  memcpy(infoMaster.peer_addr, adresaMaster, 6);
  infoMaster.encrypt = false;
  
  Serial.println("Slave 1 pornit. Căutare Master...");
  cautaMaster(); 
}

void loop() {
  if (masterGasit) {
    // 1. Citire Senzori
    digitalWrite(sensorPower, HIGH);
    delay(50); 
    int rawUmiditate = analogRead(sensorPin_umiditate_sol);
    int rawNivel = analogRead(sensorPin_water_level);
    digitalWrite(sensorPower, LOW);

    // 2. Diagnostic în Serial Monitor
    String stareSol = "";
    if(rawUmiditate >= 4000) stareSol = "Senzorul 1 nu este in sol";
    else if(rawUmiditate >= 2400) stareSol = "Sol 1: USCAT";
    else if(rawUmiditate >= 1480) stareSol = "Sol 1: UMED";
    else stareSol = "Senzor 1 in APA";

    Serial.println(stareSol);
    
    // 3. Calcul Procentaje
    // Nivel apă (0 uscat -> 2100 plin)
    int procentNivel = map(rawNivel, 0, 2100, 0, 100); 
    // Umiditate sol (4000 uscat -> 1480 ud)
    int procentUmiditate = map(rawUmiditate, 4000, 1480, 0, 100);

    procentNivel = constrain(procentNivel, 0, 100);
    procentUmiditate = constrain(procentUmiditate, 0, 100);

    // 4. Trimitere date către Master
    String mesaj = "L:" + String(procentNivel) + ",U:" + String(procentUmiditate);
    esp_now_send(adresaMaster, (uint8_t *) mesaj.c_str(), mesaj.length());
    
    Serial.println("Slave 1 Trimis: " + mesaj);
    Serial.println("-------------------------");
    
    delay(3000); 
  } else {
    cautaMaster();
  }
}