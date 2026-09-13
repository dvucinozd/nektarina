#include "board_jc_esp32p4_m3.h"
#include "esp_log.h"

void app_main(void)
{
    const nektar_board_t *board = nektar_board_get();
    ESP_LOGI("M0", "%s; physical revision pending", board->name);
    ESP_LOGI("M0", "Codec I2C SDA=%d SCL=%d addr=0x%02x; PA=%d",
             board->audio.sda, board->audio.scl,
             board->audio.codec_address_7bit, board->audio.pa_enable);
    ESP_LOGI("M0", "Descriptor only: no peripheral initialization");
}
