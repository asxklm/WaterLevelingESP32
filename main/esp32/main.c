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
#define WIFI_SSID "SSID_WIFI"        
#define WIFI_PASS "PASS_WIFI"    
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
            if (strncmp(event->topic, "TOPIK/MODEKALIBRASI", event->topic_len) == 0) {
                
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
            .address.uri = "ALAMATBROKER" 
        },
        .credentials = {
            .client_id = "ID_CLIENT"
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
