#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "soc/gpio_num.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_SAMPLE_RATE    44100
#define AUDIO_I2S_BCLK_PIN   GPIO_NUM_45
#define AUDIO_I2S_WS_PIN     GPIO_NUM_3
#define AUDIO_I2S_DOUT_PIN   GPIO_NUM_47
#define AUDIO_SD_MODE_PIN    GPIO_NUM_14

/**
 * @brief Inicijalizacija I2S master kanala za MAX98357A mono pojačalo
 * Format: 44.1 kHz, 16-bit Philips I2S stereo, BCLK=45, WS=3, DOUT=47.
 * SD_MODE is driven on GPIO14. MAX98357A GAIN is not connected to the MCU.
 *
 * @return ESP_OK pri uspjehu, odgovarajući esp_err_t kod pri grešci.
 */
esp_err_t audio_hal_init(void);

/**
 * @brief Zaustavljanje I2S kanala i postavljanje MAX98357A u shutdown.
 */
esp_err_t audio_hal_deinit(void);

/**
 * @brief Uključivanje ili isključivanje MAX98357A izlaza preko SD_MODE pina.
 */
esp_err_t audio_hal_set_muted(bool muted);

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
