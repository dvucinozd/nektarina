# M0 - COM12 Provjera i Identifikacija ESP32-S3-WROOM-1-N16R8

**Datum:** 2026-09-15  
**Serijski port:** COM12  
**Brzina:** 115200 baud (komunikacija), 460800 baud (flashanje)  
**Status:** PASS - Uspješno prepoznat ESP32-S3 čip, 16 MB Flash i 8 MB Octal PSRAM.

---

## 1. Očitani Podatci o Čipu (esptool)

```text
Connected to ESP32-S3 on COM12:
Chip type:          ESP32-S3 (QFN56) (revision v0.2)
Features:           Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded PSRAM 8MB (AP_3v3)
Crystal frequency:  40MHz
MAC:                b8:f8:62:e0:9f:bc

Flash Memory Information:
=========================
Manufacturer: 5e
Device: 4018
Detected flash size: 16MB
Flash type set in eFuse: quad (4 data lines)
Flash voltage set by eFuse: 3.3V
```

---

## 2. Bootloader i Inicijalizacija PSRAM-a

Pri bootu sustava na COM12 potvrđeno je:
1. **Bootloader:** ESP-IDF v6.0.2 2nd stage bootloader, SPI Speed 80 MHz, Mode DIO, Flash Size 16 MB.
2. **Octal PSRAM detekcija:**
   - Vendor ID: `0x0d (AP)`
   - Density: `0x03 (64 Mbit = 8 MB)`
   - Speed: `80 MHz`
   - Test: `esp_psram: SPI SRAM memory test OK`
   - Heap: `esp_psram: Adding pool of 8192K of PSRAM memory to heap allocator`
3. **Telemetrija memorije u aplikaciji (`app_main`):**
   - Free Internal RAM: `383523 bytes (374.53 KB)`
   - Free Octal PSRAM:  `8386156 bytes (8.00 MB)`
   - Free DMA RAM:      `375735 bytes (366.93 KB)`

---

## 3. Zaključak

Ploča je 100% hardverski kompatibilna sa specificiranim postavkama za ESP32-S3-WROOM-1-N16R8.
Zabranjeni pinovi GPIO 33–37 nisu dirani, a memorija radi na punoj brzini od 80 MHz.
