#include "audio_hal.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static int16_t mic_samples[AUDIO_RATE];

static void microphone(void)
{
    esp_err_t ret = audio_capture(mic_samples, AUDIO_RATE);
    if (ret != ESP_OK) { ESP_LOGE("M1", "MIC capture failed: %s", esp_err_to_name(ret)); return; }
    printf("MICBEGIN rate=%d samples=%d\n", AUDIO_RATE, AUDIO_RATE);
    fflush(stdout);
    char line[300];
    for (unsigned i = 0; i < AUDIO_RATE; i += 64) {
        int len = snprintf(line, sizeof(line), "MIC %06u ", i);
        for (unsigned j = 0; j < 64 && i + j < AUDIO_RATE; ++j)
            len += snprintf(line + len, sizeof(line) - len, "%04x", (unsigned)(uint16_t)mic_samples[i+j]);
        line[len++] = '\n';
        uart_write_bytes(UART_NUM_0, line, len);
    }
    printf("MICEND\n");
    fflush(stdout);
}

static void status(void)
{
    audio_stats_t s = audio_stats();
    ESP_LOGI("M1", "blocks=%lu dma=%lu render_max_us=%lu deadlines=%lu write_errors=%lu short=%lu tx_q_ovf=%lu gaps=%lu running=%d failed=%d",
        (unsigned long)s.blocks, (unsigned long)s.dma_completions,
        (unsigned long)s.render_max_us, (unsigned long)s.render_deadlines,
        (unsigned long)s.write_errors, (unsigned long)s.short_writes,
        (unsigned long)s.tx_queue_overflows, (unsigned long)s.service_gaps, s.running, s.failed);
}

void app_main(void)
{
    ESP_LOGI("M1", "48kHz mono TX, internal stereo, 128 frames, 440Hz; boot muted");
    esp_err_t ret = audio_init();
    if (ret != ESP_OK) { ESP_LOGE("M1", "Audio init failed: %s; PA off", esp_err_to_name(ret)); return; }
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(audio_launch());
    ESP_LOGI("M1", "READY: t/1=tone 1%%, 2=tone 2%%, m=mute, x=stop DMA, s=start muted, r=mic capture, ?=status");
    unsigned ticks = 0;
    for (;;) {
        uint8_t c;
        if (uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100)) == 1) {
            if (c == 't' || c == '1') audio_control(true, 10);
            else if (c == '2') audio_control(true, 20);
            else if (c == 'm' || c == 's') audio_control(true, 0);
            else if (c == 'x') audio_control(false, 0);
            else if (c == 'r') microphone();
            if (c != '\r' && c != '\n') { ESP_LOGI("M1", "command=%c", c); status(); }
        }
        if (++ticks >= 100) { status(); ticks = 0; }
    }
}
