#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    SYNTH_MODE_VIRTUAL_ANALOG = 0,  /**< 80s Subtractive Virtual Analog (Saw/Pulse + Resonant Biquad + ADSR) */
    SYNTH_MODE_SOUNDFONT      = 1,  /**< TinySoundFont SF2 Player (PSRAM allocated) */
} synth_engine_mode_t;

typedef struct {
    uint32_t audio_write_errors;
    uint32_t audio_short_writes;
    uint8_t active_voices;
    UBaseType_t task_stack_high_water;
} synth_engine_status_t;

/**
 * @brief Inicijalizacija sintetizatorskog podsustava
 *
 * @param midi_in_queue Red čekanja s kojeg se čitaju midi_message_t poruke
 * @return ESP_OK pri uspjehu
 */
esp_err_t synth_engine_init(QueueHandle_t midi_in_queue);

/**
 * @brief Pokretanje FreeRTOS synth_task zadatka na Core 1
 *
 * @return ESP_OK pri uspjehu
 */
esp_err_t synth_engine_start(void);

/**
 * @brief Promjena aktivnog načina rada sintetizatora (VA ili SoundFont)
 */
void synth_engine_set_mode(synth_engine_mode_t mode);

/**
 * @brief Dohvat aktivnog načina rada sintetizatora
 */
synth_engine_mode_t synth_engine_get_mode(void);

/**
 * @brief Direktno okidanje Note On događaja
 */
void synth_engine_note_on(uint8_t note, uint8_t velocity);

/**
 * @brief Direktno okidanje Note Off događaja
 */
void synth_engine_note_off(uint8_t note);

/**
 * @brief Postavljanje Pitch Bend vrijednosti (-8192 do +8191)
 */
void synth_engine_pitch_bend(int16_t bend);

/**
 * @brief Obrada Control Change poruke (CC)
 */
void synth_engine_control_change(uint8_t cc, uint8_t val);

/**
 * @brief Učitavanje SoundFont2 (.sf2) banke u PSRAM za Engine A
 *
 * @param sf2_data Pokazivač na SF2 podatke u memoriji
 * @param size Veličina u bajtovima
 * @return ESP_OK pri uspjehu
 */
esp_err_t synth_engine_load_soundfont(const void *sf2_data, size_t size);

/**
 * @brief Dohvat trenutne telemetrije synth taska.
 */
esp_err_t synth_engine_get_status(synth_engine_status_t *status);
