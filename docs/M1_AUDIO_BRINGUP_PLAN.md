# M1 - Audio Bring-Up Plan: MAX98357A I2S Mono Pojačalo

> **Povijesni dokument:** pinout `16/17/18` ispod nije aktualan. Potvrđeni pinout je `45/3/47`, `SD_MODE=14`; pogledati [WIRING_DIAGRAM.md](../WIRING_DIAGRAM.md).

**Status:** **M1 funkcionalno završen na ESP32-S3**. I2S driver, DMA cjevovod i testni ton potvrđeni.

---

## 1. Hardverska Arhitektura
- **Pojačalo:** Maxim Integrated MAX98357A (I2S Class-D mono DAC/AMP)
- **Takt i podatci:**
  - BCLK: `GPIO 16`
  - WS (LRC): `GPIO 17`
  - DOUT: `GPIO 18`
- **Konfiguracija:**
  - GAIN: Lebdeći ($12\text{ dB}$)
  - SD_MODE: Lebdeći (hardverski stereo downmix: $\frac{L + R}{2}$)
  - Bez I2C sabirnice i registara. Pojačalo autonomno pretvara I2S stream u analogni signal za zvučnik.

---

## 2. Softverska Implementacija (`components/audio_hal`)
1. **Driver:** Moderni ESP-IDF standardni I2S driver (`driver/i2s_std.h`).
2. **Format:** 44100 Hz, 16-bit stereo Philips standard.
3. **DMA konfiguracija:**
   - 6 DMA deskriptora, 256 okvira po međuspremniku.
   - Međuspremnici i deskriptori alocirani isključivo u internoj memoriji (`MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`).
   - `auto_clear = true` (tišina kada nema novih podataka).

---

## 3. Verifikacijski Kriteriji M1
- [x] I2S kanal se uspješno inicijalizira i omogućuje (`i2s_channel_enable`).
- [x] Sinusni ton frekvencije 440 Hz (A4) u trajanju od 1.5 sekundi renderira se u DMA međuspremnik.
- [x] Nema underruna, DMA zastoja niti rušenja sustava.
- [x] Prilikom spajanja zvučnika na MAX98357A, ton se jasno i glasno čuje pri svakom pokretanju uređaja.
