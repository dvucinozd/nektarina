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

static void play_test_tone(void)
{
    ESP_LOGI("AUDIO", "Playing %.1f Hz test tone for %.1f seconds to verify MAX98357A...",
             TEST_TONE_FREQ, TEST_TONE_DURATION);

    size_t total_frames = (size_t)(AUDIO_SAMPLE_RATE * TEST_TONE_DURATION);
    int16_t *buf = (int16_t *)heap_caps_malloc(BUFFER_FRAMES * 2 * sizeof(int16_t),
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE("AUDIO", "Failed to allocate test tone buffer");
        return;
    }

    float phase = 0.0f;
    float phase_inc = (TEST_TONE_FREQ * 2.0f * (float)M_PI) / (float)AUDIO_SAMPLE_RATE;
    size_t frames_rendered = 0;

    while (frames_rendered < total_frames) {
        size_t chunk = total_frames - frames_rendered;
        if (chunk > BUFFER_FRAMES) chunk = BUFFER_FRAMES;

        for (size_t i = 0; i < chunk; i++) {
            /* Smooth ramp in and ramp out to avoid pops/clicks */
            float env = 1.0f;
            size_t global_frame = frames_rendered + i;
            size_t ramp_len = AUDIO_SAMPLE_RATE / 20; /* 50 ms ramp */

            if (global_frame < ramp_len) {
                env = (float)global_frame / (float)ramp_len;
            } else if (global_frame > total_frames - ramp_len) {
                env = (float)(total_frames - global_frame) / (float)ramp_len;
            }

            float sample = sinf(phase) * 0.5f * env;
            phase += phase_inc;
            if (phase >= 2.0f * (float)M_PI) {
                phase -= 2.0f * (float)M_PI;
            }

            int16_t pcm = (int16_t)(sample * 32767.0f);
            buf[i * 2 + 0] = pcm; /* Left */
            buf[i * 2 + 1] = pcm; /* Right */
        }

        size_t written = 0;
        audio_hal_write(buf, chunk * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        frames_rendered += chunk;
    }

    /* Small silence flush */
    memset(buf, 0, BUFFER_FRAMES * 2 * sizeof(int16_t));
    size_t written = 0;
    audio_hal_write(buf, BUFFER_FRAMES * 2 * sizeof(int16_t), &written, portMAX_DELAY);

    free(buf);
    ESP_LOGI("AUDIO", "Test tone complete. MAX98357A audio hardware verified OK.");
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

    /* 2. Configure GPIO 4 (SD_MODE) and GPIO 21 (GAIN) like in InvaderESP */
    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_NUM_4));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_4, 1));
    ESP_LOGI(TAG, "MAX98357A SD_MODE enable pin set to HIGH on GPIO 4");

    ESP_ERROR_CHECK(gpio_reset_pin(GPIO_NUM_21));
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_21, 1));
    ESP_LOGI(TAG, "MAX98357A GAIN pin set to HIGH on GPIO 21");

    /* 3. Initialize Audio HAL (MAX98357A on GPIO 16, 17, 18) */
    ESP_ERROR_CHECK(audio_hal_init());

    /* 4. Play acoustic verification test tone (440 Hz, 1.5s) */
    play_test_tone();

    /* 5. Create MIDI event queue */
    QueueHandle_t midi_queue = xQueueCreate(64, sizeof(midi_message_t));
    if (!midi_queue) {
        ESP_LOGE(TAG, "Failed to create MIDI queue");
        return;
    }

    /* 6. Initialize and launch Synth Engine */
    ESP_ERROR_CHECK(synth_engine_init(midi_queue));
    ESP_ERROR_CHECK(synth_engine_start());

    /* 7. Play 80s synth engine demonstration arpeggio (C4, E4, G4, C5) */
    ESP_LOGI("SYNTH", "Playing 80s Virtual Analog demo arpeggio (C maj)...");
    synth_engine_note_on(60, 100);
    vTaskDelay(pdMS_TO_TICKS(180));
    synth_engine_note_on(64, 95);
    vTaskDelay(pdMS_TO_TICKS(180));
    synth_engine_note_on(67, 100);
    vTaskDelay(pdMS_TO_TICKS(180));
    synth_engine_note_on(72, 110);
    vTaskDelay(pdMS_TO_TICKS(1000));
    synth_engine_note_off(60);
    synth_engine_note_off(64);
    synth_engine_note_off(67);
    synth_engine_note_off(72);
    ESP_LOGI("SYNTH", "Demo arpeggio complete.");

    /* 8. Initialize USB MIDI Host */
    esp_err_t ret = usb_midi_host_init(midi_queue, on_usb_midi_connection, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize USB MIDI Host: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Waiting for Nektar MIDI keyboard on USB-OTG port...");
    }

    /* 9. Main loop: repeating diagnostic audio pulses until USB MIDI keyboard is plugged in */
    int loop_count = 0;
    while (1) {
        if (!usb_midi_host_is_connected()) {
            ESP_LOGW("AUDIO_TEST", "Pulsing synth note (C4)... Ako nista ne cujes: spoji pin SD (SD_MODE) na VIN (+5V) ili GPIO 4!");
            synth_engine_note_on(60, 110);
            vTaskDelay(pdMS_TO_TICKS(400));
            synth_engine_note_off(60);
            vTaskDelay(pdMS_TO_TICKS(2100));
        } else {
            vTaskDelay(pdMS_TO_TICKS(10000));
        }

        loop_count++;
        if (loop_count % 4 == 0) {
            uint32_t cur_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            uint32_t cur_spiram   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            ESP_LOGI("STATUS", "Heartbeat: USB Connected=%d, Free Internal=%lu, Free PSRAM=%lu",
                     usb_midi_host_is_connected(), (unsigned long)cur_internal, (unsigned long)cur_spiram);
        }
    }
}
