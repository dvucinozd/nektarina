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

#define TEST_TONE_FREQ      440.0f
#define TEST_TONE_DURATION  1.5f
#define BUFFER_FRAMES       256

static void play_diagnostic_beep(float freq, float duration_s, float amplitude)
{
    size_t total_frames = (size_t)(AUDIO_SAMPLE_RATE * duration_s);
    int16_t *buf = (int16_t *)heap_caps_malloc(BUFFER_FRAMES * 2 * sizeof(int16_t),
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE("AUDIO", "Failed to allocate test tone buffer");
        return;
    }

    float phase = 0.0f;
    float phase_inc = (freq * 2.0f * (float)M_PI) / (float)AUDIO_SAMPLE_RATE;
    size_t frames_rendered = 0;
    size_t total_written = 0;

    while (frames_rendered < total_frames) {
        size_t chunk = total_frames - frames_rendered;
        if (chunk > BUFFER_FRAMES) chunk = BUFFER_FRAMES;

        for (size_t i = 0; i < chunk; i++) {
            float sample = sinf(phase);
            phase += phase_inc;
            if (phase >= 2.0f * (float)M_PI) {
                phase -= 2.0f * (float)M_PI;
            }

            int16_t pcm = (int16_t)(sample * amplitude);
            buf[i * 2 + 0] = pcm; /* Left */
            buf[i * 2 + 1] = pcm; /* Right */
        }

        size_t written = 0;
        esp_err_t err = audio_hal_write(buf, chunk * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE("AUDIO", "audio_hal_write failed: %s", esp_err_to_name(err));
        }
        total_written += written;
        frames_rendered += chunk;
    }

    /* Silence flush */
    memset(buf, 0, BUFFER_FRAMES * 2 * sizeof(int16_t));
    size_t written = 0;
    audio_hal_write(buf, BUFFER_FRAMES * 2 * sizeof(int16_t), &written, portMAX_DELAY);

    free(buf);
    ESP_LOGI("AUDIO", "Diagnostic beep (%.1f Hz, %.1fs) finished. Written: %u bytes.",
             freq, duration_s, (unsigned)total_written);
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

    /* 2. Configure GPIO 14 (SD_MODE) and GPIO 21 (GAIN) */
    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_NUM_14));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_14, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_14, 1));
    ESP_LOGI(TAG, "MAX98357A SD_MODE enable pin set to HIGH on GPIO 14");

    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_NUM_21));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_21, 1));
    ESP_LOGI(TAG, "MAX98357A GAIN pin set to HIGH (6 dB standard gain) on GPIO 21");

    /* 3. Initialize Audio HAL (MAX98357A on GPIO 45, 3, 47) */
    ESP_ERROR_CHECK(audio_hal_init());

    /* 4. Create MIDI event queue */
    QueueHandle_t midi_queue = xQueueCreate(64, sizeof(midi_message_t));
    if (!midi_queue) {
        ESP_LOGE(TAG, "Failed to create MIDI queue");
        return;
    }

    /* 5. Initialize USB MIDI Host */
    esp_err_t ret = usb_midi_host_init(midi_queue, on_usb_midi_connection, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize USB MIDI Host: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Waiting for Nektar MIDI keyboard on USB-OTG port...");
    }

    /* 6. Diagnostic loop: 3 distinct volume steps to diagnose clipping vs signal */
    int loop_count = 0;
    bool synth_running = false;

    while (1) {
        if (!usb_midi_host_is_connected()) {
            loop_count++;
            ESP_LOGW("AUDIO_TEST", "=== Test Cycle #%d ===", loop_count);

            ESP_LOGW("AUDIO_TEST", "[Step 1] GENTLE 440 Hz (amp=2500, no clipping)...");
            play_diagnostic_beep(440.0f, 1.5f, 2500.0f);
            vTaskDelay(pdMS_TO_TICKS(500));

            ESP_LOGW("AUDIO_TEST", "[Step 2] MEDIUM 554 Hz (amp=7000)...");
            play_diagnostic_beep(554.0f, 1.5f, 7000.0f);
            vTaskDelay(pdMS_TO_TICKS(500));

            ESP_LOGW("AUDIO_TEST", "[Step 3] STRONG 659 Hz (amp=16000)...");
            play_diagnostic_beep(659.0f, 1.5f, 16000.0f);
            vTaskDelay(pdMS_TO_TICKS(1500));
        } else {
            if (!synth_running) {
                ESP_LOGI(TAG, "Nektar keyboard connected! Starting synth engine...");
                ESP_ERROR_CHECK(synth_engine_init(midi_queue));
                ESP_ERROR_CHECK(synth_engine_start());
                synth_running = true;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        if (loop_count % 5 == 0) {
            uint32_t cur_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            uint32_t cur_spiram   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            ESP_LOGI("STATUS", "Heartbeat: USB Connected=%d, Free Internal=%lu, Free PSRAM=%lu",
                     usb_midi_host_is_connected(), (unsigned long)cur_internal, (unsigned long)cur_spiram);
        }
    }
}
