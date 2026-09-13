#include "audio_hal.h"
#include "board_jc_esp32p4_m3.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev_defaults.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>

static i2s_chan_handle_t tx, rx;
static _Atomic unsigned rx_overflows;
static const audio_codec_if_t *codec;
static gpio_num_t pa;
static bool initialized;
static _Atomic unsigned requested; /* bit 16: run, lower bits: permille */
static _Atomic unsigned blocks, render_max, deadlines, errors, shorts;
static _Atomic unsigned overflows, gaps, completions, running, failed;
static float wave[1200], left[AUDIO_FRAMES], right[AUDIO_FRAMES];
static int16_t pcm[AUDIO_FRAMES];
static StaticTask_t task_state;
static StackType_t task_stack[4096];

static bool sent(i2s_chan_handle_t h, i2s_event_data_t *e, void *ctx)
{
    (void)h; (void)e; (void)ctx;
    atomic_fetch_add_explicit(&completions, 1, memory_order_relaxed);
    return false;
}
static bool overflow(i2s_chan_handle_t h, i2s_event_data_t *e, void *ctx)
{
    (void)h; (void)e; (void)ctx;
    atomic_fetch_add_explicit(&overflows, 1, memory_order_relaxed);
    return false;
}
static bool rx_overflow(i2s_chan_handle_t h, i2s_event_data_t *e, void *ctx)
{
    (void)h; (void)e; (void)ctx;
    atomic_fetch_add_explicit(&rx_overflows, 1, memory_order_relaxed);
    return false;
}

esp_err_t audio_init(void)
{
    const nektar_board_audio_t *b = &nektar_board_get()->audio;
    pa = b->pa_enable;
    /* Set latch before enabling output; hardware pull-down holds PA off at reset. */
    esp_err_t ret = gpio_set_level(pa, 0);
    if (ret != ESP_OK) return ret;
    ret = gpio_set_direction(pa, GPIO_MODE_OUTPUT);
    if (ret != ESP_OK) return ret;
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = 0, .sda_io_num = b->sda, .scl_io_num = b->scl,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ret = i2c_new_master_bus(&bus_cfg, &bus);
    if (ret != ESP_OK) return ret;
    ret = i2c_master_probe(bus, b->codec_address_7bit, 100);
    if (ret != ESP_OK) return ret;
    ESP_LOGI("audio", "ES8311 address ACK: 0x%02x", b->codec_address_7bit);
    audio_codec_i2c_cfg_t ctrl_cfg = {
        .port = 0, .addr = b->codec_address_7bit << 1,
        .bus_handle = bus, .clock_speed_hz = 100000,
    };
    const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&ctrl_cfg);
    if (!ctrl) return ESP_ERR_NO_MEM;
    es8311_codec_cfg_t cfg = {
        .ctrl_if = ctrl, .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = -1, /* PA owned here; codec must not unmute the amplifier. */
        .use_mclk = true, .mclk_div = 256,
        .no_dac_ref = true, /* ADC microphone only, no internal DAC reference. */
    };
    codec = es8311_codec_new(&cfg);
    if (!codec) return ESP_FAIL;
    i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ch.dma_desc_num = 6;
    ch.dma_frame_num = AUDIO_FRAMES;
    ch.auto_clear_after_cb = true;
    ret = i2s_new_channel(&ch, &tx, &rx);
    if (ret != ESP_OK) return ret;
    i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {.mclk = b->mclk, .bclk = b->bclk, .ws = b->ws,
                     .dout = b->dout, .din = b->din},
    };
    std.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    ret = i2s_channel_init_std_mode(tx, &std);
    if (ret != ESP_OK) return ret;
    ret = i2s_channel_init_std_mode(rx, &std);
    if (ret != ESP_OK) return ret;
    i2s_event_callbacks_t rx_callbacks = {.on_recv_q_ovf = rx_overflow};
    ret = i2s_channel_register_event_callback(rx, &rx_callbacks, NULL);
    if (ret != ESP_OK) return ret;
    i2s_event_callbacks_t callbacks = {.on_sent = sent, .on_send_q_ovf = overflow};
    ret = i2s_channel_register_event_callback(tx, &callbacks, NULL);
    if (ret != ESP_OK) return ret;
    /* Keep MCLK present while programming the codec, with PA held low. */
    ret = i2s_channel_enable(tx);
    if (ret != ESP_OK) return ret;
    esp_codec_dev_sample_info_t fs = {.sample_rate = AUDIO_RATE, .bits_per_sample = 16, .channel = 1};
    if (codec->set_fs(codec, &fs) || codec->enable(codec, true) ||
        codec->set_vol(codec, -12.0f) || codec->set_mic_gain(codec, 24.0f) ||
        codec->mute(codec, false)) {
        i2s_channel_disable(tx);
        return ESP_FAIL;
    }
    int iface = 0;
    if (codec->get_reg(codec, 0x09, &iface) || (iface & 0x1f) != 0x0c) {
        i2s_channel_disable(tx);
        return ESP_FAIL;
    }
    ESP_LOGI("audio", "ES8311 SDP register=0x%02x (16-bit Philips)", iface);
    if (codec->get_reg(codec, 0x44, &iface) || iface != 0x08) {
        i2s_channel_disable(tx);
        return ESP_FAIL;
    }
    ESP_LOGI("audio", "Microphone ADC enabled; reg44=0x%02x (DAC reference disabled)", iface);
    ret = i2s_channel_disable(tx);
    if (ret != ESP_OK) return ret;
    atomic_store(&overflows, 0); /* Exclude codec configuration before streaming. */
    atomic_store(&completions, 0);
    /* 440/48000 = 11/1200, exact repeating table; no trig in RT loop. */
    for (unsigned i = 0; i < 1200; ++i) wave[i] = sinf(6.28318530718f * i / 1200.0f);
    initialized = true;
    return ESP_OK;
}

static void audio_task(void *arg)
{
    (void)arg;
    unsigned phase = 0, zero_blocks = 0;
    float gain = 0;
    bool active = false, pa_on = false;
    int64_t last_service = 0;
    for (;;) {
        unsigned command = atomic_load(&requested);
        bool want_run = (command & 0x10000) != 0;
        if (!active) {
            if (!want_run) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
            memset(pcm, 0, sizeof(pcm));
            for (unsigned i = 0; i < 6; ++i) {
                size_t loaded;
                if (i2s_channel_preload_data(tx, pcm, sizeof(pcm), &loaded) != ESP_OK || loaded != sizeof(pcm)) goto fault;
            }
            if (i2s_channel_enable(tx) != ESP_OK) goto fault;
            active = true; atomic_store(&running, true);
            last_service = 0; zero_blocks = 0;
        }
        float target = want_run ? (command & 0xffff) / 1000.0f : 0;
        /* Raise PA while DMA contains silence, then ramp on a later block. */
        if (!pa_on && target > 0) {
            if (gpio_set_level(pa, 1) != ESP_OK) goto fault;
            pa_on = true;
            zero_blocks = 0;
            target = 0;
        }
        int64_t start = esp_timer_get_time();
        for (unsigned i = 0; i < AUDIO_FRAMES; ++i) {
            float step = 0.00001f; /* <= 105ms ramp to maximum allowed gain */
            if (gain < target) gain = fminf(gain + step, target);
            else if (gain > target) gain = fmaxf(gain - step, target);
            left[i] = right[i] = wave[phase];
            phase = (phase + 11) % 1200;
            float out = (left[i] + right[i]) * 0.5f * gain;
            out = fmaxf(-1.0f, fminf(1.0f, out));
            pcm[i] = (int16_t)(out * 32767.0f);
        }
        unsigned elapsed = (unsigned)(esp_timer_get_time() - start);
        if (elapsed > atomic_load(&render_max)) atomic_store(&render_max, elapsed);
        if (elapsed >= 2667) atomic_fetch_add(&deadlines, 1);
        size_t written = 0;
        esp_err_t result = i2s_channel_write(tx, pcm, sizeof(pcm), &written, 20);
        if (result != ESP_OK) { atomic_fetch_add(&errors, 1); goto fault; }
        if (written != sizeof(pcm)) { atomic_fetch_add(&shorts, 1); goto fault; }
        int64_t now = esp_timer_get_time();
        if (last_service && now - last_service > 16000) atomic_fetch_add(&gaps, 1);
        last_service = now;
        atomic_fetch_add(&blocks, 1);
        if (gain == 0 && target == 0) ++zero_blocks; else zero_blocks = 0;
        /* Flush all six DMA descriptors before PA off or channel stop. */
        if (zero_blocks >= 8) {
            gpio_set_level(pa, 0); pa_on = false;
            if (!want_run) {
                if (i2s_channel_disable(tx) != ESP_OK) goto fault;
                active = false; atomic_store(&running, false);
            }
        }
    }
fault:
    gpio_set_level(pa, 0);
    if (active) i2s_channel_disable(tx);
    atomic_store(&failed, true); atomic_store(&running, false);
    vTaskDelete(NULL);
}

esp_err_t audio_launch(void)
{
    if (!initialized) return ESP_ERR_INVALID_STATE;
    atomic_store(&requested, 0x10000); /* Boot runs silence, requires tone command. */
    return xTaskCreateStaticPinnedToCore(audio_task, "audio", sizeof(task_stack), NULL,
        20, task_stack, &task_state, 1) ? ESP_OK : ESP_ERR_NO_MEM;
}
void audio_control(bool run, unsigned gain)
{
    if (gain > 50) gain = 50;
    atomic_store(&requested, (run ? 0x10000 : 0) | gain);
}
audio_stats_t audio_stats(void)
{
    return (audio_stats_t){
        .blocks = atomic_load(&blocks), .render_max_us = atomic_load(&render_max),
        .render_deadlines = atomic_load(&deadlines), .write_errors = atomic_load(&errors),
        .short_writes = atomic_load(&shorts), .tx_queue_overflows = atomic_load(&overflows),
        .service_gaps = atomic_load(&gaps), .dma_completions = atomic_load(&completions),
        .running = atomic_load(&running), .failed = atomic_load(&failed),
    };
}

esp_err_t audio_capture(int16_t *samples, unsigned count)
{
    if (!samples || !count || !atomic_load(&running) || atomic_load(&failed))
        return ESP_ERR_INVALID_STATE;
    atomic_store(&rx_overflows, 0);
    esp_err_t ret = i2s_channel_enable(rx);
    if (ret != ESP_OK) return ret;
    for (unsigned pos = 0; pos < count;) {
        unsigned n = count - pos;
        if (n > AUDIO_FRAMES) n = AUDIO_FRAMES;
        size_t got = 0;
        ret = i2s_channel_read(rx, samples + pos, n * sizeof(int16_t), &got, 20);
        if (ret != ESP_OK || got != n * sizeof(int16_t)) { ret = ESP_FAIL; break; }
        pos += n;
    }
    esp_err_t stop = i2s_channel_disable(rx);
    if (ret != ESP_OK) return ret;
    if (stop != ESP_OK) return stop;
    return atomic_load(&rx_overflows) ? ESP_FAIL : ESP_OK;
}
