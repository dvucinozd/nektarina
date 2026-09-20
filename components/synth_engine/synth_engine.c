#include "synth_engine.h"
#include "audio_hal.h"
#include "usb_midi_host.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <math.h>
#include <string.h>

#define TSF_MALLOC(sz)      heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define TSF_REALLOC(p, sz)  heap_caps_realloc(p, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define TSF_FREE(p)         free(p)
#define TSF_IMPLEMENTATION
#include "tsf.h"

static const char *TAG = "synth_engine";

#define SYNTH_TASK_STACK_SIZE   8192
#define SYNTH_TASK_PRIORITY     19
#define FRAMES_PER_BLOCK        128
#define MAX_VOICES              16
#define SAMPLE_RATE             ((float)AUDIO_SAMPLE_RATE)
#define TWO_PI                  6.283185307179586f
#define RELEASE_TIME_SECONDS    0.350f
#define FILTER_MAX_RATIO        0.45f

/* ADSR Stages */
typedef enum {
    ENV_IDLE = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE,
} env_stage_t;

/* Single polyphonic voice */
typedef struct {
    bool active;
    uint8_t note;
    float velocity;
    float phase;
    float base_freq;
    float target_freq;
    
    /* ADSR envelope state */
    env_stage_t env_stage;
    float env_level;
    float attack_step;
    float decay_step;
    float sustain_level;
    float release_step;

    /* Resonant State Variable Filter (SVF) state */
    float svf_low;
    float svf_band;

    uint32_t age; /* for voice stealing */
} synth_voice_t;

static synth_voice_t s_voices[MAX_VOICES];
static QueueHandle_t s_midi_queue = NULL;
static synth_engine_mode_t s_mode = SYNTH_MODE_VIRTUAL_ANALOG;
static tsf *s_tsf = NULL;
static TaskHandle_t s_synth_task_handle = NULL;

/* Master Controls */
static float s_pitch_bend_ratio = 1.0f;
static float s_mod_wheel = 0.0f;          /* CC 1 */
static float s_base_cutoff = 3000.0f;     /* CC 74 (Hz) */
static float s_resonance = 0.5f;          /* CC 71 (0.05 to 0.95) */
static float s_master_volume = 0.8f;      /* CC 7 */
static uint32_t s_voice_age_counter = 0;
static volatile uint32_t s_audio_write_errors = 0;
static volatile uint32_t s_audio_short_writes = 0;
static volatile uint8_t s_active_voice_count = 0;
static volatile UBaseType_t s_task_stack_high_water = 0;
static TickType_t s_last_audio_error_log_tick = 0;

static inline float fast_sin_filter(float x)
{
    /* Fifth-order approximation; x is restricted to [0, 0.45*pi]. */
    float x2 = x * x;
    return x * (1.0f - x2 * (1.0f / 6.0f - x2 * (1.0f / 120.0f)));
}

/* PolyBLEP anti-aliasing residual */
static inline float polyblep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

static float midi_note_to_freq(uint8_t note)
{
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

void synth_engine_set_mode(synth_engine_mode_t mode)
{
    if (mode == SYNTH_MODE_SOUNDFONT && !s_tsf) {
        ESP_LOGW(TAG, "SoundFont mode rejected: no SF2 bank is loaded");
        return;
    }
    s_mode = mode;
    ESP_LOGI(TAG, "Synth mode set to: %s", (mode == SYNTH_MODE_VIRTUAL_ANALOG) ? "Virtual Analog" : "SoundFont (SF2)");
}

synth_engine_mode_t synth_engine_get_mode(void)
{
    return s_mode;
}

void synth_engine_note_on(uint8_t note, uint8_t velocity)
{
    if (note > 127) note = 127;
    if (velocity > 127) velocity = 127;
    if (velocity == 0) {
        synth_engine_note_off(note);
        return;
    }

    ESP_LOGI("MIDI", "Note On: Note=%d, Velocity=%d", note, velocity);

    if (s_mode == SYNTH_MODE_SOUNDFONT && s_tsf) {
        tsf_note_on(s_tsf, 0, note, (float)velocity / 127.0f);
        return;
    }

    /* Engine B: Virtual Analog voice allocation */
    int free_idx = -1;
    uint32_t oldest_age = 0;
    int oldest_idx = -1;

    for (int i = 0; i < MAX_VOICES; i++) {
        if (!s_voices[i].active || s_voices[i].env_stage == ENV_IDLE) {
            free_idx = i;
            break;
        }
        if (s_voices[i].note == note) {
            /* Retrigger same note */
            free_idx = i;
            break;
        }
        uint32_t age = s_voice_age_counter - s_voices[i].age;
        if (age > oldest_age) {
            oldest_age = age;
            oldest_idx = i;
        }
    }

    if (free_idx < 0) {
        free_idx = (oldest_idx >= 0) ? oldest_idx : 0;
    }

    synth_voice_t *v = &s_voices[free_idx];
    v->active = true;
    v->note = note;
    v->velocity = (float)velocity / 127.0f;
    v->base_freq = midi_note_to_freq(note);
    v->phase = 0.0f;
    v->age = ++s_voice_age_counter;

    /* ADSR envelope parameters (seconds -> per-sample increment) */
    float attack_time = 0.008f;               /* 8 ms fast attack */
    float decay_time = 0.250f;                /* 250 ms decay */
    v->sustain_level = 0.65f;                 /* 65% sustain level */
    v->attack_step = 1.0f / (attack_time * SAMPLE_RATE);
    v->decay_step = (1.0f - v->sustain_level) / (decay_time * SAMPLE_RATE);
    v->release_step = v->sustain_level / (RELEASE_TIME_SECONDS * SAMPLE_RATE);

    v->env_stage = ENV_ATTACK;
    v->env_level = 0.0f;
    v->svf_low = 0.0f;
    v->svf_band = 0.0f;
}

void synth_engine_note_off(uint8_t note)
{
    ESP_LOGI("MIDI", "Note Off: Note=%d", note);

    if (s_mode == SYNTH_MODE_SOUNDFONT && s_tsf) {
        tsf_note_off(s_tsf, 0, note);
        return;
    }

    for (int i = 0; i < MAX_VOICES; i++) {
        if (s_voices[i].active && s_voices[i].note == note) {
            if (s_voices[i].env_stage != ENV_RELEASE) {
                s_voices[i].release_step = s_voices[i].env_level /
                                           (RELEASE_TIME_SECONDS * SAMPLE_RATE);
                if (s_voices[i].release_step <= 0.0f) {
                    s_voices[i].release_step = 1.0f / (RELEASE_TIME_SECONDS * SAMPLE_RATE);
                }
            }
            s_voices[i].env_stage = ENV_RELEASE;
        }
    }
}

void synth_engine_pitch_bend(int16_t bend)
{
    if (bend < -8192) bend = -8192;
    if (bend > 8191) bend = 8191;
    /* Bend range: +/- 2 semitones */
    float semitones = ((float)bend / 8192.0f) * 2.0f;
    s_pitch_bend_ratio = powf(2.0f, semitones / 12.0f);

    if (s_tsf) {
        tsf_channel_set_pitchwheel(s_tsf, 0, bend + 8192);
    }
}

void synth_engine_control_change(uint8_t cc, uint8_t val)
{
    if (cc > 127) cc = 127;
    if (val > 127) val = 127;
    float norm = (float)val / 127.0f;
    switch (cc) {
    case 1: /* Mod Wheel */
        s_mod_wheel = norm;
        break;
    case 7: /* Master Volume */
        s_master_volume = norm;
        break;
    case 71: /* Filter Resonance */
        s_resonance = 0.05f + norm * 0.90f;
        break;
    case 74: /* Filter Cutoff / Brightness */
        s_base_cutoff = 200.0f + norm * norm * 14000.0f;
        break;
    case 120: /* All Sound Off */
    case 123: /* All Notes Off */
        for (int i = 0; i < MAX_VOICES; i++) {
            s_voices[i].active = false;
            s_voices[i].env_stage = ENV_IDLE;
            s_voices[i].env_level = 0.0f;
        }
        if (s_tsf) {
            tsf_note_off_all(s_tsf);
        }
        break;
    default:
        break;
    }
}

esp_err_t synth_engine_load_soundfont(const void *sf2_data, size_t size)
{
    if (!sf2_data || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_synth_task_handle != NULL) {
        ESP_LOGE(TAG, "SoundFont must be loaded before synth_engine_start()");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Loading SoundFont (%u bytes) into PSRAM", (unsigned)size);
    tsf *new_tsf = tsf_load_memory(sf2_data, (int)size);
    if (!new_tsf) {
        ESP_LOGE(TAG, "Failed to parse SoundFont2 data");
        return ESP_FAIL;
    }

    tsf_set_output(new_tsf, TSF_STEREO_INTERLEAVED, (int)SAMPLE_RATE, 0.0f);
    if (s_tsf) {
        tsf_close(s_tsf);
    }
    s_tsf = new_tsf;
    ESP_LOGI(TAG, "SoundFont loaded successfully into PSRAM, presets=%d", tsf_get_presetcount(s_tsf));
    s_mode = SYNTH_MODE_SOUNDFONT;
    return ESP_OK;
}

static void synth_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "synth_task started on core %d, allocating DMA I2S buffer in internal RAM", xPortGetCoreID());

    /* Strict requirement: DMA buffer strictly in internal DMA-capable RAM */
    size_t dma_buf_size = FRAMES_PER_BLOCK * 2 * sizeof(int16_t);
    int16_t *dma_buf = (int16_t *)heap_caps_malloc(dma_buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!dma_buf) {
        ESP_LOGE(TAG, "FATAL: Failed to allocate I2S DMA buffer in internal RAM!");
        s_synth_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    float mix_left[FRAMES_PER_BLOCK];
    float mix_right[FRAMES_PER_BLOCK];

    while (1) {
        /* 1. Process all pending MIDI events in the queue */
        midi_message_t msg;
        while (s_midi_queue && xQueueReceive(s_midi_queue, &msg, 0) == pdTRUE) {
            switch (msg.type) {
            case MIDI_EVENT_NOTE_ON:
                synth_engine_note_on(msg.data1, msg.data2);
                break;
            case MIDI_EVENT_NOTE_OFF:
                synth_engine_note_off(msg.data1);
                break;
            case MIDI_EVENT_PITCH_BEND:
                synth_engine_pitch_bend(msg.pitch_bend);
                break;
            case MIDI_EVENT_CC:
                synth_engine_control_change(msg.data1, msg.data2);
                break;
            default:
                break;
            }
        }

        uint8_t active_voices = 0;
        for (int i = 0; i < MAX_VOICES; i++) {
            if (s_voices[i].active && s_voices[i].env_stage != ENV_IDLE) {
                active_voices++;
            }
        }
        s_active_voice_count = active_voices;

        /* 2. Render Audio Block */
        if (s_mode == SYNTH_MODE_SOUNDFONT && s_tsf) {
            /* Engine A: TinySoundFont SF2 playback */
            tsf_render_short(s_tsf, dma_buf, FRAMES_PER_BLOCK, 0);
        } else {
            /* Engine B: 80s Virtual Analog Subtractive Synth */
            memset(mix_left, 0, sizeof(mix_left));
            memset(mix_right, 0, sizeof(mix_right));

            for (int v_idx = 0; v_idx < MAX_VOICES; v_idx++) {
                synth_voice_t *v = &s_voices[v_idx];
                if (!v->active || v->env_stage == ENV_IDLE) continue;

                float freq = v->base_freq * s_pitch_bend_ratio;
                float dt = freq / SAMPLE_RATE;
                if (dt >= 0.49f) dt = 0.49f;

                for (int f = 0; f < FRAMES_PER_BLOCK; f++) {
                    /* ADSR Envelope state machine */
                    switch (v->env_stage) {
                    case ENV_ATTACK:
                        v->env_level += v->attack_step;
                        if (v->env_level >= 1.0f) {
                            v->env_level = 1.0f;
                            v->env_stage = ENV_DECAY;
                        }
                        break;
                    case ENV_DECAY:
                        v->env_level -= v->decay_step;
                        if (v->env_level <= v->sustain_level) {
                            v->env_level = v->sustain_level;
                            v->env_stage = ENV_SUSTAIN;
                        }
                        break;
                    case ENV_SUSTAIN:
                        /* Holds at sustain level */
                        break;
                    case ENV_RELEASE:
                        v->env_level -= v->release_step;
                        if (v->env_level <= 0.0f) {
                            v->env_level = 0.0f;
                            v->env_stage = ENV_IDLE;
                            v->active = false;
                        }
                        break;
                    default:
                        break;
                    }

                    if (v->env_level <= 0.0f && v->env_stage == ENV_IDLE) {
                        break;
                    }

                    /* Dual Oscillator: Sawtooth (65%) + Pulse (35%) */
                    float t = v->phase;
                    /* Raw saw: 2*t - 1 */
                    float saw = (2.0f * t - 1.0f) - polyblep(t, dt);

                    /* Square/Pulse with 50% duty + PolyBLEP */
                    float square = (t < 0.5f) ? 1.0f : -1.0f;
                    square += polyblep(t, dt);
                    float t_half = t + 0.5f;
                    if (t_half >= 1.0f) t_half -= 1.0f;
                    square -= polyblep(t_half, dt);

                    float raw_sample = 0.65f * saw + 0.35f * square;

                    /* Advance phase */
                    v->phase += dt;
                    if (v->phase >= 1.0f) {
                        v->phase -= 1.0f;
                    }

                    /* State Variable Resonant Filter (SVF) */
                    /* Dynamic cutoff: base + envelope modulation + mod wheel */
                    float cutoff = s_base_cutoff + (v->env_level * 3500.0f) + (s_mod_wheel * 4000.0f);
                    const float max_cutoff = SAMPLE_RATE * FILTER_MAX_RATIO;
                    if (cutoff > max_cutoff) cutoff = max_cutoff;
                    if (cutoff < 100.0f) cutoff = 100.0f;

                    float q_damp = 1.0f - (s_resonance * 0.90f);
                    float angle = (float)M_PI * cutoff / SAMPLE_RATE;
                    float f_coeff = 2.0f * fast_sin_filter(angle);
                    float stability_limit = sqrtf(4.0f - q_damp * q_damp) - q_damp;
                    stability_limit *= 0.95f;
                    if (f_coeff > stability_limit) f_coeff = stability_limit;

                    v->svf_low += f_coeff * v->svf_band;
                    float svf_high = raw_sample - v->svf_low - (q_damp * v->svf_band);
                    v->svf_band += f_coeff * svf_high;

                    float filtered = v->svf_low;
                    float voice_out = filtered * v->env_level * v->velocity;

                    mix_left[f] += voice_out;
                    mix_right[f] += voice_out;
                }
            }

            /* Convert mixed float audio to 16-bit signed stereo with hard limiting. */
            for (int i = 0; i < FRAMES_PER_BLOCK; i++) {
                float l = mix_left[i] * s_master_volume * 0.85f;
                float r = mix_right[i] * s_master_volume * 0.85f;

                /* Soft clipping using fast tanh approximation */
                if (l > 1.0f) l = 1.0f; else if (l < -1.0f) l = -1.0f;
                if (r > 1.0f) r = 1.0f; else if (r < -1.0f) r = -1.0f;

                dma_buf[i * 2 + 0] = (int16_t)(l * 32767.0f);
                dma_buf[i * 2 + 1] = (int16_t)(r * 32767.0f);
            }
        }

        /* 3. Output to MAX98357A via audio_hal_write */
        size_t written = 0;
        esp_err_t err = audio_hal_write(dma_buf, dma_buf_size, &written, portMAX_DELAY);
        if (err != ESP_OK) {
            s_audio_write_errors++;
        } else if (written != dma_buf_size) {
            s_audio_short_writes++;
        }
        if (err != ESP_OK || written != dma_buf_size) {
            TickType_t now = xTaskGetTickCount();
            if ((now - s_last_audio_error_log_tick) >= pdMS_TO_TICKS(1000)) {
                s_last_audio_error_log_tick = now;
                ESP_LOGE(TAG, "I2S write failed: %s (%u/%u bytes), errors=%lu short=%lu",
                         esp_err_to_name(err), (unsigned)written, (unsigned)dma_buf_size,
                         (unsigned long)s_audio_write_errors,
                         (unsigned long)s_audio_short_writes);
            }
        }
        s_task_stack_high_water = uxTaskGetStackHighWaterMark(NULL);
    }
}

esp_err_t synth_engine_init(QueueHandle_t midi_in_queue)
{
    if (!midi_in_queue) {
        return ESP_ERR_INVALID_ARG;
    }
    s_midi_queue = midi_in_queue;
    memset(s_voices, 0, sizeof(s_voices));
    s_mode = SYNTH_MODE_VIRTUAL_ANALOG;
    s_audio_write_errors = 0;
    s_audio_short_writes = 0;
    s_active_voice_count = 0;
    s_task_stack_high_water = 0;
    s_last_audio_error_log_tick = 0;
    ESP_LOGI(TAG, "synth_engine initialized with 16 polyphonic VA voices");
    return ESP_OK;
}

esp_err_t synth_engine_start(void)
{
    if (s_synth_task_handle != NULL) {
        return ESP_OK;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(
        synth_task, "synth_task", SYNTH_TASK_STACK_SIZE, NULL, SYNTH_TASK_PRIORITY, &s_synth_task_handle, 1);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create synth_task on core 1");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "synth_task started successfully on Core 1 (Priority %d)", SYNTH_TASK_PRIORITY);
    return ESP_OK;
}

esp_err_t synth_engine_get_status(synth_engine_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    status->audio_write_errors = s_audio_write_errors;
    status->audio_short_writes = s_audio_short_writes;
    status->active_voices = s_active_voice_count;
    status->task_stack_high_water = s_task_stack_high_water;
    return ESP_OK;
}
