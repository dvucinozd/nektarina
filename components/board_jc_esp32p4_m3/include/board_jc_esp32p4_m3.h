#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Documentary profile, not runtime detection. See docs/M0_HARDWARE_BSP_AUDIT.md.
 * All DOUT/DIN directions are relative to ESP32-P4.
 * A 7-bit address must be converted if a codec library expects 8-bit notation.
 */
typedef struct {
    gpio_num_t sda, scl;
    uint8_t codec_address_7bit;
    gpio_num_t mclk, bclk, ws, dout, din, pa_enable;
    unsigned output_channels;
} nektar_board_audio_t;

typedef struct {
    int host_slot;
    gpio_num_t clk, cmd, data[4];
    int power_ldo_channel;
    gpio_num_t card_detect;
} nektar_board_sd_t;

typedef struct {
    int host_slot;
    gpio_num_t clk, cmd, data[4], chip_enable;
    bool module_internal_wiring_verified;
} nektar_board_c6_t;

typedef struct {
    const char *name;
    bool physical_revision_verified;
    nektar_board_audio_t audio;
    nektar_board_sd_t sd;
    nektar_board_c6_t c6;
    gpio_num_t boot_button;
    gpio_num_t usb_vbus_enable; /* NC: no controllable switch identified. */
} nektar_board_t;

/* Immutable profile; does not configure GPIO, clocks, power or drivers. */
const nektar_board_t *nektar_board_get(void);

#ifdef __cplusplus
}
#endif
