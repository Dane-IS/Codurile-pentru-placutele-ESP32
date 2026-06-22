# 🌿 Sistem de Irigații Smart — ESP32 Firmware

Acest repository conține codul sursă (C++/Arduino) pentru rețeaua de microcontrollere **ESP32** care gestionează automatizarea și monitorizarea unui sistem inteligent de irigații. 

Sistemul este gândit modular, folosind o arhitectură de tip **Master - Slave** via protocolul local **ESP-NOW** și comunicare cu cloud-ul prin **MQTT**.

> 📱 **Notă:** Codul sursă pentru aplicația mobilă (Flutter) care controlează acest sistem se află în repository-ul principal: **https://github.com/Dane-IS/Aplicatie-sistem-de-irigatii-smart**

---

## 🏗️ Arhitectura Sistemului

Sistemul este format din 3 plăci de dezvoltare ESP32:

```text
[ Senzori Sol & Nivel ] ──> ( Slave 1: Grădină ) ──┐
                                                   ├─(ESP-NOW)─> [ MASTER ESP32 ] ──(Wi-Fi / MQTT)──> [ Broker EMQX ] <──> 📱 Aplicație Flutter
[ Senzori Sol & Nivel ] ──> ( Slave 2: Balcon  ) ──┘
