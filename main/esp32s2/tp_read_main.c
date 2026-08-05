#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "esp_log.h"

#define TOUCH_BUTTON_NUM 9  // Menggunakan T1 sampai T9

static const char *TAG = "TOUCH_READ";

// Daftar touch pad yang digunakan (T1-T9)
static const touch_pad_t touch_buttons[TOUCH_BUTTON_NUM] = {
    TOUCH_PAD_NUM1,
    TOUCH_PAD_NUM2,
    TOUCH_PAD_NUM3,
    TOUCH_PAD_NUM4,
    TOUCH_PAD_NUM5,
    TOUCH_PAD_NUM6,
    TOUCH_PAD_NUM7,
    TOUCH_PAD_NUM8,
    TOUCH_PAD_NUM9
};

void tp_read_task(void *pvParameter)
{
    uint32_t raw_val, filtered_val;

    vTaskDelay(pdMS_TO_TICKS(100));  // Tunggu inisialisasi selesai

    while (1) {
        for (int i = 0; i < TOUCH_BUTTON_NUM; i++) {
            touch_pad_read_raw_data(touch_buttons[i], &raw_val);
            touch_pad_read_filtered(touch_buttons[i], &filtered_val);
            ESP_LOGI(TAG, "T%d -> Raw: %4d | Filtered: %4d", touch_buttons[i], raw_val, filtered_val);
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void app_main(void)
{
    // Inisialisasi touch pad driver
    touch_pad_init();

    // Konfigurasi tiap touch pad
    for (int i = 0; i < TOUCH_BUTTON_NUM; i++) {
        touch_pad_config(touch_buttons[i], 0);  // Threshold 0, hanya raw read
    }

    // Konfigurasi filter agar fungsi read_filtered() valid
    touch_pad_filter_start(10);  // Filter period = 10 * 8ms = 80ms

    // Mulai finite state machine internal untuk touch pad
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();

    // Jalankan task pembacaan
    xTaskCreate(&tp_read_task, "tp_read_task", 2048, NULL, 5, NULL);
}
