#include "audio_hal.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_hal";

static i2s_chan_handle_t s_tx_chan = NULL;
static bool s_initialized = false;

esp_err_t audio_hal_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "audio_hal already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing MAX98357A I2S driver (BCLK=%d, WS=%d, DOUT=%d, %d Hz stereo 16-bit)",
             AUDIO_I2S_BCLK_PIN, AUDIO_I2S_WS_PIN, AUDIO_I2S_DOUT_PIN, AUDIO_SAMPLE_RATE);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 128;

    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = AUDIO_I2S_BCLK_PIN,
            .ws   = AUDIO_I2S_WS_PIN,
            .dout = AUDIO_I2S_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;

    ret = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return ret;
    }

    ret = i2s_channel_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX channel: %s", esp_err_to_name(ret));
        i2s_del_channel(s_tx_chan);
        s_tx_chan = NULL;
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "MAX98357A I2S driver initialized and channel enabled successfully");
    return ESP_OK;
}

esp_err_t audio_hal_write(const void *src, size_t size, size_t *bytes_written, TickType_t timeout)
{
    if (!s_initialized || !s_tx_chan) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!src || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t timeout_ms = (timeout == portMAX_DELAY) ? portMAX_DELAY : pdTICKS_TO_MS(timeout);
    return i2s_channel_write(s_tx_chan, src, size, bytes_written, timeout_ms);
}

bool audio_hal_is_initialized(void)
{
    return s_initialized;
}
