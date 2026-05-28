#include "device_settings.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG          "dev_settings"
#define POLL_MS      100   // check every 100 ms
#define HOLD_COUNT   30    // 30 × 100 ms = 3 s
#define COOLDOWN_MS  3000  // ignore button for 3 s after firing

typedef struct {
    int                  gpio_num;
    device_settings_cb_t cb;
} args_t;

static void monitor_task(void *arg)
{
    args_t *a = (args_t *)arg;
    int held = 0;

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << a->gpio_num,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    while (1) {
        if (gpio_get_level(a->gpio_num) == 0) {
            if (++held == HOLD_COUNT) {
                ESP_LOGI(TAG, "3-second hold detected on GPIO%d", a->gpio_num);
                a->cb();
                held = 0;
                vTaskDelay(pdMS_TO_TICKS(COOLDOWN_MS));
            }
        } else {
            held = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void device_settings_start(int gpio_num, device_settings_cb_t on_hold)
{
    static args_t a;  // outlives this call
    a.gpio_num = gpio_num;
    a.cb       = on_hold;
    xTaskCreate(monitor_task, "dev_settings", 2048, &a, 1, NULL);
}
