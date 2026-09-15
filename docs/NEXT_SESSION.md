# Sljedeći Koraci (Next Session)

**Datum:** 2026-09-15  
**Trenutno Stanje:** Firmware uspješno izgrađen i flashan na ESP32-S3 (N16R8) na COM12.

---

## 1. Što je Završeno
- [x] Migracija platforme s ESP32-P4 na ESP32-S3-WROOM-1-N16R8.
- [x] Prepoznavanje 16 MB Flash i 8 MB Octal PSRAM memorije.
- [x] Implementacija I2S drivera za MAX98357A (GPIO 16, 17, 18).
- [x] Implementacija USB Host MIDI stoga (GPIO 19, 20) s parsiranjem 4-bajtnih USB-MIDI paketa.
- [x] Implementacija 16-glasovnog 80s Virtual Analog synth enginea i integracija TinySoundFont-a u PSRAM-u.
- [x] Zvučni testni ton 440 Hz (A4) na pokretanju.

---

## 2. Sljedeće Fizičke Radnje (Korisnik)
1. **Povezivanje MAX98357A pojačala:**
   - `BCLK` -> `GPIO 16`
   - `LRC / WS` -> `GPIO 17`
   - `DIN` -> `GPIO 18`
   - `VIN` -> `5V` (ili `3.3V`)
   - `GND` -> `GND`
   - Spojiti zvučnik (4 Ω / 8 Ω) na izlazne stezaljke pojačala.
2. **Spajanje Nektar klavijature:**
   - Povezati klavijaturu na USB-OTG USB-C priključak ESP32-S3 ploče.
   - Pritisnuti tipke na klavijaturi i provjeriti `Note On` / `Note Off` logove u konzoli te zvuk na zvučniku.
