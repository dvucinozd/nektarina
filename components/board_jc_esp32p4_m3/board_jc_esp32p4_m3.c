#include "board_jc_esp32p4_m3.h"
#include "sdkconfig.h"

#if !CONFIG_IDF_TARGET_ESP32P4
#error "This board profile requires ESP32-P4"
#endif

/* Schematics sheets 3/4 and the supplied mp3_player example agree. */
static const nektar_board_t board = {
    .name = "Guition JC-ESP32P4-M3-DEV (supplied DEV0 documentation)",
    .physical_revision_verified = false,
    .audio = {
        .sda = GPIO_NUM_7, .scl = GPIO_NUM_8,
        .codec_address_7bit = 0x18,
        .mclk = GPIO_NUM_13, .bclk = GPIO_NUM_12, .ws = GPIO_NUM_10,
        .dout = GPIO_NUM_9, .din = GPIO_NUM_48, .pa_enable = GPIO_NUM_11,
        .output_channels = 1,
    },
    .sd = {
        .host_slot = 0,
        .clk = GPIO_NUM_43, .cmd = GPIO_NUM_44,
        .data = {GPIO_NUM_39, GPIO_NUM_40, GPIO_NUM_41, GPIO_NUM_42},
        .power_ldo_channel = 4, .card_detect = GPIO_NUM_NC,
    },
    /* Wi-Fi demo sdkconfig only: reserve pins, do not initialize yet.
     * GPIO54 -> C6_CHIP_PU is also visible on schematic sheet 4.
     * ESP-Hosted's RESET_ACTIVE_HIGH label is not interpreted here as EN polarity.
     */
    .c6 = {
        .host_slot = 1,
        .clk = GPIO_NUM_18, .cmd = GPIO_NUM_19,
        .data = {GPIO_NUM_14, GPIO_NUM_15, GPIO_NUM_16, GPIO_NUM_17},
        .chip_enable = GPIO_NUM_54,
        .module_internal_wiring_verified = false,
    },
    .boot_button = GPIO_NUM_35,
    .usb_vbus_enable = GPIO_NUM_NC,
};

const nektar_board_t *nektar_board_get(void)
{
    return &board;
}
