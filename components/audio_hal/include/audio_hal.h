#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "soc/gpio_num.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_SAMPLE_RATE    44100
#define AUDIO_I2S_BCLK_PIN   GPIO_NUM_16
#define AUDIO_I2S_WS_PIN     GPIO_NUM_17
#define AUDIO_I2S_DOUT_PIN   GPIO_NUM_18

/**
 * @brief Inicijalizacija I2S master kanala za MAX98357A mono pojačalo
 * Format: 44.1 kHz, 16-bit Philips I2S stereo, BCLK=16, WS=17, DOUT=18.
 *
 * @return ESP_OK pri uspjehu, odgovarajući esp_err_t kod pri grešci.
 */
esp_err_t audio_hal_init(void);

/**
 * @brief Slanje PCM audio uzoraka na I2S sabirnicu
 *
 * @param src Pokazivač na međuspremnik s podacima
 * @param size Veličina podataka u bajtovima
 * @param bytes_written Pokazivač na varijablu u koju se sprema broj zapisanih bajtova
 * @param timeout Maksimalno vrijeme čekanja (TickType_t)
 * @return ESP_OK pri uspjehu, odgovarajući esp_err_t kod pri grešci.
 */
esp_err_t audio_hal_write(const void *src, size_t size, size_t *bytes_written, TickType_t timeout);

/**
 * @brief Provjera je li audio podsustav inicijaliziran
 */
bool audio_hal_is_initialized(void);
