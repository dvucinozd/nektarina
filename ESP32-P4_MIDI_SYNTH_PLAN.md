# ESP32-P4 USB-MIDI Synthesizer
## Koncept, arhitektura i implementacijski plan

**Razvojni status (2026-09-13):** M0 audit/BSP i M1 audio HAL su implementirani.
Ton, slušni prijelazi i mikrofon potvrđeni su. Soak 601.970 s prolazi na 0.1.0-m1;
aktualni 0.1.1-m1-mic ima zaseban kratki test. M2 nije započet.
Detalji: [trenutno stanje](docs/STATE_SUMMARY.md) i [M1 dokazi](docs/M1_AUDIO_TEST.md).

**Ciljna platforma:** Guition JC-ESP32P4-M3-DEV
**MIDI kontroler:** Nektar Impact GX49
**Framework:** ESP-IDF 6 (nalazi se na C:\Espressif\v6.0.2\esp-idf )
**Mreža:** ESP32-C6 preko ESP-Hosted / esp_wifi_remote
**Audio:** ESP32-P4 → I2S → ES8311 → onboard speaker izlaz / budući line-out
**USB Host:** postojeće korisnikovo rješenje, integrira se kao gotov podsustav

---

# 1. Cilj projekta

Cilj je izraditi samostalni embedded MIDI synthesizer temeljen na ESP32-P4.

Nektar Impact GX49 spaja se putem USB-a na ESP32-P4, koji radi kao USB host. MIDI događaji iz klavijature ulaze u real-time synth engine, koji generira audio i šalje ga preko I2S sučelja prema ES8311 audio codecu na JC-ESP32P4-M3-DEV razvojnoj ploči.

Sustav se konfigurira i upravlja preko web sučelja dostupnog putem Wi-Fi mreže. Wi-Fi funkcionalnost osigurava ESP32-C6 koji s ESP32-P4 komunicira preko ESP-Hosted arhitekture.

Osnovni tok sustava:

```text
Nektar Impact GX49
        │
        │ USB MIDI
        ▼
postojeći USB Host podsustav
        │
        ▼
MIDI parser / dispatcher
        │
        ▼
real-time MIDI event queue
        │
        ▼
Synth Engine
        │
        ▼
DSP / Effects
        │
        ▼
48 kHz PCM audio
        │
        ▼
I2S
        │
        ▼
ES8311
        │
        ├── onboard speaker izlaz
        └── budući stereo / line-out izlaz
```

Paralelno:

```text
Browser / mobitel / računalo
        │
        ▼
Wi-Fi
        │
        ▼
ESP32-C6
        │
        ▼
ESP-Hosted
        │
        ▼
ESP32-P4 HTTP/WebSocket server
        │
        ▼
preseti / synth parametri / MIDI mapping / sistemske postavke
```

---

# 2. Zašto je ESP32-P4 dobar izbor

ESP32-P4 je vrlo prikladna platforma za ovakav uređaj zbog:

- dual-core RISC-V procesora visokih performansi
- radnog takta do približno 400 MHz
- FPU podrške
- velikog broja perifernih sklopova
- dva neovisna USB OTG kontrolera
- I2S audio podrške
- mogućnosti korištenja velike vanjske PSRAM memorije
- dobre ESP-IDF RTOS infrastrukture
- DMA podrške
- dovoljno performansi za ozbiljan embedded DSP

Guition JC-ESP32P4-M3-DEV dodatno daje:

- 32 MB PSRAM
- 16 MB flash
- ES8311 audio codec
- onboard audio power amplifier
- speaker konektor
- microSD
- ESP32-C6 za Wi-Fi/Bluetooth povezivost
- više USB priključaka

To znači da za prvi funkcionalni prototip nije potreban dodatni DAC niti dodatni Wi-Fi modul.

---

# 3. Nektar Impact GX49

GX49 će biti primarni fizički kontroler syntha.

Relevantne funkcije:

- USB class-compliant MIDI
- velocity-sensitive tipke
- Pitch Bend
- Modulation Wheel
- assignable potentiometer
- transport i druge kontrolne tipke
- octave up/down
- sustain pedal input
- program / MIDI CC kontrola

Podržani MIDI događaji koje naš sustav treba obraditi:

- Note On
- Note Off
- Velocity
- Control Change
- Pitch Bend
- Program Change
- Sustain pedal
- Channel Pressure / Aftertouch ako ga uređaj šalje
- SysEx kao kasniji dodatak

---

# 4. USB Host

USB host sloj već postoji kao zasebno korisnikovo rješenje.

Zbog toga se USB host više ne tretira kao otvoreni razvojni problem.

Potrebno je napraviti samo stabilno sučelje između postojećeg USB host podsustava i našeg MIDI sloja.

Predloženo API sučelje:

```c
typedef struct {
    uint8_t cable;
    uint8_t cin;
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
} midi_usb_event_t;
```

USB host podsustav ne smije direktno upravljati synth engineom.

Umjesto toga:

```text
USB Host
   │
   ▼
MIDI packet decoder
   │
   ▼
MIDI event queue
   │
   ▼
Synth Engine
```

Time USB sloj ostaje potpuno odvojen od audio real-time logike.

---

# 5. MIDI arhitektura

Predlaže se poseban `midi` component.

Primjer:

```text
components/
    midi/
        midi_parser.c
        midi_router.c
        midi_mapping.c
        midi_queue.c
        include/
```

Interni MIDI event:

```c
typedef enum {
    MIDI_NOTE_ON,
    MIDI_NOTE_OFF,
    MIDI_CC,
    MIDI_PITCH_BEND,
    MIDI_PROGRAM_CHANGE,
    MIDI_CHANNEL_PRESSURE,
    MIDI_SYSEX
} midi_event_type_t;

typedef struct {
    midi_event_type_t type;
    uint8_t channel;
    uint8_t data1;
    uint16_t data2;
    uint32_t timestamp;
} midi_event_t;
```

Audio engine nikada ne bi trebao raditi USB parsing.

MIDI eventi se pretvaraju u jednostavan interni format i predaju real-time event queueu.

---

# 6. Real-time arhitektura

Audio dio mora biti potpuno izoliran od:

- Wi-Fi
- HTTP servera
- WebSocketa
- SD kartice
- filesystema
- USB enumeracije
- logiranja
- dinamičke alokacije memorije

Predložena podjela:

```text
CORE 0
 ├── USB Host
 ├── MIDI input/parser
 ├── ESP-Hosted
 ├── Wi-Fi
 ├── HTTP server
 ├── WebSocket
 ├── SD/storage
 └── UI/system tasks

CORE 1
 ├── MIDI event consumption
 ├── voice allocator
 ├── oscillator rendering
 ├── filters
 ├── envelopes
 ├── effects
 ├── mixer
 └── I2S DMA
```

Ova raspodjela se kasnije mora potvrditi profilingom, ali je dobar početni dizajn.

---

# 7. Audio konfiguracija

Preporučena početna konfiguracija:

```text
sample rate:     48 kHz
processing:      float32 interno
output:          16-bit ili 24-bit PCM prema I2S/codec mogućnostima
audio block:     128 samples
channels:        interno stereo
onboard output:  mono/downmix ako ES8311 put zahtijeva mono
```

Audio block od 128 frameova pri 48 kHz daje:

```text
128 / 48000 = 2.667 ms
```

Kasnije se može testirati:

```text
64 samples = 1.333 ms
```

Cilj ukupne key-to-audio latencije:

**manje od 10 ms**, idealno približno 4–7 ms.

---

# 8. Audio HAL

Preporučuje se napraviti vlastiti audio abstraction layer:

```text
components/
    audio_hal/
        audio_codec.c
        audio_i2s.c
        audio_dma.c
        board_audio.c
```

API primjer:

```c
esp_err_t audio_init(void);
esp_err_t audio_start(void);
esp_err_t audio_stop(void);
void audio_set_volume(float volume);
size_t audio_write(const float *left,
                   const float *right,
                   size_t frames);
```

Synth engine tako ne mora znati ništa o ES8311 niti konkretnim GPIO pinovima.

---

# 9. Board Support Package

Hardkodirane GPIO vrijednosti treba izbjeći.

Napraviti:

```text
components/
    board_jc_esp32p4_m3/
```

U njemu definirati:

- I2S pinove
- ES8311 I2C adresu
- AMP_EN
- microSD pinove
- USB konfiguraciju
- ESP32-C6 SDIO pinove
- power control
- eventualne LED-ice i tipke

Time ostatak projekta postaje neovisan o konkretnoj ploči.

---

# 10. Prvi synth engine

Prvi pravi engine treba biti Virtual Analog synthesizer.

Ne bih počeo SoundFontom jer VA synth omogućuje lakše testiranje:

- latencyja
- MIDI obrade
- polyphonyja
- filtera
- envelopeova
- audio stabilnosti

Minimalni voice:

```text
MIDI note
   │
   ▼
oscillator
   │
   ▼
filter
   │
   ▼
amplitude envelope
   │
   ▼
voice mixer
```

---

# 11. Oscillatori

Prva verzija:

- sine
- triangle
- saw
- square
- pulse
- noise

Nakon toga:

- PolyBLEP saw
- PolyBLEP square
- variable pulse width
- oscillator sync
- sub oscillator

Svaki voice može imati:

```text
OSC1
OSC2
SUB
NOISE
```

Parametri:

- waveform
- octave
- semitone
- fine tune
- level
- phase
- pulse width
- detune

---

# 12. PolyBLEP

Naivni saw/square oscillator proizvodi aliasing.

Zbog toga je preporučljivo relativno rano implementirati PolyBLEP.

Time dobivamo puno kvalitetniji virtual-analog zvuk bez skupog oversamplinga.

---

# 13. Polyphony

Početni cilj:

**16 voiceova**

Nakon profilinga:

- 24
- 32
- 48
- ili više

ovisno o:

- broju oscillatora
- filteru
- efektima
- sample engineu
- CPU opterećenju

Ne treba unaprijed fiksirati maksimalnu polyphony.

---

# 14. Voice allocator

Voice manager mora podržavati:

- free voice allocation
- oldest voice stealing
- lowest amplitude stealing
- retrigger
- legato
- mono mode
- poly mode
- unison
- sustain-held voices

Primjer voice strukture:

```c
typedef struct {
    bool active;
    uint8_t note;
    uint8_t velocity;

    float frequency;

    oscillator_t osc1;
    oscillator_t osc2;

    envelope_t amp_env;
    envelope_t filter_env;

    filter_t filter;

    uint32_t age;
} synth_voice_t;
```

---

# 15. ADSR envelope

Potrebni parametri:

- Attack
- Decay
- Sustain
- Release

Kasnije:

- velocity sensitivity
- envelope curves
- multi-stage envelope

Treba izbjegavati skupe matematičke operacije za svaki sample gdje god je moguće.

---

# 16. Filter

Prvi filter:

- resonant low-pass

Kasnije:

- high-pass
- band-pass
- notch
- ladder style
- multimode filter

Parametri:

- cutoff
- resonance
- key tracking
- velocity modulation
- envelope amount
- LFO amount

---

# 17. LFO

Minimalno:

- sine
- triangle
- saw
- square
- sample & hold

Destinacije:

- pitch
- filter cutoff
- amplitude
- pulse width
- oscillator level
- pan

Kasnije:

- tempo sync
- phase reset
- multiple LFO
- modulation matrix

---

# 18. Modulation Matrix

Dugoročno bi synth trebao imati malu modulation matrix.

Primjeri:

```text
Mod Wheel → LFO Depth
Velocity  → Filter Cutoff
Velocity  → Amp
LFO1      → Pitch
LFO2      → Cutoff
Envelope2 → Pitch
CC74      → Filter
```

To značajno povećava fleksibilnost instrumenta.

---

# 19. Pitch Bend

Pitch bend treba koristiti barem 14-bitnu MIDI vrijednost.

Konfigurabilan range:

```text
±2 semitona
±7
±12
±24
```

Default:

**±2 semitona**

---

# 20. Sustain

CC64 mora biti pravilno implementiran.

Note Off ne gasi voice dok je sustain aktivan.

Tek nakon otpuštanja sustain pedale gase se svi voiceovi označeni kao released.

---

# 21. MIDI Learn

Jedna od važnijih funkcija web sučelja.

Workflow:

1. klik na synth parametar
2. klik na `MIDI LEARN`
3. korisnik pomakne knob na GX49
4. uređaj vidi CC broj
5. mapping se sprema

Primjer:

```text
CC1  → LFO depth
CC7  → master volume
CC21 → filter cutoff
CC22 → resonance
```

Mapping se sprema u preset ili globalnu konfiguraciju.

---

# 22. Preset sustav

Preset treba sadržavati:

```text
oscillator settings
filter settings
envelopes
LFO
modulation matrix
effects
MIDI mappings
polyphony
voice mode
master settings
```

Format tijekom razvoja može biti JSON.

Primjer:

```json
{
  "name": "Warm Pad",
  "osc1": {
    "wave": "saw",
    "level": 0.8
  },
  "filter": {
    "cutoff": 0.42,
    "resonance": 0.18
  }
}
```

Kasnije se može koristiti kompaktni binary format.

---

# 23. Preset bankovi

Struktura:

```text
/presets/
    factory/
    user/
```

Funkcije:

- Load
- Save
- Save As
- Rename
- Delete
- Copy
- Favorite
- Bank selection

MIDI Program Change može prebacivati preset.

---

# 24. SoundFont engine

Nakon stabilizacije VA syntha dodati SF2 engine.

Dobar prvi kandidat je TinySoundFont.

Prednosti:

- malen
- jednostavan za integraciju
- MIT licenca
- nema veliki framework dependency
- može renderirati PCM direktno u naš buffer

Namjene:

- piano
- electric piano
- organ
- strings
- brass
- pads
- GM instrumenti

---

# 25. PSRAM

32 MB PSRAM je jedna od najvažnijih prednosti ploče.

Treba je koristiti za:

- SoundFont/sample memoriju
- delay buffer
- reverb buffer
- wavetable podatke
- veće preset/bank strukture
- file cache

Real-time male strukture i kritični audio bufferi trebaju ostati u brzoj internoj memoriji kad god je moguće.

---

# 26. Optimizirani SoundFont format

Dugoročno je korisno napraviti vlastiti offline konverter:

```text
SF2
 │
 ▼
P4SF converter
 │
 ▼
P4SF
```

P4SF može sadržavati samo:

- odabrane instrumente
- PCM16 sampleove
- sample loop
- root note
- key zones
- velocity zones
- ADSR
- tuning
- pan
- gain

Prednosti:

- manje RAM-a
- manje storagea
- brže učitavanje
- preciznija kontrola
- nema runtime parsiranja velikog SF2 formata

---

# 27. Sample streaming

Za velike sample libraryje:

```text
microSD
   │
   ▼
read-ahead cache
   │
   ▼
PSRAM
   │
   ▼
sample engine
```

Treba koristiti double-buffering ili ring-buffering.

Audio thread nikad ne smije direktno čekati SD card read.

---

# 28. Wavetable engine

Kasnija nadogradnja:

- 256/512/1024/2048 sample waveform
- wavetable interpolation
- wavetable position modulation
- morphing
- custom user tables

To omogućuje modernije synth zvukove.

---

# 29. FM engine

Dugoročna opcija:

- 2-op FM
- 4-op FM
- eventualno 6-op

Nije prioritet prve verzije.

---

# 30. Drum / Sample engine

Dodatni engine:

- one-shot sample
- multisample
- velocity layers
- drum map
- choke groups

Može omogućiti:

- drum kits
- percussion
- FX samples

---

# 31. Effects chain

Predloženi signal path:

```text
Synth voices
     │
     ▼
Mixer
     │
     ▼
EQ
     │
     ▼
Chorus
     │
     ▼
Delay
     │
     ▼
Reverb
     │
     ▼
Limiter
     │
     ▼
I2S
```

Prvi efekti:

- chorus
- delay
- reverb
- 3-band EQ
- limiter

Svaki efekt mora imati bypass.

---

# 32. Chorus

Parametri:

- rate
- depth
- mix
- feedback
- stereo spread

---

# 33. Delay

Parametri:

- time
- feedback
- mix
- low-pass
- tempo sync

Kasnije:

- stereo ping-pong

---

# 34. Reverb

Početno koristiti lagani algorithmic reverb:

- Schroeder
- Freeverb-like
- FDN kao kasnija opcija

Parametri:

- room
- decay
- damping
- pre-delay
- wet/dry

---

# 35. Limiter

Na izlazu koristiti soft limiter kako se ne bi događao digitalni clipping kod mnogo aktivnih voiceova.

Također dodati:

- master gain
- output meter
- clipping counter

---

# 36. Stereo arhitektura

Iako onboard audio može završiti kao mono, cijeli synth treba interno dizajnirati kao stereo.

Razlozi:

- chorus
- stereo delay
- reverb
- voice pan
- sample stereo data
- budući stereo line-out

Na onboard outputu se po potrebi radi:

```text
L + R
-----
  2
```

---

# 37. Budući line output

Speaker output nije isto što i line output.

Ne treba direktno spajati pojačani speaker output na standardni RCA/AUX line input bez provjere topologije.

Za finalni uređaj predvidjeti:

```text
ESP32-P4
   │
I2S
   │
stereo DAC
   │
output buffer
   │
RCA / 3.5 mm LINE OUT
```

Moguće je zadržati onboard speaker kao monitor.

---

# 38. ESP32-C6 i ESP-Hosted

ESP32-P4 nema integrirani Wi-Fi pa ESP32-C6 služi kao mrežni koprocesor.

Arhitektura:

```text
ESP32-P4
   │
SDIO / ESP-Hosted
   │
ESP32-C6
   │
Wi-Fi
```

Koristiti:

- esp_hosted
- esp_wifi_remote

P4 ostaje glavni procesor.

C6 se tretira kao network interface.

---

# 39. Web server

ESP-IDF `esp_http_server` dovoljan je za projekt.

Ne treba teški web framework.

Web server služi:

- frontend datoteke
- REST API
- WebSocket
- file upload
- status
- preset management

---

# 40. WebSocket

WebSocket koristiti za real-time parametre.

Primjer:

```json
{
  "param": "filter.cutoff",
  "value": 0.715
}
```

ili optimizirani binary protocol kasnije.

Web task samo mijenja control state.

Audio thread čita shadow/cached vrijednosti bez blokiranja.

---

# 41. Web GUI

Predložene stranice:

## Performance

- preset
- bank
- active voices
- master volume
- transpose
- CPU usage
- DSP usage
- MIDI status

## Oscillators

- OSC1
- OSC2
- waveform
- tune
- detune
- levels
- sub
- noise

## Filter

- type
- cutoff
- resonance
- env amount
- key tracking

## Envelopes

- Amp ADSR
- Filter ADSR

## LFO

- waveform
- rate
- depth
- destination

## Mod Matrix

- source
- amount
- destination

## Effects

- chorus
- delay
- reverb
- EQ
- limiter

## MIDI

- USB device
- channel
- event monitor
- MIDI Learn
- mapping
- velocity curve
- pitch bend range

## SoundFont

- upload
- file browser
- bank
- preset
- unload

## Storage

- SD status
- used/free space
- files

## System

- Wi-Fi
- network
- firmware
- OTA
- uptime
- CPU
- PSRAM
- heap
- audio underruns
- logs

---

# 42. mDNS

Omogućiti pristup preko npr.:

```text
http://midisynth.local
```

Time korisnik ne mora znati IP adresu uređaja.

---

# 43. Wi-Fi konfiguracija

Podržati:

- Station mode
- eventualno fallback AP mode

Primjer fallback AP-a:

```text
SSID: P4Synth-Setup
```

Web captive/config portal može omogućiti izbor Wi-Fi mreže.

---

# 44. microSD

Predložena struktura:

```text
/synth/
    presets/
    soundfonts/
    samples/
    wavetables/
    recordings/
    config/
```

Potrebno je prije konačne implementacije potvrditi pinove i moguće konflikte između:

- microSD
- ESP32-C6 SDIO
- drugih board funkcija

na točnoj reviziji JC-ESP32P4-M3-DEV ploče.

---

# 45. NVS

NVS koristiti samo za male vrijednosti:

- Wi-Fi
- boot preset
- global MIDI configuration
- master settings
- hostname

Velike datoteke ne treba držati u NVS-u.

---

# 46. OTA

Implementirati A/B OTA:

```text
factory
ota_0
ota_1
```

Uz rollback.

OTA upload može biti dostupan kroz web GUI.

Firmware upgrade ne smije moći uništiti radni firmware.

---

# 47. Diagnostics

Web GUI treba prikazivati:

- free internal heap
- free PSRAM
- CPU load
- active voices
- max voices
- audio underrun count
- I2S errors
- MIDI packet count
- USB reconnect count
- SD errors
- Wi-Fi RSSI
- uptime

---

# 48. Profiling

Od početka dodati performance counters.

Primjer:

```text
audio block processing time
maximum block processing time
average DSP time
CPU load
voice count
XRUN/underrun count
```

Bitno pravilo:

```text
processing_time < audio_block_duration
```

Za 128 frameova / 48 kHz:

```text
processing_time < 2.667 ms
```

uz dovoljno sigurnosne margine.

---

# 49. Memory strategy

Interni SRAM:

- task stackovi
- MIDI queue
- audio DMA
- voice state
- kritični DSP state

PSRAM:

- SoundFont/sample data
- reverb/delay
- wavetable bankovi
- file cache
- velike strukture

Flash:

- firmware
- web frontend
- default presets

SD:

- user content

---

# 50. RTOS taskovi

Početni prijedlog:

```text
audio_task
priority: very high
core: 1

usb_midi_task
priority: high
core: 0

midi_router_task
priority: high
core: 0

wifi_task
core: 0

web_task
core: 0

storage_task
core: 0

system_monitor_task
low priority
```

Točne prioritete treba odrediti nakon testiranja.

---

# 51. Zabranjene operacije u audio tasku

Audio task ne smije raditi:

```text
malloc()
free()
printf()
ESP_LOGI()
file read
file write
HTTP
Wi-Fi calls
USB enumeration
mutex wait bez vrlo jasne garancije
```

Sve potrebno treba biti unaprijed alocirano.

---

# 52. Parametar smoothing

Promjene parametara iz browsera i MIDI knobova ne smiju direktno skakati.

Primjer:

```text
cutoff_target = new_value

cutoff_current +=
    smoothing *
    (cutoff_target - cutoff_current)
```

Time se izbjegavaju klikovi i zipper noise.

---

# 53. Arpeggiator

Kasnija performance funkcija:

- up
- down
- up/down
- random
- chord
- latch

Parametri:

- BPM
- division
- gate
- octave range

---

# 54. Split keyboard

GX49 može biti podijeljen:

```text
C1–B2 → bass
C3–C6 → piano
```

ili:

```text
LEFT  → SoundFont bass
RIGHT → VA lead
```

---

# 55. Layers

Omogućiti istovremeni playback više enginea:

```text
Layer 1: piano
Layer 2: pad
```

Svaki layer može imati:

- key range
- velocity range
- transpose
- volume
- pan
- MIDI channel

---

# 56. Multi-engine arhitektura

Idealno dugoročno:

```text
MIDI
 │
 ▼
Performance Engine
 │
 ├── VA Synth
 ├── SoundFont
 ├── Wavetable
 ├── Sampler
 └── FM
       │
       ▼
      Mixer
       │
       ▼
      FX
```

Engine interface:

```c
typedef struct synth_engine {
    void (*note_on)(...);
    void (*note_off)(...);
    void (*control)(...);
    void (*render)(float *l, float *r, size_t n);
} synth_engine_t;
```

---

# 57. Buduća druga USB funkcija

ESP32-P4 ima dva USB kontrolera.

Zato dugoročno možemo imati:

```text
USB #1:
GX49
ESP32-P4 = HOST
```

i istodobno:

```text
USB #2:
ESP32-P4 = DEVICE
Computer = HOST
```

Drugi USB mogao bi nuditi:

- USB MIDI Device
- USB Audio Device
- MIDI + Audio composite device

Tada bi uređaj postao:

- hardware synth
- sound module
- MIDI interface
- USB audio uređaj

---

# 58. Predložena struktura repozitorija

```text
p4-midi-synth/
│
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
│
├── main/
│   ├── app_main.c
│   └── CMakeLists.txt
│
├── components/
│   ├── board/
│   ├── usb_host_adapter/
│   ├── midi/
│   ├── audio_hal/
│   ├── synth_core/
│   ├── synth_va/
│   ├── synth_sf2/
│   ├── dsp/
│   ├── effects/
│   ├── preset/
│   ├── storage/
│   ├── networking/
│   ├── webserver/
│   └── system_monitor/
│
├── webui/
│
├── tools/
│   └── p4sf-converter/
│
├── docs/
│
└── tests/
```

---

# 59. Implementacijski plan

## Faza M0 — Hardware/BSP audit

Potvrditi:

- točnu reviziju ploče
- ES8311
- I2S GPIO
- I2C
- AMP_EN
- USB
- microSD
- C6 SDIO
- napajanja

Napraviti board component.

---

## Faza M1 — Audio bring-up

Cilj:

```text
ESP32-P4 → I2S → ES8311 → zvučnik
```

Generirati:

**440 Hz sine**

Potvrditi:

- 48 kHz
- stabilan DMA
- nema underruna
- volume control radi

---

## Faza M2 — Integracija postojećeg USB hosta

Ne razvijamo novi USB host.

Integrirati postojeće rješenje kao component ili adapter.

Potvrditi:

- connect
- disconnect
- reconnect
- MIDI endpoint
- stabilno primanje podataka

---

## Faza M3 — MIDI parser

Implementirati:

- Note On
- Note Off
- Velocity
- CC
- Pitch Bend
- Sustain
- Program Change

Napraviti MIDI event queue.

---

## Faza M4 — First Note

Minimalni synth:

```text
GX49
 │
 ▼
MIDI
 │
 ▼
sine oscillator
 │
 ▼
ES8311
```

Cilj:

**pritisak tipke na GX49 proizvodi odgovarajući ton.**

To je prvi glavni milestone.

---

## Faza M5 — Polyphonic synth

Dodati:

- 16 voices
- voice allocator
- ADSR
- velocity
- sustain
- pitch bend

---

## Faza M6 — Virtual Analog engine

Dodati:

- OSC1
- OSC2
- sine
- triangle
- saw
- square
- PolyBLEP
- sub
- noise
- resonant filter
- LFO
- glide
- mono/poly
- unison

---

## Faza M7 — ESP-Hosted

Pokrenuti:

```text
P4 ↔ C6 ↔ Wi-Fi
```

Dodati:

- DHCP
- hostname
- mDNS

---

## Faza M8 — Web GUI

Implementirati:

- HTTP server
- WebSocket
- REST
- synth control UI
- MIDI monitor
- system status

---

## Faza M9 — Preseti

Implementirati:

- save
- load
- rename
- delete
- bank
- program change

---

## Faza M10 — microSD

Dodati storage layer.

Directoryji:

- presets
- soundfonts
- samples
- wavetables

---

## Faza M11 — SoundFont

Integrirati TinySoundFont.

Testirati mali SF2.

Dodati:

- bank
- instrument
- polyphony
- PSRAM loading

---

## Faza M12 — P4SF

Napraviti vlastiti offline converter.

```text
SF2 → P4SF
```

Optimizirati sample storage.

---

## Faza M13 — Effects

Dodati:

- chorus
- delay
- reverb
- EQ
- limiter

---

## Faza M14 — Performance features

Dodati:

- MIDI Learn
- split
- layers
- arpeggiator
- transpose
- velocity curves
- chord mode
- favorites

---

## Faza M15 — OTA

Dodati:

- A/B OTA
- rollback
- web firmware upload

---

## Faza M16 — Reliability

Testirati:

- USB hotplug
- Wi-Fi reconnect
- SD removal
- corrupted preset
- synth overload
- audio underrun
- watchdog
- brownout / restart

---

## Faza M17 — Final hardware

Kasnije razmotriti:

- stereo DAC
- RCA line out
- 3.5 mm line out
- headphone amplifier
- DIN MIDI IN
- DIN MIDI OUT
- rotary encoder
- lokalni display
- finalno kućište

---

# 60. Prvi milestone koji treba zaključati

Prvi stvarni cilj projekta treba biti:

```text
Nektar Impact GX49
        │
        ▼
postojeći USB Host
        │
        ▼
MIDI parser
        │
        ▼
MIDI queue
        │
        ▼
16-voice synth
        │
        ▼
48 kHz audio
        │
        ▼
ES8311
        │
        ▼
speaker
```

Za uspjeh milestonea mora vrijediti:

- Note On radi
- Note Off radi
- velocity radi
- pitch bend radi
- sustain radi
- nema audio dropouta
- nema klikova
- nema primjetne latencije
- USB reconnect radi

Tek nakon toga graditi Wi-Fi, web UI, SoundFont i napredne funkcije.

---

# 61. Glavne projektne odluke

Preporučeno je odmah zaključati sljedeće:

1. ESP-IDF kao jedini primarni framework.
2. Postojeći USB host ostaje zaseban podsustav.
3. Audio engine mora biti potpuno real-time safe.
4. Interni DSP format je float32.
5. Interni audio engine je stereo.
6. Osnovni sample rate je 48 kHz.
7. VA synth je prvi engine.
8. SoundFont dolazi nakon stabilnog VA syntha.
9. Web GUI ne smije izravno blokirati audio.
10. PSRAM koristiti za velike podatke, ne kritični audio state.
11. Hardware detalje sakriti iza BSP-a.
12. Svaki subsistem treba biti zaseban ESP-IDF component.
13. Od početka mjeriti CPU, DSP vrijeme i audio underrun.
14. Dizajn treba ostaviti otvoren put za drugi USB kontroler u device modu.
15. Finalni sustav treba biti sposoban raditi bez računala.

---

# 62. Dugoročna vizija

Konačni uređaj može postati puno više od jednostavnog ESP32 MIDI syntha.

Moguća konačna funkcionalnost:

```text
USB MIDI Host
+
Virtual Analog Synth
+
SoundFont Player
+
Sampler
+
Wavetable Synth
+
Effects Processor
+
Preset Manager
+
MIDI Learn
+
Wi-Fi Web UI
+
OTA
+
microSD Library
+
USB MIDI Device
+
USB Audio Device
```

Drugim riječima:

**samostalni embedded hardware synthesizer / sound module / MIDI interface temeljen na ESP32-P4.**

---

# 63. Preporučeni prvi razvojni redoslijed

Najkraći put do funkcionalnog proizvoda:

```text
1. BSP
2. ES8311 + I2S
3. 440 Hz sine test
4. postojeći USB host integration
5. MIDI Note On/Off
6. single oscillator
7. ADSR
8. 16-voice allocator
9. pitch bend + velocity + sustain
10. VA oscillatori
11. filter
12. LFO
13. ESP-Hosted Wi-Fi
14. WebSocket/web UI
15. preset system
16. SoundFont
17. effects
18. advanced performance features
19. OTA
20. final hardware
```

To je preporučeni baseline arhitekture za početak razvoja.
