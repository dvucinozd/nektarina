# M0 Validacija - ESP32-S3 (2026-09-15)

> **Povijesni rezultat:** tablice i logovi ispod opisuju tadašnji firmware. Aktualni potvrđeni MAX98357A pinout je `45/3/47`, `SD_MODE=14`; pogledati [WIRING_DIAGRAM.md](../WIRING_DIAGRAM.md).

## 1. Aktualni Build i Flash na COM12

**PASS:** `idf.py build` i `idf.py -p COM12 flash` završeni s exit code 0 na ESP-IDF 6.0.2.

- **Ciljna platforma:** ESP32-S3 (N16R8), CPU Dual-Core LX7 @ 240 MHz.
- **Konfiguracija:** Flash 16 MB DIO/80 MHz, PSRAM 8 MB Octal/80 MHz.
- **Aplikacija:** `build/nektar_s3_midi.bin`, veličina 290912 bajtova (`0x47060`).
- **Particija:** `factory` app (1 MB), preostalo 765856 bajtova (72% slobodno).
- **Hardverska provjera:** Flashano na COM12, bootloader i aplikacija uspješno pokrenuti bez grešaka ili rušenja.

---

## 2. Ponovljiv Postupak

Iz korijena repozitorija u PowerShellu:

```powershell
. C:\Espressif\v6.0.2\esp-idf\export.ps1
idf.py set-target esp32s3
idf.py build
idf.py -p COM12 flash
```

Za serijski monitor:
```powershell
idf.py -p COM12 monitor
```

---

## 3. Matrica Validacije

| Provjera | Očekivano | Izmjereno / Rezultat | Status |
| :--- | :--- | :--- | :--- |
| **Target SoC** | `esp32s3` | `esp32s3` (revizija v0.2) | **PASS** |
| **Flash memorija** | 16 MB | 16 MB detektirano | **PASS** |
| **PSRAM memorija** | 8 MB Octal (80 MHz) | 8 MB detektirano, SRAM test OK | **PASS** |
| **Zabranjeni pinovi (GPIO 33–37)** | Nula referenci | 0 referenci u cijelom kodu | **PASS** |
| **I2S Driver (MAX98357A)** | BCLK=16, WS=17, DOUT=18 | Inicijaliziran, kanal omogućen | **PASS** |
| **Akustični test ton** | 440 Hz, 1.5s sinus | Generiran i poslan u DMA buffer | **PASS** |
| **USB Host Driver** | OTG Full-Speed (GPIO 19/20) | Lib & Client taskovi aktivni | **PASS** |
| **Synth Engine** | Core 1, 16 glasova VA | Pokrenut task (prioritet 19) | **PASS** |
