#include "audio_hal.h"
#include "usb_midi_host.h"
#include "synth_engine.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <math.h>
#include <string.h>

static const char *TAG = "app_main";

static void i2s_audio_task(void *param)
{
    (void)param;
    const uint32_t SAMPLE_RATE = AUDIO_SAMPLE_RATE;
    const int BUF_FRAMES = 128;
    int16_t buf[BUF_FRAMES * 2];

    uint16_t freq = 440;
    float phase = 0.0f;
    float phase_inc = (float)440 / (float)SAMPLE_RATE;
    float amp_current = 16000.0f;
    uint32_t iteration = 0;

    ESP_LOGI(TAG, "i2s_audio_task running on core %d (InvaderESP engine)", xPortGetCoreID());

    while (1) {
        iteration++;
        uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        /* Cycle volume every 3 seconds: 800 (ultra-low power ~25mA), 4000 (low), 12000 (normal) */
        uint32_t period = (now / 3000) % 3;
        float target_amp = 800.0f;
        if (period == 1) {
            target_amp = 4000.0f;
        } else if (period == 2) {
            target_amp = 12000.0f;
        }

        for (int i = 0; i < BUF_FRAMES; i++) {
            amp_current += (target_amp - amp_current) * 0.02f;
            float sample = sinf(2.0f * (float)M_PI * phase);
            int16_t val = (int16_t)(sample * amp_current);
            buf[2 * i + 0] = val;
            buf[2 * i + 1] = val;
            phase += phase_inc;
            if (phase >= 1.0f) phase -= 1.0f;
        }

        size_t written = 0;
        esp_err_t err = audio_hal_write(buf, sizeof(buf), &written, pdMS_TO_TICKS(100));

        if (iteration % 125 == 0) { // approx every 1 second (125 * 8ms = 1000ms)
            int sd_lvl = gpio_get_level(GPIO_NUM_14);
            int gain_lvl = gpio_get_level(GPIO_NUM_21);
            ESP_LOGI("AUDIO_DIAG", "err=%s written=%u/%u freq=%u amp=%.0f SD(GPIO14)=%d GAIN(GPIO21)=%d",
                     esp_err_to_name(err), (unsigned)written, (unsigned)sizeof(buf), freq, amp_current, sd_lvl, gain_lvl);
        }
    }
}

static void on_usb_midi_connection(bool connected, void *user_ctx)
{
    (void)user_ctx;
    if (connected) {
        ESP_LOGI("MIDI", ">>> Nektar MIDI keyboard CONNECTED and active <<<");
    } else {
        ESP_LOGW("MIDI", ">>> Nektar MIDI keyboard DISCONNECTED <<<");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "===============================================================");
    ESP_LOGI(TAG, "  NEKTAR-MIDI ESP32-S3-WROOM-1-N16R8 Synthesizer Firmware      ");
    ESP_LOGI(TAG, "===============================================================");

    /* 1. Memory telemetry */
    uint32_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t free_spiram   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t free_dma      = heap_caps_get_free_size(MALLOC_CAP_DMA);

    ESP_LOGI("HEAP", "Free Internal RAM: %lu bytes (%.2f KB)",
             (unsigned long)free_internal, (float)free_internal / 1024.0f);
    ESP_LOGI("HEAP", "Free Octal PSRAM:  %lu bytes (%.2f MB)",
             (unsigned long)free_spiram, (float)free_spiram / (1024.0f * 1024.0f));
    ESP_LOGI("HEAP", "Free DMA RAM:      %lu bytes (%.2f KB)",
             (unsigned long)free_dma, (float)free_dma / 1024.0f);

    /* 2. Configure SD and GAIN with input-output capability so we can read back actual level */
    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_NUM_14));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_14, 0)); // Start in shutdown while clocks initialize

    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_NUM_21));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_21, GPIO_MODE_INPUT_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_21, 1));

    /* 3. Initialize Audio HAL (MAX98357A on GPIO 45, 3, 47) - Starts clocks */
    ESP_ERROR_CHECK(audio_hal_init());

    /* 4. Allow clocks to stabilize for 50ms, then assert SD_MODE HIGH (proper power-up sequence) */
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_14, 1));
    ESP_LOGI(TAG, "MAX98357A SD_MODE enabled (HIGH on GPIO 14) after I2S clock start");

    /* 4. Start continuous background i2s_audio_task */
    if (xTaskCreatePinnedToCore(i2s_audio_task, "i2s_audio_task", 4096, NULL, 5, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create i2s_audio_task");
        return;
    }

    /* 5. Create MIDI event queue */
    QueueHandle_t midi_queue = xQueueCreate(64, sizeof(midi_message_t));
    if (!midi_queue) {
        ESP_LOGE(TAG, "Failed to create MIDI queue");
        return;
    }

    /* 6. Initialize USB MIDI Host */
    esp_err_t ret = usb_midi_host_init(midi_queue, on_usb_midi_connection, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize USB MIDI Host: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Waiting for Nektar MIDI keyboard on USB-OTG port...");
    }

    /* 7. Heartbeat loop */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        uint32_t cur_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        uint32_t cur_spiram   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI("STATUS", "Heartbeat: USB Connected=%d, Free Internal=%lu, Free PSRAM=%lu",
                 usb_midi_host_is_connected(), (unsigned long)cur_internal, (unsigned long)cur_spiram);
    }
}
