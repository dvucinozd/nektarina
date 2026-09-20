# M0-S3 - Hardware & Architecture Migration Audit: ESP32-S3-WROOM-1-N16R8

> **Povijesni dokument:** pinout `16/17/18` i nepovezani SD_MODE iz ovog audita više nisu aktualni. Potvrđeni pinout je `45/3/47`, `SD_MODE=14`; pogledati [WIRING_DIAGRAM.md](../WIRING_DIAGRAM.md).

**Datum:** 2026-09-15  
**Verzija:** 0.2.0-s3-midi  
**Status:** Migracija s ESP32-P4 na ESP32-S3 uspješno specificirana i implementirana.

---

## 1. Kontekst i Svrha Migracije

Izvorni repozitorij `dvucinozd/nektar-midi` razvijan je za eksperimentalnu platformu ESP32-P4 (`board_jc_esp32p4_m3`) s integriranim ES8311 I2C/I2S audio kodekom i složenom sabirnicom.
Cilj ove migracije je prelazak na industrijsku produkcijsku ploču **ESP32-S3-WROOM-1-N16R8**, eliminacija kompleksnog I2C kodeka u korist čistog, robusnog I2S mono pojačala **MAX98357A**, te puna implementacija **USB Host MIDI** stoga za kontrolu zvuka putem Nektar klavijature.

---

## 2. Specifikacija Hardvera

### 2.1 Mikrokontroler i Memorijska Arhitektura
- **MCU:** Espressif ESP32-S3 (Dual-Core Xtensa® 32-bit LX7 @ 240 MHz)
- **Flash:** 16 MB Octal SPI (OPI) High-Speed Flash
- **PSRAM:** 8 MB Octal SPI (OPI) High-Speed PSRAM
- **FreeRTOS Takt:** 1000 Hz (`CONFIG_FREERTOS_HZ=1000`)

### 2.2 Stroga Hardverska Zabrana (Octal Sabirnica)
> [!CAUTION]
> **GPIO 33, 34, 35, 36 i 37 SU STROGO ZABRANJENI.**
> Na N16R8 modulima (16 MB OPI Flash + 8 MB OPI PSRAM), ovi pinovi su interno trajno povezani na brzu Octal SPI sabirnicu memorijskog kontrolera. Konfiguriranje ili dodjela funkcije bilo kojem od ovih pinova rezultira trenutnim padom mikrokontrolera (`Guru Meditation Error / Cache Panic`).
> Kamera OV3660 na razvojnoj pločici se u potpunosti ignorira (DVP/CSI sabirnica se ne inicijalizira).

### 2.3 Audio Izlaz: MAX98357A I2S Mono Pojačalo
MAX98357A je integrirani I2S DAC i pojačalo D-klase (3.2 W u 4 Ω) koje radi potpuno autonomno bez potrebe za I2C konfiguracijom ili kontrolnim registrima:
- **BCLK (Bit Clock):** `GPIO 16`
- **WS / LRC (Word Select / Left-Right Clock):** `GPIO 17`
- **DOUT (Serial Data - DIN na pojačalu):** `GPIO 18`
- **MCLK / DIN:** Nisu spojeni (`I2S_GPIO_UNUSED`).
- **GAIN:** Ostavljen nepospojen ($12\text{ dB}$ tvorničko pojačanje).
- **SD_MODE:** Ostavljen nepospojen (automatski hardverski zbraja stereo stream u mono sumu: $\frac{L + R}{2}$).

### 2.4 USB Host Priključak (Nektar MIDI)
- Koristi se ugrađeni Full-Speed (12 Mbps) USB OTG PHY:
  - **USB D-:** `GPIO 19`
  - **USB D+:** `GPIO 20`
- Napajanje: 5 V VBUS preko USB-OTG konektora.

---

## 3. Sažetak Pinouta

| Funkcija | ESP32-S3 GPIO | Spojeno na komponentu | Napomena |
| :--- | :--- | :--- | :--- |
| **I2S BCLK** | **GPIO 16** | MAX98357A BCLK | Bit clock (44.1 kHz × 32 = 1.4112 MHz) |
| **I2S WS** | **GPIO 17** | MAX98357A LRC | Word Select / Frame Sync (44.1 kHz) |
| **I2S DOUT** | **GPIO 18** | MAX98357A DIN | 16-bit stereo PCM podatci |
| **USB D-** | **GPIO 19** | Nektar USB-C Port | Full-Speed USB Host D- |
| **USB D+** | **GPIO 20** | Nektar USB-C Port | Full-Speed USB Host D+ |
| *Flash/PSRAM* | *GPIO 33–37* | **INTERNO (Octal SPI)** | **ZABRANJENO KORISTITI U FIRMWAREU** |

---

## 4. Arhitektura Softverskih Slojeva

```
┌─────────────────────────────────────────────────────────────┐
│                       main/app_main.c                       │
│      (Boot telemetrija, 440 Hz test ton, status heartbeat)   │
└──────────────┬───────────────────────────────┬──────────────┘
               │                               │
               ▼                               ▼
┌─────────────────────────────┐ ┌─────────────────────────────┐
│  components/usb_midi_host   │ │   components/synth_engine   │
│  - USB Host Library & PHY   │ │  - Core 1 synth_task (prio 19)
│  - MIDIStreaming parser     │ │  - Engine A: TinySoundFont  │
│  - 4-bajtni USB-MIDI decoding│ │  - Engine B: 80s VA Synth   │
└──────────────┬──────────────┘ └──────────────┬──────────────┘
               │                               │
               │      FreeRTOS Queue           │
               └───────────────────────────────┤
                                               ▼
                                ┌─────────────────────────────┐
                                │    components/audio_hal     │
                                │  - esp_driver_i2s (std)     │
                                │  - Philips 16-bit 44.1 kHz  │
                                │  - MAX98357A (GPIO 16/17/18)│
                                └─────────────────────────────┘
```

### 4.1 Pravila Alokacije Memorije
1. **I2S DMA deskriptori i međuspremnici:**
   Alociraju se isključivo u internoj memoriji s DMA sposobnostima:
   ```c
   void *dma_buf = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
   ```
2. **SoundFont banke i sample tablice:**
   Alociraju se eksplicitno u 8 MB Octal PSRAM memoriji:
   ```c
   void *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
   ```

### 4.2 Synth Engine Značajke
- **Engine B (80s Virtual Analog):**
  - 16 polifonih glasova s dinamičkim voice-stealing algoritmom.
  - PolyBLEP anti-aliasing na Sawtooth i Pulse/Square oscilatorima.
  - 2-polni rezonantni State Variable Filter (SVF) s envelope modulacijom.
  - 4-stupanjska ADSR omotnica (Attack, Decay, Sustain, Release).
  - Podrška za Pitch Bend ($\pm 2$ polutona) i Mod Wheel (CC 1).
- **Engine A (TinySoundFont Grand Piano):**
  - Cijela SF2 banka i generirani uzorci alociraju se u PSRAM-u putem prepisanih `TSF_MALLOC`/`TSF_FREE` makroa.
