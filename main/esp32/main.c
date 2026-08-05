// #include <stdio.h>
// #include <string.h> // Diperlukan untuk strerror jika ada error MQTT
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
// #include "freertos/event_groups.h" // Diperlukan untuk WiFi event handler
// #include "tp_pad.h" // Modul custom untuk touch pad
// #include "esp_log.h"
// #include "esp_wifi.h"       // Diperlukan untuk inisialisasi WiFi
// #include "esp_event.h"      // Diperlukan untuk event handler WiFi
// #include "nvs_flash.h"      // Diperlukan untuk penyimpanan non-volatile
// #include "mqtt_client.h"    // Diperlukan untuk MQTT client
// #include "esp_tls.h"        // Diperlukan jika menggunakan TLS/SSL untuk MQTT (meskipun tidak digunakan di sini)
// #include "lwip/dns.h"
// #include "lwip/sockets.h"
// #include "lwip/inet.h"

// // --- Kredensial WiFi Anda ---
// // Ganti dengan SSID dan Password WiFi Anda
// #define WIFI_SSID "Wabe"        // Ganti dengan SSID WiFi Anda
// #define WIFI_PASS "asdfhjkl"    // Ganti dengan Password WiFi Anda
// // ----------------------------

// // TAG untuk logging di main.c
// static const char *TAG = "MAIN_APP";

// // Handle global untuk client MQTT
// static esp_mqtt_client_handle_t client = NULL;

// // Flag global untuk melacak status koneksi MQTT
// static bool mqtt_connected = false;

// static tCalibration_Table ch8Cal = {
//     {110080,101888,93696,84480,78848,73728,67584,64256,61696,57856,54528,51200,49664,47360,45312,43776,41984,40448,38912,37376,36608,35584,34560,33280,32256,31744,30208,29440,28672,27904,26880,26368,25600,25088,24576,24064,23296,22784,22528,22016},
//     {1280,2560,3840,5120,6400,7680,8960,10240,11520,12800,14080,15360,16640,17920,19200,20480,21760,23040,24320,25600,26880,28160,29440,30720,32000,33280,34560,35840,37120,38400,39680,40960,42240,43520,44800,46080,47360,48640,49920,51200},
//     8,
//     100,
//     40
// };

// // Definisi sensor sentuh yang digunakan pada board ESP32
// static Touch_Sensor_t g_tp_sensor[] = {
//     CREATE_TOUCH_SENSOR(CH8,8),
// }; 

// // Objek yang mengelompokkan semua sensor yang akan digunakan oleh modul tp_pad
// static tTouch_Sensor_Obj tp_obj = 
// {
//     .sensor = g_tp_sensor,
//     .length = (sizeof g_tp_sensor / sizeof g_tp_sensor[0])
// };

// // ---

// // ## MQTT Event Handler (Wrapper)

// // ```c
// // Callback function yang menangani berbagai event dari client MQTT
// // Ini adalah WRAPPER FUNCTION yang sesuai dengan esp_event_handler_t
// static void mqtt_event_handler(void* arg, esp_event_base_t event_base,
//                                 int32_t event_id, void* event_data)
// {
//     esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
//     client = event->client; // Simpan handle client MQTT dari event

//     switch (event->event_id) {
//         case MQTT_EVENT_CONNECTED:
//             ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
//             mqtt_connected = true; // Set flag ke true saat terhubung
//             break;
//         case MQTT_EVENT_DISCONNECTED:
//             ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
//             mqtt_connected = false; // Set flag ke false saat terputus
//             break;
//         case MQTT_EVENT_PUBLISHED:
//             ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
//             break;
//         case MQTT_EVENT_DATA:
//             ESP_LOGI(TAG, "MQTT_EVENT_DATA. TOPIC=%.*s DATA=%.*s", event->topic_len, event->topic, event->data_len, event->data);
//             break;
//         case MQTT_EVENT_ERROR:
//             ESP_LOGE(TAG, "MQTT_EVENT_ERROR type=%d", event->error_handle->error_type);
//             // Ketika menggunakan TCP non-TLS, error yang relevan adalah esp_transport_sock_errno
//             if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
//                 // Log detail error transportasi TCP jika terjadi
//                 ESP_LOGE(TAG, "Last errno value: %d (%s)", event->error_handle->esp_transport_sock_errno, strerror(event->error_handle->esp_transport_sock_errno));
//             }
//             break;
//         default:
//             ESP_LOGI(TAG, "Other MQTT event id: %d", event->event_id);
//             break;
//     }
// }

// // Handler event WiFi untuk memantau status koneksi
// static void wifi_event_handler(void* arg, esp_event_base_t event_base,
//                                 int32_t event_id, void* event_data)
// {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         esp_wifi_connect();
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
//         ESP_LOGI(TAG, "GOT IP:" IPSTR, IP2STR(&event->ip_info.ip));
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         ESP_LOGI(TAG, "WiFi disconnected, retrying...");
//         esp_wifi_connect();
//     }
// }

// // Fungsi untuk mengatur DNS secara manual ke 8.8.8.8
// void set_manual_dns()
// {
//     ip_addr_t dnsserver;
//     inet_pton(AF_INET, "8.8.8.8", &dnsserver.u_addr.ip4);
//     dnsserver.type = IPADDR_TYPE_V4;
//     dns_setserver(0, &dnsserver);
// }

// // Fungsi inisialisasi WiFi dalam mode STA
// void wifi_init_sta(void)
// {
//     esp_netif_init();

//     set_manual_dns();  // Atur DNS ke 8.8.8.8 sebelum membuat interface

//     esp_event_loop_create_default();
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     esp_wifi_init(&cfg);

//     // Registrasi event handler
//     esp_event_handler_instance_register(WIFI_EVENT,
//                                         ESP_EVENT_ANY_ID,
//                                         &wifi_event_handler,
//                                         NULL,
//                                         NULL);
//     esp_event_handler_instance_register(IP_EVENT,
//                                         IP_EVENT_STA_GOT_IP,
//                                         &wifi_event_handler,
//                                         NULL,
//                                         NULL);

//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = WIFI_SSID,
//             .password = WIFI_PASS,
//             .threshold.authmode = WIFI_AUTH_WPA2_PSK,
//         },
//     };
//     esp_wifi_set_mode(WIFI_MODE_STA);
//     esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
//     esp_wifi_start();

//     ESP_LOGI(TAG, "WiFi init finished. Trying to connect to SSID: %s", WIFI_SSID);
// }

// // Fungsi untuk menginisialisasi dan memulai MQTT client
// static void mqtt_app_start(void)
// {
//     // Konfigurasi MQTT client
//     esp_mqtt_client_config_t mqtt_cfg = {
//         .broker = {
//             // Menggunakan 'mqtt://' untuk koneksi TCP standar (non-TLS/SSL)
//             .address.uri = "mqtt://broker.emqx.io:1883" // <--- PASTIKAN INI ADALAH BROKER TCP ANDA
//         },
//         .credentials = {
//             .client_id = "TPdata"
//         }
//         // Untuk TCP, tidak perlu konfigurasi 'cert_pem', 'client_cert_pem', 'client_key_pem'
//         // atau 'skip_cert_validation'
//     };

//     client = esp_mqtt_client_init(&mqtt_cfg);
    
//     // Pendaftaran event handler MQTT dengan wrapper yang sesuai
//     esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);

//     esp_mqtt_client_start(client);
//     ESP_LOGI(TAG, "MQTT Client started.");
// }

// // Task ini bertanggung jawab untuk mengambil data dari antrean sensor
// // dan mempublikasikannya ke broker MQTT.
// static void mqtt_publish_task(void *pvParameter)
// {
//     QueueHandle_t sensor_data_queue = (QueueHandle_t)pvParameter;
//     tTouchPad_msg *msg_ptr;

//     while (1) {
//         if (xQueueReceive(sensor_data_queue, &msg_ptr, portMAX_DELAY) == pdPASS) {
//             ESP_LOGI(TAG, "Received from sensor task: %s", msg_ptr->message);

//             // Periksa apakah client MQTT sudah terinisialisasi DAN terhubung
//             if (client && mqtt_connected) { 
//                 int msg_id = esp_mqtt_client_publish(
//                     client,
//                     "obd_fate",  // <--- TOPIK MQTT UNTUK DATA AIR ANDA
//                     msg_ptr->message,
//                     0,
//                     2,
//                     0
//                 );
//                 ESP_LOGI(TAG, "MQTT published message ID: %d", msg_id);
//             } else {
//                 ESP_LOGW(TAG, "MQTT client not connected, cannot publish data.");
//             }
//         }
//     }
// }

// // ---

// // ## `app_main` (Fungsi Utama Aplikasi)

// // ```c
// // Fungsi utama aplikasi yang akan dijalankan oleh ESP-IDF saat boot
// void app_main(void)
// {
//     // 1. Inisialisasi NVS (Non-Volatile Storage)
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);

//     // 2. Inisialisasi dan koneksi WiFi
//     wifi_init_sta();

//     // 3. Inisialisasi MQTT client
//     mqtt_app_start();

//     // 4. Inisialisasi hardware touch pad dan konfigurasi sensor
//     EnableScale(&g_tp_sensor[0], &ch8Cal, 500);
//     // EnableScale(&g_tp_sensor[1], &ch3Cal, 500);
    
//     InitTPSensors(tp_obj.sensor, tp_obj.length, 10); 

//     // 5. Inisialisasi task pembacaan touch pad (`vTouchReadTask` dari `tp_pad.c`)
//     QueueHandle_t sensor_data_queue = InitTouchPadTask(&tp_obj);

//     // 6. Buat task baru (`mqtt_publish_task`) untuk membaca data dari antrean sensor
//     // dan mempublikasikannya ke broker MQTT.
//     xTaskCreate(mqtt_publish_task, "mqtt_pub_task", 4096, (void*)sensor_data_queue, 5, NULL);
// }












#include <stdio.h>
#include <string.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h" 
#include "tp_pad.h" 
#include "esp_log.h"
#include "esp_wifi.h"       
#include "esp_event.h"      
#include "nvs_flash.h"      
#include "mqtt_client.h"    
#include "esp_tls.h"        
#include "lwip/dns.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

// --- Kredensial WiFi Anda ---
#define WIFI_SSID "Wabe"        
#define WIFI_PASS "asdfhjkl"    
// ----------------------------

static const char *TAG = "MAIN_APP";
static esp_mqtt_client_handle_t client = NULL;
static bool mqtt_connected = false;

// 1. DEFINISI VAR TABEL KALIBRASI (Nanti diisi dinamis dari NVS Flash)
tCalibration_Table ch8Cal = {
    .qBaseNum = 8,
    .xNum = 100,
    .paramCount = 0
};

// Definisi sensor sentuh yang digunakan pada board ESP32
static Touch_Sensor_t g_tp_sensor[] = {
    CREATE_TOUCH_SENSOR(CH8, 8),
}; 

// Objek yang mengelompokkan semua sensor
static tTouch_Sensor_Obj tp_obj = 
{
    .sensor = g_tp_sensor,
    .length = (sizeof g_tp_sensor / sizeof g_tp_sensor[0])
};

// Callback function yang menangani berbagai event dari client MQTT
static void mqtt_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    client = event->client;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_connected = true;
            
            // 2. SUBSCRIBE KE TOPIK MODE KALIBRASI DARI HOME ASSISTANT
            esp_mqtt_client_subscribe(client, "ta/sensor/calibration_mode", 0);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_connected = false; 
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA. TOPIC=%.*s", event->topic_len, event->topic);

            // 1. Abaikan pesan jika pesan tersebut adalah Retained Message (pesan lama dari broker)
            if (event->retain) {
                ESP_LOGW(TAG, "Pesan diabaikan karena merupakan Retained Message!");
                break;
            }

            // 2. Cek apakah topik sesuai
            if (strncmp(event->topic, "ta/sensor/calibration_mode", event->topic_len) == 0) {
                
                // PASTI BEDA: Cek panjang data harus persis 5 karakter (panjang kata "START")
                if (event->data_len == 5 && strncmp(event->data, "START", 5) == 0) {
                    ESP_LOGI(TAG, "Sinyal START Valid Diterima! Memulai Kalibrasi...");
                    Start_Auto_Calibration();
                } else {
                    ESP_LOGW(TAG, "Payload diterima tapi bukan 'START' (Panjang data: %d). Diabaikan.", event->data_len);
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR type=%d", event->error_handle->error_type);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Last errno value: %d (%s)", 
                         event->error_handle->esp_transport_sock_errno, 
                         strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;

        default:
            break;
    }
}

// Handler event WiFi untuk memantau status koneksi
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "GOT IP:" IPSTR, IP2STR(&event->ip_info.ip));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    }
}

// Fungsi untuk mengatur DNS secara manual ke 8.8.8.8
void set_manual_dns()
{
    ip_addr_t dnsserver;
    inet_pton(AF_INET, "8.8.8.8", &dnsserver.u_addr.ip4);
    dnsserver.type = IPADDR_TYPE_V4;
    dns_setserver(0, &dnsserver);
}

// Fungsi inisialisasi WiFi dalam mode STA
void wifi_init_sta(void)
{
    esp_netif_init();

    set_manual_dns(); 

    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi init finished. Trying to connect to SSID: %s", WIFI_SSID);
}

// Fungsi untuk menginisialisasi dan memulai MQTT client
static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = "mqtt://broker.emqx.io:1883" 
        },
        .credentials = {
            .client_id = "TPdata"
        }
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "MQTT Client started.");
}

// Task pengirim data MQTT ke broker
static void mqtt_publish_task(void *pvParameter)
{
    QueueHandle_t sensor_data_queue = (QueueHandle_t)pvParameter;
    tTouchPad_msg *msg_ptr;

    while (1) {
        if (xQueueReceive(sensor_data_queue, &msg_ptr, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "Received from sensor task: %s", msg_ptr->message);

            if (client && mqtt_connected) { 
                int msg_id = esp_mqtt_client_publish(
                    client,
                    "obd_fate",  
                    msg_ptr->message,
                    0,
                    1, // QoS 1 agar lebih terjamin
                    0
                );
                ESP_LOGI(TAG, "MQTT published message ID: %d", msg_id);
            } else {
                ESP_LOGW(TAG, "MQTT client not connected, cannot publish data.");
            }
        }
    }
}

// Fungsi utama aplikasi saat booting
void app_main(void)
{
    // 1. Inisialisasi NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. MUAT DATA KALIBRASI TERSIMPAN DARI NVS STORAGE
    Load_Calibration_From_NVS();

    // 3. Inisialisasi dan koneksi WiFi
    wifi_init_sta();

    // 4. Inisialisasi MQTT client
    mqtt_app_start();

    // 5. Inisialisasi hardware touch pad dan skala
    EnableScale(&g_tp_sensor[0], &ch8Cal, 500);
    InitTPSensors(tp_obj.sensor, tp_obj.length, 10); 

    // 6. Inisialisasi task pembacaan touch pad (vTouchReadTask)
    QueueHandle_t sensor_data_queue = InitTouchPadTask(&tp_obj);

    // 7. Buat task mqtt_publish_task untuk mengirim data queue
    xTaskCreate(mqtt_publish_task, "mqtt_pub_task", 4096, (void*)sensor_data_queue, 5, NULL);
}