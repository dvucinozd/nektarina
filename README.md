# NEKTARINA - ESP32-S3 USB-MIDI Synthesizer (N16R8)

Repozitorij: [github.com/dvucinozd/nektarina](https://github.com/dvucinozd/nektarina.git)

Samostalni ESP32-S3 USB-MIDI synthesizer za Nektar klavijature (Impact GX49 i srodne).

Arhitektura i migracija: [M0-S3 Migration Audit](docs/M0_S3_MIGRATION_AUDIT.md).

## Hardverska Konfiguracija
- **MCU:** ESP32-S3 Dual-Core Xtensa LX7 @ 240 MHz
- **Memorija:** 16 MB Octal Flash + 8 MB Octal PSRAM (`MALLOC_CAP_SPIRAM`)
- **Audio izlaz:** MAX98357A I2S mono pojačalo
  - BCLK: `GPIO 16`
  - WS (LRC): `GPIO 17`
  - DOUT: `GPIO 18`
- **USB Host:** Integrirani USB OTG Full-Speed PHY (GPIO 19 D-, GPIO 20 D+)

> [!CAUTION]
> **GPIO 33–37 SU ZABRANJENI:** Ovi pinovi su na Octal modulu vezani za OPI sabirnicu.

## Arhitektura Podsustava
1. **audio_hal:** Moderni ESP-IDF I2S master driver (`driver/i2s_std.h`), 44.1 kHz 16-bit stereo.
2. **usb_midi_host:** USB Host stog koji automatski prepoznaje Nektar / USB MIDI class-compliant kontrolere, dekodira 4-bajtne USB-MIDI pakete i šalje ih u FreeRTOS Queue.
3. **synth_engine:**
   - **Engine B (80s Virtual Analog):** 16 polifonih glasova, PolyBLEP Saw/Pulse oscilatori, rezonantni 2-polni State Variable Filter (SVF), ADSR omotnica, Pitch Bend i Mod Wheel.
   - **Engine A (Grand Piano):** TinySoundFont SF2 player s alokacijom uzoraka u PSRAM-u.
4. **app_main:** Memorijska telemetrija, 1.5s 440 Hz test ton za brzu zvučnu potvrdu MAX98357A pojačala, i dispečiranje MIDI događaja.

## Build naredba (ESP-IDF 6.0.2)

```powershell
. C:\Espressif\v6.0.2\esp-idf\export.ps1
idf.py set-target esp32s3
idf.py build
```
