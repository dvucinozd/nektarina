#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#define AUDIO_RATE 48000
#define AUDIO_FRAMES 128
typedef struct {
    uint32_t blocks, render_max_us, render_deadlines, write_errors, short_writes;
    uint32_t tx_queue_overflows, service_gaps, dma_completions;
    bool running, failed;
} audio_stats_t;
/* One initialization per boot; failure is latched with PA disabled. */
esp_err_t audio_init(void);
esp_err_t audio_launch(void);
/* Control-task API: gain 0..50 permille, ramped in the audio task. */
void audio_control(bool running, unsigned gain_permille);
audio_stats_t audio_stats(void);
/* Control-task only; bounded RX capture while TX continues. */
esp_err_t audio_capture(int16_t *samples, unsigned count);
