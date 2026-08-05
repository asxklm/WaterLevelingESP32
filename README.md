# 🌊 Capacitive Water Level Monitoring & Auto-Calibration System (ESP32)

[![ESP32](https://img.shields.io/badge/Hardware-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/Framework-ESP--IDF%20%2F%20C-green.svg)](https://docs.espressif.com/projects/esp-idf/)
[![MQTT](https://img.shields.io/badge/Protocol-MQTT-orange.svg)](https://mqtt.org/)
[![Home Assistant](https://img.shields.io/badge/Integration-Home%20Assistant-blue.svg)](https://www.home-assistant.io/)

Repositori ini berisi kode sumber (*source code*) untuk sistem pemantauan ketinggian dan estimasi volume air berbasis sensor kapasitif menggunakan mikrokontroler **ESP32**. Sistem ini dirancang untuk mengompensasi karakteristik *non-linear* dari sensor kapasitif dengan memanfaatkan fitur **Auto-Calibration 20-point** berbasis pewaktuan pengisian pompa dan penyimpanan data acuan secara permanen pada **Non-Volatile Storage (NVS) Flash**.

---

## 📌 Ringkasan Sistem

Sensor kapasitif memiliki sensitivitas pembacaan sinyal mentah (*raw value*) yang bersifat *non-linear* terhadap perubahan ketinggian fluida. Untuk mengatasi hal tersebut, sistem ini bekerja dengan alur sebagai berikut:

1. **Akuisisi Sinyal:** Membaca data kapasitansi mentah melalui pin masukan sentuh (**Touch Pin / GPIO8**) pada ESP32.
2. **Auto-Calibration:** Saat mode kalibrasi dipicu secara nirkabel dari Home Assistant, ESP32 mengontrol sakelar pintar (*Smart Switch*) untuk menyalakan pompa pengisi air. Sistem merekam 20 titik sampel data secara otomatis selama rentang durasi pengisian (46 detik) dan membekukannya ke dalam tabel acuan di memori **NVS Flash**.
3. **Kalkulasi Data:** Pembacaan mentah waktu nyata (*real-time*) diolah menggunakan algoritma **interpolasi linier** berdasarkan tabel NVS untuk menghasilkan estimasi ketinggian (cm) dan volume air (liter) yang presisi.
4. **Telemetri & Kontrol:** Data dikemas dalam format JSON dan dikirimkan via broker MQTT ke platform Home Assistant untuk visualisasi *dashboard* dan eksekusi otomatisasi.

---

## ✨ Fitur Utama

* **Non-Linear Compensation:** Menggunakan *Look-Up Table* 20 titik sampel untuk akurasi tinggi pada seluruh rentang pengukuran.
* **Auto-Calibration Mechanism:** Proses kalibrasi otomatis berbasis pewaktuan pompa yang dipicu via MQTT tanpa perlu konfigurasi ulang *firmware*.
* **NVS Flash Persistence:** Data kalibrasi disimpan secara permanen di memori NVS ESP32 sehingga aman dari kehilangan data saat perangkat mati (*power loss*).
* **MQTT Telemetry & Control:** Pengiriman data telemetri berlatensi rendah serta penerimaan perintah kontrol sakelar/kalibrasi nirkabel.
* **Home Assistant Native Integration:** Kompatibel penuh dengan *dashboard* Home Assistant untuk pemantauan *real-time* dan logika otomatisasi pengisian air.

---

## 🛠️ Development Environment

* **Microcontroller:** ESP32 Board
* **Framework:** ESP-IDF (C Language)
* **IDE / Build System:** VS Code dengan Ekstensi ESP-IDF / CMake / Ninja
* **Protokol Komunikasi:** Wi-Fi (802.11 b/g/n), MQTT, JSON
* **Integrasi Pihak Ketiga:** Home Assistant, LocalTuya / Smart Switch MQTT

---

## 🚀 Panduan Instalasi

### 1. Kloning Repositori
```bash
git clone https://github.com/asxklm/WaterLevelingESP32.git
cd WaterLevelingESP32
```

### 2. Konfigurasi Kredensial & Broker

Sebelum melakukan *build* dan *flash*, atur konfigurasi jaringan Wi-Fi, MQTT Broker, serta topik transmisi pada kode program (misalnya pada file `main/main.c` atau `main/config.h`):

```c
// Konfigurasi Wi-Fi
#define WIFI_SSID           "SSID_WIFI_KAMU"
#define WIFI_PASSWORD       "PASSWORD_WIFI_KAMU"

// Konfigurasi Broker MQTT
#define MQTT_BROKER_URL     "mqtt://192.168.1.X" // Ubah sesuai IP Broker / Home Assistant kamu
#define MQTT_PORT           1883
#define MQTT_USERNAME       "USERNAME_MQTT"
#define MQTT_PASSWORD       "PASSWORD_MQTT"

// Konfigurasi Topik MQTT
#define TOPIC_TELEMETRY     "water_level/telemetry"   // Publish: Data JSON (ketinggian & volume)
#define TOPIC_CALIBRATION   "water_level/calibrate"   // Subscribe: Pemicu mode auto-calibration
```

### 3. Build, Flash, & Pengujian System

Setelah clone dan konfigurasi kredensial/broker selesai, ikuti langkah-langkah berikut untuk mengompilasi dan menguji perangkat:

#### A. Hubungkan Hardware
1. Hubungkan modul ESP32 ke komputer menggunakan kabel USB data (pastikan kabel mendukung data transfer).
2. Periksa nomor port serial perangkat (misalnya `COM3` atau `COM4` di Windows, atau `/dev/ttyUSB0` di Linux/Mac).

#### B. Build & Flash Firmware (ESP-IDF)
Buka terminal VS Code (dengan lingkungan ESP-IDF aktif) atau ESP-IDF Command Prompt, lalu jalankan perintah berikut:

```bash
# Set target mikrokontroler ke ESP32
idf.py set-target esp32

# Build, flash ke ESP32, dan buka serial monitor sekaligus
# (Sesuaikan COM3 dengan port serial ESP32 kamu)
idf.py -p COM3 flash monitor
```
### 4. Verifikasi Output Serial

Setelah proses *flashing* selesai, pantau *log* terminal serial pada bitrate **115200 baud** (atau via `idf.py monitor`). Pastikan urutan inisialisasi dan indikator status berhasil tampil seperti berikut:

```text
I (512) wifi: mode : sta (xx:xx:xx:xx:xx:xx)
I (1024) wifi: connected to ssid WIFI_KAMU, status 0
I (1536) esp_netif_handlers: STA ip: 192.168.1.50, mask: 255.255.255.0, gw: 192.168.1.1
I (2048) WATER_LEVEL_NVS: Table calibration data successfully read from NVS Flash (20 points loaded).
I (2560) MQTT_CLIENT: MQTT_EVENT_CONNECTED - Broker connection established.
I (3072) MQTT_CLIENT: Subscribed to topic: water_level/calibrate

I (4000) WATER_LEVEL_APP: Raw Touch: 8450 | Calibrated Height: 125.4 cm | Volume: 15.7 L
I (5000) MQTT_CLIENT: Telemetry JSON published to water_level/telemetry
```

## 🏡 Konfigurasi Home Assistant

Untuk mengintegrasikan perangkat ESP32 dengan Home Assistant, tambahkan entitas sensor dan tombol pemicu kalibrasi ke dalam file `configuration.yaml` pada Home Assistant kamu:

### 1. Entitas Sensor MQTT (`configuration.yaml`)

```yaml
mqtt:
  sensor:
    - name: "Ketinggian Air"
      state_topic: "water_level/telemetry"
      unit_of_measurement: "cm"
      value_template: "{{ value_json.height }}"
      icon: "mdi:water-percent"

    - name: "Volume Air"
      state_topic: "water_level/telemetry"
      unit_of_measurement: "L"
      value_template: "{{ value_json.volume }}"
      icon: "mdi:gauge"

  button:
    - name: "Pemicu Auto Calibration"
      command_topic: "water_level/calibrate"
      payload_press: "START"
      icon: "mdi:autorenew"
```

### 2. Otomasi Sakelar Pintar (Automation)

Otomasi ini mengatur pengisian air secara otomatis via sakelar pintar (*Smart Switch*). Pompa akan menyala (*ON*) secara otomatis ketika ketinggian air berada di bawah batas minimum (air habis) dan mati (*OFF*) ketika menyentuh batas maksimum (air penuh).

Tambahkan skrip berikut ke dalam file `automations.yaml`:

```yaml
# 1. Otomasi Nyalakan Pompa Saat Air Habis (< 20 cm)
- alias: "Pompa Air - Nyalakan Otomatis (Air Habis)"
  trigger:
    - platform: numeric_state
      entity_id: sensor.ketinggian_air
      below: 20
  action:
    - service: switch.turn_on
      target:
        entity_id: switch.smart_switch_pompa
  mode: single

# 2. Otomasi Matikan Pompa Saat Air Penuh (>= 150 cm)
- alias: "Pompa Air - Matikan Otomatis (Air Penuh)"
  trigger:
    - platform: numeric_state
      entity_id: sensor.ketinggian_air
      above: 150
  action:
    - service: switch.turn_off
      target:
        entity_id: switch.smart_switch_pompa
  mode: single
```

### 5. Pengujian Integrasi Home Assistant

Prosedur pengujian dilakukan untuk memastikan bahwa seluruh komunikasi data dua arah (telemetri & kontrol) antara ESP32, MQTT Broker, dan platform Home Assistant berjalan dengan presisi.

#### A. Verifikasi Entitas & Telemetri Real-Time
1. Buka *dashboard* Home Assistant, lalu masuk ke menu **Settings > Devices & Services > Entities**.
2. Cari entitas `sensor.ketinggian_air` dan `sensor.volume_air`.
3. Pastikan status entitas memperbarui nilainya secara *real-time* sesuai dengan interval data JSON yang dikirimkan oleh ESP32.

#### B. Pengujian Mode Auto-Calibration (20-Point)
1. Kosongkan pipa/kolom ukur transparan.
2. Tekan tombol **Pemicu Auto Calibration** (`button.pemicu_auto_calibration`) pada *dashboard* Home Assistant.
3. **Skenario Sistem:**
   * Home Assistant akan memublikasikan pesan pemicu (`START`) ke topik `water_level/calibrate`.
   * ESP32 menerima perintah, lalu menyalakan sakelar pintar (*Smart Switch*) untuk mengaktifkan pompa air.
   * Selama durasi pengisian **46 detik**, ESP32 mengambil 20 sampel data pembacaan sensor kapasitif secara periodik.
   * Data acuan baru dibekukan (*frozen*) dan disimpan secara permanen ke memori **NVS Flash** ESP32.
   * Pompa mati secara otomatis dan sistem kembali ke mode pemantauan normal menggunakan tabel kalibrasi baru.

#### C. Pengujian Otomasi Sakelar Pintar (Air Habis & Air Penuh)
* **Pengujian Air Habis:** Kurangi volume air hingga berada di bawah batas minimum (< 20 cm). Pastikan otomasi Home Assistant berhasil memicu *Smart Switch* ke status **ON** untuk menyalakan pompa pengisi.
* **Pengujian Air Penuh:** Biarkan pengisian air berjalan hingga menyentuh batas maksimum (>= 150 cm). Pastikan otomasi Home Assistant berhasil memicu *Smart Switch* ke status **OFF** untuk menghentikan pompa secara otomatis.
