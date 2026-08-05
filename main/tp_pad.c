// #include <stdio.h>
// #include <string.h>
// #include "tp_pad.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
// #include "esp_log.h"
// #include <math.h> // Diperlukan untuk M_PI (nilai Pi)

// static const char *TAG = "TOUCH_PAD";
// static QueueHandle_t xTPQueueHandle;
// static tTouchPad_msg tp_msg[5];

// // --- Definisikan dimensi tangki Anda di sini ---
// // SESUAIKAN NILAI INI DENGAN JARI-JARI TANGKI FISIK ANDA DALAM SENTIMETER!
// // Contoh: Jari-jari tangki 20 cm
// #define TANK_RADIUS_CM 20.0f
// // ---------------------------------------------

// // --- Variabel global untuk melacak data volume dan waktu sebelumnya untuk perhitungan debit ---
// static float last_measured_volume_liter[MAX_TOUCH_SENSOR]; // Menyimpan volume terakhir untuk setiap sensor
// static TickType_t last_measured_tick[MAX_TOUCH_SENSOR];      // Menyimpan waktu terakhir pengukuran (dalam FreeRTOS Ticks)
// static bool first_measurement_done[MAX_TOUCH_SENSOR];        // Flag untuk menandai apakah pengukuran pertama telah dilakukan
// // -----------------------------------------------------------------------------------------

// esp_err_t InitTPSensors(Touch_Sensor_t * sensors, int len, uint32_t filterPeriod)
// {
//     touch_pad_init();
//     // Atur tegangan internal touch pad untuk sensitivitas
//     touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    
//     // Konfigurasi setiap touch pad sesuai nomor pin dan threshold
//     for(int i = 0; i < len; i++)
//     {
//         touch_pad_config(sensors[i].num, sensors[i].threshold);
//     }
//     // Mulai filter hardware untuk pembacaan yang lebih stabil
//     touch_pad_filter_start(filterPeriod);
//     return ESP_OK;
// }

// esp_err_t ReadTPSensors(Touch_Sensor_t * sensor, int len)
// {
//     // Baca nilai mentah dan terfilter untuk setiap sensor yang aktif
//     for(int i = 0; i < len; i++)
//     {
//         if(sensor[i].enable)
//         {
//             touch_pad_read_raw_data(sensor[i].num, &sensor[i].raw_value);
//             touch_pad_read_filtered(sensor[i].num, &sensor[i].filtered_value);
//         }
//     }
//     return ESP_OK;
// }

// QueueHandle_t InitTouchPadTask(tTouch_Sensor_Obj *touch_obj)
// {
//     // Buat antrean FreeRTOS untuk komunikasi data sensor
//     xTPQueueHandle = xQueueCreate(5, sizeof(tTouchPad_msg *));
//     // Buat dan mulai task FreeRTOS untuk membaca dan memproses sensor
//     xTaskCreate(vTouchReadTask, "TouchReadTask", 4096, (void*)touch_obj, 6, NULL); // Tambahkan stack size ke 4096 untuk JSON
//     return xTPQueueHandle; // Kembalikan handle antrean
// }

// int16_t ScaleSensor(Touch_Sensor_t *sensor)
// {
//     int32_t tmpValue;
//     int32_t rawValue;
    
//     // Jika tidak ada tabel kalibrasi, kembalikan nilai mentah
//     if(sensor->calTable == NULL)
//         return sensor->raw_value;

//     tCalibration_Table *tbl = sensor->calTable;
//     int index;

//     // Konversi nilai mentah sensor ke format Q-number untuk perhitungan fixed-point
//     rawValue = (int32_t)sensor->raw_value << tbl->qBaseNum;

//     // Jika nilai mentah melebihi atau sama dengan threshold, anggap level air 0
//     if(sensor->raw_value >= sensor->threshold) return 0;
    
//     // Tangani kasus di mana rawValue lebih tinggi dari titik kalibrasi pertama
//     if(rawValue >= tbl->sensReading[0])
//     {
//         tmpValue = 1 * tbl->xNum;
//         tmpValue >>= tbl->qBaseNum;
//         return tmpValue;
//     }

//     // Cari interval di tabel kalibrasi tempat rawValue berada
//     for(index = 0; index < tbl->paramCount - 1; index++)
//     {
//         if((rawValue <= tbl->sensReading[index]) && (rawValue >= tbl->sensReading[index+1]))
//         {
//             break; // Interval ditemukan
//         }
//     }
//     // Jika loop selesai tanpa menemukan interval, berarti rawValue lebih kecil dari titik kalibrasi terakhir
//     if (index == tbl->paramCount - 1) {
//         tmpValue = tbl->qActualRead[index] * tbl->xNum;
//         tmpValue >>= tbl->qBaseNum;
//         return tmpValue;
//     }

//     int32_t xa, xb, x;
//     int16_t ya, yb;

//     // Ambil nilai x dan y dari tabel kalibrasi untuk interpolasi
//     xa = tbl->sensReading[index];
//     xb = tbl->sensReading[index+1];
//     x  = rawValue;

//     ya = tbl->qActualRead[index];
//     yb = tbl->qActualRead[index+1];
    
//     // Lakukan interpolasi linier: y = ya + (x - xa) * (yb - ya) / (xb - xa)
//     tmpValue = ((yb - ya) * (x - xa)) / (xb - xa);
//     tmpValue += ya; 
    
//     // Kalikan dengan faktor pembesar dan konversi kembali
//     tmpValue *= tbl->xNum; 
//     tmpValue >>= tbl->qBaseNum; 

//     return (int16_t)tmpValue;
// }

// void EnableScale(Touch_Sensor_t *sensor, tCalibration_Table *tbl, uint16_t threshold)
// {
//     // Mengatur tabel kalibrasi dan threshold untuk sensor, serta mengaktifkan penskalaan
//     sensor->calTable = tbl;
//     sensor->threshold = threshold;
//     sensor->useScale = true;
// }

// /**
//  * @brief Task utama yang berjalan di latar belakang untuk membaca sensor,
//  * menghitung nilai, dan mengirimkannya sebagai string JSON ke queue.
//  */
// void vTouchReadTask(void *pvParameter)
// {
//     tTouch_Sensor_Obj *sensor_obj = (tTouch_Sensor_Obj *)pvParameter;
//     int idx = 0;
//     Touch_Sensor_t *sensor = sensor_obj->sensor;
//     int len = sensor_obj->length;
//     tTouchPad_msg *q_msg;
    
//     // Inisialisasi flag dan data awal untuk perhitungan debit
//     for(int i = 0; i < len; i++) {
//         first_measurement_done[i] = false;
//         last_measured_volume_liter[i] = 0.0f; 
//         last_measured_tick[i] = 0; 
//     }

//     while(1)
//     {
//         // Bersihkan buffer dan mulai string JSON dengan kurung siku buka
//         memset(tp_msg[idx].message, 0, sizeof(tp_msg[idx].message));
//         // strcpy(tp_msg[idx].message, "[");

//         int sensor_num = 0;
        
//         // Loop melalui setiap sensor yang terdaftar
//         for(int i = 0; i < len; i++)
//         {
//             if(sensor[i].enable)
//             {
//                 // Baca nilai mentah dan terfilter dari sensor
//                 touch_pad_read_raw_data(sensor[i].num, &sensor[i].raw_value);
//                 touch_pad_read_filtered(sensor[i].num, &sensor[i].filtered_value);
                
//                 // Jika penskalaan diaktifkan dan tabel kalibrasi tersedia
//                 if(sensor[i].useScale && sensor[i].calTable != NULL)
//                 {
//                     int16_t val = ScaleSensor(&sensor[i]);
//                     float height_cm = (float)val / sensor[i].calTable->xNum;
//                     float volume_l = (M_PI * TANK_RADIUS_CM * TANK_RADIUS_CM * height_cm) / 1000.0f;
//                     float consumption_rate = 0.0f;
//                     TickType_t current_tick = xTaskGetTickCount();

//                     if (first_measurement_done[i]) {
//                         TickType_t elapsed_ticks = current_tick - last_measured_tick[i];
//                         float elapsed_ms = (float)elapsed_ticks * portTICK_PERIOD_MS;

//                         if (elapsed_ms > 0.0f) {
//                             float volume_change = last_measured_volume_liter[i] - volume_l;
//                             if (volume_change > 0.01f) { 
//                                 consumption_rate = (volume_change / elapsed_ms) * 60000.0f;
//                             } else {
//                                 consumption_rate = 0.0f;
//                             }
//                         }
//                     } else {
//                         first_measurement_done[i] = true;
//                     }
                    
//                     last_measured_volume_liter[i] = volume_l;
//                     last_measured_tick[i] = current_tick;
                    
//                     // --- REVISI: MEMBUAT OBJEK JSON UNTUK SETIAP SENSOR ---
//                     char json_object[128]; // Buffer sementara untuk satu objek JSON

//                     // Tambahkan koma jika ini bukan objek JSON pertama
//                     if (sensor_num > 0) {
//                         strcat(tp_msg[idx].message, ",");
//                     }

//                     // Buat string objek JSON untuk sensor ini
//                     sprintf(json_object, 
//                             "{\"name\":\"%s\",\"height_cm\":%.2f,\"volume_l\":%.2f,\"flow_lpm\":%.2f}",
//                             sensor[i].name,
//                             height_cm,
//                             volume_l,
//                             consumption_rate
//                            );
                    
//                     // Gabungkan objek JSON ke pesan utama
//                     strcat(tp_msg[idx].message, json_object);
//                     // ----------------------------------------------------

//                 } else // Jika penskalaan tidak digunakan, buat JSON sederhana
//                 {
//                     char json_object[64];
//                     if (sensor_num > 0) {
//                         strcat(tp_msg[idx].message, ",");
//                     }
//                     sprintf(json_object, "{\"name\":\"%s\".\"raw_value\":%d}", sensor[i].name, sensor[i].raw_value);
//                     strcat(tp_msg[idx].message, json_object);
//                 }
//                 sensor_num++;
//             }
//         }
        
//         // Tutup array JSON dengan kurung siku
//         // strcat(tp_msg[idx].message, "]");

//         // Kirim pesan ke antrean FreeRTOS
//         if(uxQueueMessagesWaiting(xTPQueueHandle) > 4)
//         {
//             tTouchPad_msg *removed_msg;
//             xQueueReceive(xTPQueueHandle, &removed_msg, (TickType_t)10);
//             ESP_LOGI(TAG, "QUEUE IS FULL, OLD DATA REMOVED");
//         }
//         q_msg = &tp_msg[idx];
//         xQueueSendToBack(xTPQueueHandle, (void*)&q_msg, ( TickType_t ) 0);
        
//         idx++;
//         if(idx > 3) idx = 0;
        
//         vTaskDelay(250 / portTICK_PERIOD_MS);
//     }
// }















#include <stdio.h>
#include <string.h>
#include "tp_pad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <math.h> // Diperlukan untuk M_PI (nilai Pi)
#include "nvs_flash.h" // <--- DITAMBAHKAN
#include "nvs.h"       // <--- DITAMBAHKAN

#define TOTAL_HEIGHT_CM     160.0f  // Sesuaikan tinggi bak fisik (cm)
#define TOTAL_TIME_SEC      45.0f  // Sesuaikan waktu pengisian penuh (detik)
#define TARGET_SAMPLES      20      // Jumlah data tabel (ch8Cal)

static bool is_calibrating = false;
static TickType_t cal_start_tick = 0;
static int current_sample_idx = 0;

static const char *TAG = "TOUCH_PAD";
static QueueHandle_t xTPQueueHandle;
static tTouchPad_msg tp_msg[5];

// --- Definisikan dimensi tangki Anda di sini ---
#define TANK_RADIUS_CM 2.25f
// ---------------------------------------------

// --- Variabel global untuk melacak data volume dan waktu sebelumnya untuk perhitungan debit ---
static float last_measured_volume_liter[MAX_TOUCH_SENSOR]; 
static TickType_t last_measured_tick[MAX_TOUCH_SENSOR];      
static bool first_measurement_done[MAX_TOUCH_SENSOR];        
// -----------------------------------------------------------------------------------------

esp_err_t InitTPSensors(Touch_Sensor_t * sensors, int len, uint32_t filterPeriod)
{
    touch_pad_init();
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    
    for(int i = 0; i < len; i++)
    {
        touch_pad_config(sensors[i].num, sensors[i].threshold);
    }
    touch_pad_filter_start(filterPeriod);
    return ESP_OK;
}

esp_err_t ReadTPSensors(Touch_Sensor_t * sensor, int len)
{
    for(int i = 0; i < len; i++)
    {
        if(sensor[i].enable)
        {
            touch_pad_read_raw_data(sensor[i].num, &sensor[i].raw_value);
            touch_pad_read_filtered(sensor[i].num, &sensor[i].filtered_value);
        }
    }
    return ESP_OK;
}

QueueHandle_t InitTouchPadTask(tTouch_Sensor_Obj *touch_obj)
{
    xTPQueueHandle = xQueueCreate(5, sizeof(tTouchPad_msg *));
    xTaskCreate(vTouchReadTask, "TouchReadTask", 4096, (void*)touch_obj, 6, NULL); 
    return xTPQueueHandle; 
}

int32_t ScaleSensor(Touch_Sensor_t *sensor)
{
    int32_t tmpValue;
    int32_t rawValue;
    
    if(sensor->calTable == NULL)
        return sensor->raw_value;

    tCalibration_Table *tbl = sensor->calTable;
    int index;

    rawValue = (int32_t)sensor->raw_value << tbl->qBaseNum;

    if(sensor->raw_value >= sensor->threshold) return 0;
    
    if(rawValue >= tbl->sensReading[0])
    {
        tmpValue = 1 * tbl->xNum;
        tmpValue >>= tbl->qBaseNum;
        return tmpValue;
    }

    for(index = 0; index < tbl->paramCount - 1; index++)
    {
        if((rawValue <= tbl->sensReading[index]) && (rawValue >= tbl->sensReading[index+1]))
        {
            break; 
        }
    }

    if (index == tbl->paramCount - 1) {
        tmpValue = tbl->qActualRead[index] * tbl->xNum;
        tmpValue >>= tbl->qBaseNum;
        return tmpValue;
    }

    int32_t xa, xb, x;
    int32_t ya, yb;

    xa = tbl->sensReading[index];
    xb = tbl->sensReading[index+1];
    x  = rawValue;

    ya = tbl->qActualRead[index];
    yb = tbl->qActualRead[index+1];
    
    tmpValue = ((yb - ya) * (x - xa)) / (xb - xa);
    tmpValue += ya; 
    
    tmpValue *= tbl->xNum; 
    tmpValue >>= tbl->qBaseNum; 

    return (int32_t)tmpValue;
}

// =========================================================================
// FUNGSI KALIBRASI DAN NVS STORAGE
// =========================================================================

void Start_Auto_Calibration(void) {
    is_calibrating = true;
    current_sample_idx = 0;
    cal_start_tick = xTaskGetTickCount();
    ch8Cal.paramCount = 0; // Reset parameter lama
    ESP_LOGI(TAG, ">>> AUTO-CALIBRATION STARTED <<<");
}

// <--- FUNGSI BARU DITAMBAHKAN
void Save_Calibration_To_NVS(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_blob(my_handle, "cal_table", &ch8Cal, sizeof(tCalibration_Table));
        nvs_commit(my_handle);
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Tabel Kalibrasi Berhasil Disimpan ke NVS Flash!");
    } else {
        ESP_LOGE(TAG, "Gagal Membuka NVS Storage!");
    }
}

// <--- FUNGSI BARU DITAMBAHKAN
void Load_Calibration_From_NVS(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(tCalibration_Table);
        err = nvs_get_blob(my_handle, "cal_table", &ch8Cal, &required_size);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Tabel Kalibrasi Dimuat dari NVS! Total Titik: %d", ch8Cal.paramCount);
        } else {
            ESP_LOGW(TAG, "NVS Kosong, Menggunakan Data Kalibrasi Default/Hardcoded");
        }
        nvs_close(my_handle);
    }
}
// =========================================================================

void EnableScale(Touch_Sensor_t *sensor, tCalibration_Table *tbl, uint16_t threshold)
{
    sensor->calTable = tbl;
    sensor->threshold = threshold;
    sensor->useScale = true;
}

/**
 * @brief Task utama yang berjalan di latar belakang untuk membaca sensor,
 * menghitung nilai, dan mengirimkannya sebagai string JSON ke queue.
 */
// void vTouchReadTask(void *pvParameter)
// {
//     tTouch_Sensor_Obj *sensor_obj = (tTouch_Sensor_Obj *)pvParameter;
//     int idx = 0;
//     Touch_Sensor_t *sensor = sensor_obj->sensor;
//     int len = sensor_obj->length;
//     tTouchPad_msg *q_msg;
    
//     for(int i = 0; i < len; i++) {
//         first_measurement_done[i] = false;
//         last_measured_volume_liter[i] = 0.0f; 
//         last_measured_tick[i] = 0; 
//     }

//     while(1)
//     {
//         memset(tp_msg[idx].message, 0, sizeof(tp_msg[idx].message));

//         int sensor_num = 0;
        
//         for(int i = 0; i < len; i++)
//         {
//             if(sensor[i].enable)
//             {
//                 touch_pad_read_raw_data(sensor[i].num, &sensor[i].raw_value);
//                 touch_pad_read_filtered(sensor[i].num, &sensor[i].filtered_value);

//                 // --- LOGIKA SAMPLING AUTO-KALIBRASI ---
//                 if (is_calibrating) {
//                     float interval_sec = TOTAL_TIME_SEC / TARGET_SAMPLES;
//                     TickType_t elapsed_ticks = xTaskGetTickCount() - cal_start_tick;
//                     float elapsed_sec = (float)elapsed_ticks * portTICK_PERIOD_MS / 1000.0f;

//                     if (elapsed_sec >= (current_sample_idx * interval_sec) && current_sample_idx < TARGET_SAMPLES) {
                        
//                         uint32_t raw_cap = sensor[i].raw_value; 
//                         float estimated_height_cm = (current_sample_idx + 1) * (TOTAL_HEIGHT_CM / TARGET_SAMPLES);

//                         ch8Cal.sensReading[current_sample_idx] = (int32_t)raw_cap << ch8Cal.qBaseNum;
                        
//                         int32_t q_actual = (int32_t)((estimated_height_cm * (1 << ch8Cal.qBaseNum)) / ch8Cal.xNum);
//                         ch8Cal.qActualRead[current_sample_idx] = q_actual;

//                         current_sample_idx++;
//                         ch8Cal.paramCount = current_sample_idx;

//                         ESP_LOGI(TAG, "Sample [%d/%d] -> Raw: %ld, Height: %.1f cm", 
//                                  current_sample_idx, TARGET_SAMPLES, raw_cap, estimated_height_cm);

//                         if (current_sample_idx >= TARGET_SAMPLES) {
//                             is_calibrating = false;
//                             Save_Calibration_To_NVS(); 
//                             ESP_LOGI(TAG, ">>> AUTO-CALIBRATION SELESAI & TERSIMPAN DI NVS <<<");
//                         }
//                     }
//                 }
                
//                 // --- LOGIKA BACA SCALE NORMAL & JSON ---
//                 if(sensor[i].useScale && sensor[i].calTable != NULL)
//                 {
//                     int16_t val = ScaleSensor(&sensor[i]);
//                     float height_cm = (float)val / sensor[i].calTable->xNum;
//                     float volume_l = (M_PI * TANK_RADIUS_CM * TANK_RADIUS_CM * height_cm) / 1000.0f;
//                     float consumption_rate = 0.0f;
//                     TickType_t current_tick = xTaskGetTickCount();

//                     if (first_measurement_done[i]) {
//                         TickType_t elapsed_ticks = current_tick - last_measured_tick[i];
//                         float elapsed_ms = (float)elapsed_ticks * portTICK_PERIOD_MS;

//                         if (elapsed_ms > 0.0f) {
//                             float volume_change = last_measured_volume_liter[i] - volume_l;
//                             if (volume_change > 0.01f) { 
//                                 consumption_rate = (volume_change / elapsed_ms) * 60000.0f;
//                             } else {
//                                 consumption_rate = 0.0f;
//                             }
//                         }
//                     } else {
//                         first_measurement_done[i] = true;
//                     }
                    
//                     last_measured_volume_liter[i] = volume_l;
//                     last_measured_tick[i] = current_tick;
                    
//                     char json_object[128];

//                     if (sensor_num > 0) {
//                         strcat(tp_msg[idx].message, ",");
//                     }

//                     sprintf(json_object, 
//                             "{\"name\":\"%s\",\"height_cm\":%.2f,\"volume_l\":%.2f,\"flow_lpm\":%.2f}",
//                             sensor[i].name,
//                             height_cm,
//                             volume_l,
//                             consumption_rate
//                            );
                    
//                     strcat(tp_msg[idx].message, json_object);

//                 } else // Jika penskalaan tidak digunakan
//                 {
//                     char json_object[64];
//                     if (sensor_num > 0) {
//                         strcat(tp_msg[idx].message, ",");
//                     }
//                     // PERBAIKAN: Mengubah titik menjadi koma di format string JSON
//                     sprintf(json_object, "{\"name\":\"%s\",\"raw_value\":%d}", sensor[i].name, sensor[i].raw_value);
//                     strcat(tp_msg[idx].message, json_object);
//                 }
//                 sensor_num++;
//             }
//         }

//         if(uxQueueMessagesWaiting(xTPQueueHandle) > 4)
//         {
//             tTouchPad_msg *removed_msg;
//             xQueueReceive(xTPQueueHandle, &removed_msg, (TickType_t)10);
//             ESP_LOGI(TAG, "QUEUE IS FULL, OLD DATA REMOVED");
//         }
//         q_msg = &tp_msg[idx];
//         xQueueSendToBack(xTPQueueHandle, (void*)&q_msg, ( TickType_t ) 0);
        
//         idx++;
//         if(idx > 3) idx = 0;
        
//         vTaskDelay(250 / portTICK_PERIOD_MS);
//     }
// }

void vTouchReadTask(void *pvParameter)
{
    tTouch_Sensor_Obj *sensor_obj = (tTouch_Sensor_Obj *)pvParameter;
    int idx = 0;
    Touch_Sensor_t *sensor = sensor_obj->sensor;
    int len = sensor_obj->length;
    tTouchPad_msg *q_msg;
    
    for(int i = 0; i < len; i++) {
        first_measurement_done[i] = false;
        last_measured_volume_liter[i] = 0.0f; 
        last_measured_tick[i] = 0; 
    }

    while(1)
    {
        memset(tp_msg[idx].message, 0, sizeof(tp_msg[idx].message));

        int sensor_num = 0; // ✅ Direset setiap iterasi task
        
        for(int i = 0; i < len; i++)
        {
            if(sensor[i].enable)
            {
                touch_pad_read_raw_data(sensor[i].num, &sensor[i].raw_value);
                touch_pad_read_filtered(sensor[i].num, &sensor[i].filtered_value);

                // --- LOGIKA SAMPLING AUTO-KALIBRASI ---
                if (is_calibrating) {
                    float interval_sec = TOTAL_TIME_SEC / TARGET_SAMPLES;
                    TickType_t elapsed_ticks = xTaskGetTickCount() - cal_start_tick;
                    float elapsed_sec = (float)elapsed_ticks * portTICK_PERIOD_MS / 1000.0f;

                    if (elapsed_sec >= (current_sample_idx * interval_sec) && current_sample_idx < TARGET_SAMPLES) {
                        
                        uint32_t raw_cap = sensor[i].raw_value; 
                        // float estimated_height_cm = (current_sample_idx + 1) * (TOTAL_HEIGHT_CM / TARGET_SAMPLES);
                        float estimated_height_cm = current_sample_idx * (TOTAL_HEIGHT_CM / (TARGET_SAMPLES - 1));

                        ch8Cal.sensReading[current_sample_idx] = (int32_t)raw_cap << ch8Cal.qBaseNum;
                        
                        // ✅ PERBAIKAN BUG UTAMA: Menghilangkan pembagian ch8Cal.xNum
                        int32_t q_actual = (int32_t)(estimated_height_cm * (1 << ch8Cal.qBaseNum));
                        ch8Cal.qActualRead[current_sample_idx] = q_actual;

                        current_sample_idx++;
                        ch8Cal.paramCount = current_sample_idx;

                        ESP_LOGI(TAG, "Sample [%d/%d] -> Raw: %ld, Height: %.1f cm", 
                                 current_sample_idx, TARGET_SAMPLES, raw_cap, estimated_height_cm);

                        if (current_sample_idx >= TARGET_SAMPLES) {
                            is_calibrating = false;
                            Save_Calibration_To_NVS(); 
                            ESP_LOGI(TAG, ">>> AUTO-CALIBRATION SELESAI & TERSIMPAN DI NVS <<<");
                        }
                    }
                }
                
                // --- LOGIKA BACA SCALE NORMAL & JSON ---
                if(sensor[i].useScale && sensor[i].calTable != NULL)
                {
                    int32_t val = ScaleSensor(&sensor[i]);
                    float height_cm = (float)val / sensor[i].calTable->xNum;
                    float volume_l = (M_PI * TANK_RADIUS_CM * TANK_RADIUS_CM * height_cm) / 1000.0f;
                    float consumption_rate = 0.0f;
                    TickType_t current_tick = xTaskGetTickCount();

                    if (first_measurement_done[i]) {
                        TickType_t elapsed_ticks = current_tick - last_measured_tick[i];
                        float elapsed_ms = (float)elapsed_ticks * portTICK_PERIOD_MS;

                        if (elapsed_ms > 0.0f) {
                            float volume_change = last_measured_volume_liter[i] - volume_l;
                            if (volume_change > 0.01f) { 
                                consumption_rate = (volume_change / elapsed_ms) * 60000.0f;
                            } else {
                                consumption_rate = 0.0f;
                            }
                        }
                    } else {
                        first_measurement_done[i] = true;
                    }
                    
                    last_measured_volume_liter[i] = volume_l;
                    last_measured_tick[i] = current_tick;
                    
                    char json_object[128];

                    if (sensor_num > 0) {
                        strcat(tp_msg[idx].message, ",");
                    }

                    sprintf(json_object, 
                            "{\"name\":\"%s\",\"height_cm\":%.2f,\"volume_l\":%.2f,\"flow_lpm\":%.2f}",
                            sensor[i].name,
                            height_cm,
                            volume_l,
                            consumption_rate
                           );
                    
                    strcat(tp_msg[idx].message, json_object);

                } else // Jika penskalaan tidak digunakan
                {
                    char json_object[64];
                    if (sensor_num > 0) {
                        strcat(tp_msg[idx].message, ",");
                    }
                    sprintf(json_object, "{\"name\":\"%s\",\"raw_value\":%d}", sensor[i].name, sensor[i].raw_value);
                    strcat(tp_msg[idx].message, json_object);
                }
                sensor_num++;
            }
        }

        if(uxQueueMessagesWaiting(xTPQueueHandle) > 4)
        {
            tTouchPad_msg *removed_msg;
            xQueueReceive(xTPQueueHandle, &removed_msg, (TickType_t)10);
            ESP_LOGI(TAG, "QUEUE IS FULL, OLD DATA REMOVED");
        }
        q_msg = &tp_msg[idx];
        xQueueSendToBack(xTPQueueHandle, (void*)&q_msg, ( TickType_t ) 0);
        
        idx++;
        if(idx > 3) idx = 0;
        
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}