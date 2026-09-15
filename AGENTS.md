# AGENTS.md - Upute i Vodič za Agente za Nastavak Razvoja (NEKTARINA)

**Projekt:** NEKTARINA - Samostalni ESP32-S3 USB-MIDI Synthesizer  
**Ciljni hardver:** ESP32-S3-WROOM-1-N16R8  
**Repozitorij:** [https://github.com/dvucinozd/nektarina.git](https://github.com/dvucinozd/nektarina.git)  
**Datum ažuriranja:** 2026-09-15  
**Verzija:** 0.2.0-s3-midi  

---

## 1. Arhitektura Sustava i Uloga

Ovaj projekt implementira samostalni hardverski synthesizer pogonjen mikrokontrolerom ESP32-S3:
- **Ulaz:** Nektar MIDI klavijatura (npr. Impact GX49 ili bilo koji standardni USB MIDI kontroler) spojena na ugrađeni USB-OTG priključak mikrokontrolera u USB Host načinu rada.
- **Procesiranje:** Polifoni audio DSP synth engine koji radi u stvarnom vremenu na Core 1 FreeRTOS zadatka.
- **Izlaz:** MAX98357A I2S Class-D mono pojačalo spojeno na zvučnik (44.1 kHz, 16-bit stereo PCM stream).

---

## 2. Hardverske Specifikacije i Striktna Ograničenja

### 2.1 MCU i Memorija
- **SoC:** ESP32-S3 (Dual-Core Xtensa® 32-bit LX7 @ 240 MHz)
- **Flash:** 16 MB Octal SPI (OPI) High-Speed Flash (DIO / 80 MHz)
- **PSRAM:** 8 MB Octal SPI (OPI) High-Speed PSRAM (80 MHz)
- **FreeRTOS Takt:** 1000 Hz (`CONFIG_FREERTOS_HZ=1000`)

### 2.2 ⚠️ KRITIČNA HARDVERSKA ZABRANA (Octal Sabirnica)
> [!CAUTION]
> **GPIO 33, 34, 35, 36 i 37 SU NA STROGOJ ZABRANI KORIŠTENJA.**
> Na N16R8 modulima ovi su pinovi interno izravno povezani na brzu sabirnicu Octal Flasha i Octal PSRAM-a.
> Dodjela bilo koje funkcije, promjena smjera ili povlačenje ovih pinova dovodi do trenutnog rušenja mikrokontrolera (`Guru Meditation Error / Cache Panic`).
> **Kameru OV3660 na pločici u potpunosti ignorirati** (DVP/CSI sabirnica se ne smije inicijalizirati).

### 2.3 Pinout Tablica

| Funkcija | ESP32-S3 GPIO | Periferija / Spojeno na | Napomena |
| :--- | :--- | :--- | :--- |
| **I2S BCLK** | **GPIO 16** | MAX98357A BCLK | Bit Clock (1.4112 MHz za 44.1 kHz 16-bit stereo) |
| **I2S WS (LRC)** | **GPIO 17** | MAX98357A LRC | Word Select / Frame Clock (44.1 kHz) |
| **I2S DOUT** | **GPIO 18** | MAX98357A DIN | Serijski PCM audio podatci |
| **USB D-** | **GPIO 19** | Nektar USB-C Port | Ugrađeni USB-OTG Full-Speed PHY D- |
| **USB D+** | **GPIO 20** | Nektar USB-C Port | Ugrađeni USB-OTG Full-Speed PHY D+ |
| *UART TX/RX* | *GPIO 43 / 44* | Console / Serial | UART 0 konzola za logiranje i programiranje (COM12) |
| *Flash/PSRAM* | *GPIO 33–37* | **INTERNO (Octal SPI)**| **NIKADA NE KORISTITI U KODU** |

### 2.4 Audio Podsustav (MAX98357A)
- Nema I2C sabirnice, nema internih registara.
- Čip radi potpuno autonomno čim prima I2S takt i podatke.
- **GAIN:** Ostavljen nepospojen ($12\text{ dB}$ tvorničko pojačanje).
- **SD_MODE:** Ostavljen nepospojen (hardverski downmix stereo signala u mono sumu: $\frac{L + R}{2}$).

---

## 3. Struktura Komponenti i Softverska Arhitektura

```text
├── CMakeLists.txt              # Target esp32s3, projekt nektar_s3_midi
├── sdkconfig.defaults          # Konfiguracija za 16MB Flash, 8MB Octal PSRAM, USB Host, FreeRTOS
├── main/
│   ├── CMakeLists.txt          # Registracija aplikacije s ovisnostima
│   ├── app_main.c              # Inicijalizacijski pipeline, memorijska telemetrija, test ton, status
│   └── idf_component.yml       # Upravljana ovisnost: espressif/usb: "^1.5.0"
├── components/
│   ├── audio_hal/              # I2S master driver za MAX98357A (esp_driver_i2s / driver/i2s_std.h)
│   │   ├── include/audio_hal.h # Standardizirani API: audio_hal_init, audio_hal_write
│   │   └── audio_hal.c
│   ├── usb_midi_host/          # USB Host MIDI stog (Full-Speed OTG PHY)
│   │   ├── include/usb_midi_host.h # midi_message_t definicije, init i status API
│   │   └── usb_midi_host.c     # 4-bajtni paketni parser, deskriptor skener, hot-plug
│   └── synth_engine/           # Polifoni audio DSP sintetizator
│       ├── include/synth_engine.h # Kontrole: note_on, note_off, pitch_bend, control_change, modovi
│       ├── include/tsf.h       # TinySoundFont biblioteka s PSRAM alokacijom
│       └── synth_engine.c      # 16-glasovni 80s VA synth + SF2 player, Core 1 zadatak
└── docs/                       # Tehnička dokumentacija, auditi i rezultati testova
    ├── M0_S3_MIGRATION_AUDIT.md
    ├── M0_COM12_S3_CHECK.md
    ├── M0_VALIDATION.md
    ├── M1_AUDIO_BRINGUP_PLAN.md
    ├── M1_AUDIO_TEST.md
    ├── M2_USB_HOST_ADOPTION_PLAN.md
    ├── NEXT_SESSION.md
    ├── REPOSITORY_CONTENTS.md
    └── STATE_SUMMARY.md
```

### 3.1 Stroga Pravila Alokacije Memorije
- **I2S DMA deskriptori i međuspremnici:**
  Isključivo u internoj memoriji s DMA podrškom:
  ```c
  void *dma_buf = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  ```
- **Velike sample tablice i SoundFont (SF2) banke:**
  Isključivo u 8 MB Octal PSRAM-u:
  ```c
  void *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  ```
- `tsf.h` alokatori su preusmjereni na PSRAM:
  ```c
  #define TSF_MALLOC(sz)     heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
  #define TSF_REALLOC(p, sz) heap_caps_realloc(p, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
  #define TSF_FREE(p)        free(p)
  ```

### 3.2 Synth Engine Detalji
- **Engine B (80s Virtual Analog):**
  - 16 glasova polifonije s inteligentnim voice-stealingom (krade najstariji/najtiši glas).
  - Oscilatori: Sawtooth (65%) + Pulse/Square (35%) s PolyBLEP anti-aliasingom za kristalno čist ton bez distorzije na visokim frekvencijama.
  - 2-polni rezonantni State Variable Filter (SVF) s envelope modulacijom i kontrolom rezonancije (Q).
  - 4-stupanjska ADSR omotnica (Attack 8ms, Decay 250ms, Sustain 65%, Release 350ms).
  - Puni MIDI odziv: Pitch Bend ($\pm 2$ polutona), CC 1 (Mod Wheel), CC 7 (Volume), CC 71 (Resonance), CC 74 (Filter Cutoff), CC 120/123 (All Notes Off).
- **Engine A (TinySoundFont Grand Piano):**
  - Podržava učitavanje standardnih SF2 datoteka u PSRAM putem `synth_engine_load_soundfont(data, size)`.

---

## 4. Razvojno Okruženje i Naredbe

- **ESP-IDF verzija:** `6.0.2`
- **Putanja alata:** `C:\Espressif\v6.0.2\esp-idf`
- **Python okruženje:** `C:\Espressif\python_env\idf6.0_py3.11_env\Scripts\python.exe`
- **Serijski port:** `COM12` (brzina 115200 baud, flash 460800 baud)

### Naredbe za Build i Flash (PowerShell)

1. **Aktivacija ESP-IDF okruženja:**
   ```powershell
   . C:\Espressif\v6.0.2\esp-idf\export.ps1
   ```
2. **Postavljanje targeta (ako se kreira novi build direktorij):**
   ```powershell
   idf.py set-target esp32s3
   ```
3. **Kompilacija projekta:**
   ```powershell
   idf.py build
   ```
4. **Flashanje na uređaj:**
   ```powershell
   idf.py -p COM12 flash
   ```
5. **Pokretanje serijskog monitora:**
   ```powershell
   idf.py -p COM12 monitor
   ```
   *(Izlaz iz monitora: `Ctrl + ]`)*

---

## 5. Zadaci za Daljnji Razvoj (Roadmap)

1. **Fizički akustički test:**
   - Spojiti zvučnik na MAX98357A stezaljke, te spojiti pojačalo na GPIO 16 (BCLK), 17 (WS), 18 (DOUT), 5V i GND.
   - Poslušati 1.5s 440 Hz test ton pri bootu.
2. **Test Nektar klavijature:**
   - Uključiti Nektar Impact GX49 u USB-OTG USB-C port ploče.
   - Provjeriti u logu:
     `I (MIDI) >>> Nektar MIDI keyboard CONNECTED and active <<<`
     `I (MIDI) Note On: Note=60, Velocity=100`
   - Provjeriti odziv zvuka u stvarnom vremenu.
3. **Učitavanje SoundFont banke (Engine A):**
   - Pripremiti kompaktnu SF2 SoundFont datoteku (npr. General MIDI Piano SF2 veličine 2–6 MB).
   - Ugraditi je u SPIFFS particiju ili embeddati kao binarni simbol u firmware te učitati u PSRAM pozivom `synth_engine_load_soundfont()`.
4. **Efekti i DSP nadogradnje:**
   - Stereo Chorus / Flanger efekt za autentičan 80s analogni karakter.
   - Stereo Ping-Pong Delay u PSRAM-u.
