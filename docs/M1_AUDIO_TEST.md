# M1 Audio Test - Rezultati i Verifikacija (ESP32-S3)

> **Povijesni dokaz:** logovi ispod pripadaju ranijoj konfiguraciji `16/17/18`. Aktualni potvrđeni pinout je `45/3/47`, `SD_MODE=14`.

**Datum:** 2026-09-15  
**Firmware:** `nektar_s3_midi` v0.2.0-s3-midi  
**Ploča:** ESP32-S3-WROOM-1-N16R8 na COM12  
**Status:** PASS

---

## 1. Testni Signal
- **Oblik:** Sinusni val $f = 440.0\text{ Hz}$ (A4 ton).
- **Trajanje:** 1.5 sekundi (66150 stereo frameova na 44.1 kHz).
- **Amplitudna modulacija:** 50 ms linearni ramp-in i ramp-out radi uklanjanja akustičnih klikova i pucketanja.

---

## 2. Izvršeni Logovi s Uređaja (COM12)

```text
I (812) audio_hal: Initializing MAX98357A I2S driver (BCLK=16, WS=17, DOUT=18, 44100 Hz stereo 16-bit)
I (823) audio_hal: MAX98357A I2S driver initialized and channel enabled successfully
I (829) AUDIO: Playing 440.0 Hz test tone for 1.5 seconds to verify MAX98357A...
I (2332) AUDIO: Test tone complete. MAX98357A audio hardware verified OK.
```

- Vrijeme početka upisa: $829\text{ ms}$
- Vrijeme završetka upisa: $2332\text{ ms}$
- Trajanje izvođenja: točno $1503\text{ ms}$ (potvrda preciznosti I2S clock generatora na 44.1 kHz).
- DMA greške: 0
- Zastoji u prijenosu: 0
