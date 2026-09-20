#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MIDI_EVENT_NOTE_OFF    = 0x80,
    MIDI_EVENT_NOTE_ON     = 0x90,
    MIDI_EVENT_CC          = 0xB0,
    MIDI_EVENT_PROGRAM     = 0xC0,
    MIDI_EVENT_PITCH_BEND  = 0xE0,
} midi_event_type_t;

typedef struct {
    midi_event_type_t type;
    uint8_t channel;
    uint8_t data1;      /* Note number (0-127) ili Controller number (0-127) */
    uint8_t data2;      /* Velocity (0-127) ili Controller value (0-127) */
    int16_t pitch_bend; /* -8192 do +8191 (centar na 0) */
} midi_message_t;

/**
 * @brief Callback tip za dojavu promjene stanja veze s USB MIDI uređajem
 */
typedef void (*usb_midi_connection_cb_t)(bool connected, void *user_ctx);

/**
 * @brief Inicijalizacija USB Host biblioteke, registracija MIDI klijenta i pokretanje taskova.
 *
 * @param midi_queue Red čekanja za prosljeđivanje strukturiranih midi_message_t poruka
 * @param conn_cb Opcionalni callback za spajanje/odspajanje (može biti NULL)
 * @param user_ctx Korisnički pokazivač za callback
 * @return ESP_OK pri uspješnoj inicijalizaciji
 */
esp_err_t usb_midi_host_init(QueueHandle_t midi_queue, usb_midi_connection_cb_t conn_cb, void *user_ctx);

/**
 * @brief Provjera je li USB MIDI uređaj spojen i aktivan
 */
bool usb_midi_host_is_connected(void);

/**
 * @brief Broj MIDI poruka odbačenih zbog punog reda od pokretanja drivera.
 */
uint32_t usb_midi_host_get_dropped_messages(void);
