#include "audio_hal.h"
#include "usb_midi_host.h"
#include "synth_engine.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <math.h>

static const char *TAG = "app_main";

#if CONFIG_NEKTARINA_BOOT_TEST_TONE
static esp_err_t play_boot_test_tone(void)
{
    enum { BUF_FRAMES = 128 };
    const uint32_t total_frames = AUDIO_SAMPLE_RATE / 4;
    int16_t buf[BUF_FRAMES * 2] = {0};
    float phase = 0.0f;
    const float phase_inc = 440.0f / (float)AUDIO_SAMPLE_RATE;

    ESP_LOGI(TAG, "Playing optional 250 ms boot diagnostic tone");
    for (uint32_t rendered = 0; rendered < total_frames;) {
        uint32_t frames = total_frames - rendered;
        if (frames > BUF_FRAMES) {
            frames = BUF_FRAMES;
        }
        for (uint32_t i = 0; i < frames; i++) {
            float sample = sinf(2.0f * (float)M_PI * phase);
            int16_t val = (int16_t)(sample * 4000.0f);
            buf[2 * i + 0] = val;
            buf[2 * i + 1] = val;
            phase += phase_inc;
            if (phase >= 1.0f) {
                phase -= 1.0f;
            }
        }

        size_t written = 0;
        size_t bytes = frames * 2 * sizeof(int16_t);
        esp_err_t err = audio_hal_write(buf, bytes, &written, pdMS_TO_TICKS(100));
        if (err != ESP_OK || written != bytes) {
            ESP_LOGE(TAG, "Boot tone write failed: %s (%u/%u bytes)",
                     esp_err_to_name(err), (unsigned)written, (unsigned)bytes);
            return err != ESP_OK ? err : ESP_FAIL;
        }
        rendered += frames;
    }
    return ESP_OK;
}
#endif

static void on_usb_midi_connection(bool connected, void *user_ctx)
{
    QueueHandle_t midi_queue = (QueueHandle_t)user_ctx;
    if (connected) {
        ESP_LOGI("MIDI", ">>> Nektar MIDI keyboard CONNECTED and active <<<");
    } else {
        ESP_LOGW("MIDI", ">>> Nektar MIDI keyboard DISCONNECTED <<<");
        midi_message_t all_notes_off = {
            .type = MIDI_EVENT_CC,
            .data1 = 123,
            .data2 = 0,
        };
        if (!midi_queue || xQueueSendToFront(midi_queue, &all_notes_off, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGE("MIDI", "Unable to queue All Notes Off after disconnect");
        }
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

    /* 2. Initialize Audio HAL. It owns SD_MODE=GPIO14 and starts muted. */
    esp_err_t ret = audio_hal_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Audio HAL: %s", esp_err_to_name(ret));
        return;
    }

#if CONFIG_NEKTARINA_BOOT_TEST_TONE
    ret = play_boot_test_tone();
    if (ret != ESP_OK) {
        audio_hal_deinit();
        return;
    }
#endif

    /* 3. Create the queue before starting either producer or consumer. */
    QueueHandle_t midi_queue = xQueueCreate(64, sizeof(midi_message_t));
    if (!midi_queue) {
        ESP_LOGE(TAG, "Failed to create MIDI queue");
        audio_hal_deinit();
        return;
    }

    /* 4. Start the synth as the only continuous I2S writer. */
    ret = synth_engine_init(midi_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize synth engine: %s", esp_err_to_name(ret));
        vQueueDelete(midi_queue);
        audio_hal_deinit();
        return;
    }
    ret = synth_engine_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start synth engine: %s", esp_err_to_name(ret));
        vQueueDelete(midi_queue);
        audio_hal_deinit();
        return;
    }

    /* 5. Start USB MIDI only after its queue consumer is running. */
    ret = usb_midi_host_init(midi_queue, on_usb_midi_connection, midi_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize USB MIDI Host: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Waiting for Nektar MIDI keyboard on USB-OTG port...");
    }

    /* 6. Supervisor heartbeat. */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        uint32_t cur_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        uint32_t cur_spiram   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        UBaseType_t queued = uxQueueMessagesWaiting(midi_queue);
        synth_engine_status_t synth_status = {0};
        synth_engine_get_status(&synth_status);
        ESP_LOGI("STATUS", "USB=%d MIDI=%u/64 dropped=%lu voices=%u I2S err=%lu short=%lu Internal=%lu PSRAM=%lu Stack app=%u synth=%u",
                 usb_midi_host_is_connected(), (unsigned)queued,
                 (unsigned long)usb_midi_host_get_dropped_messages(),
                 (unsigned)synth_status.active_voices,
                 (unsigned long)synth_status.audio_write_errors,
                 (unsigned long)synth_status.audio_short_writes,
                 (unsigned long)cur_internal, (unsigned long)cur_spiram,
                 (unsigned)uxTaskGetStackHighWaterMark(NULL),
                 (unsigned)synth_status.task_stack_high_water);
    }
}
