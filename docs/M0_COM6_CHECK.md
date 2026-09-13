# M0 - provjera COM6, 2026-09-12

## Očitano s uređaja

- ESP32-P4, silicijska revizija **v1.3**, kristal 40 MHz.
- SPI flash JEDEC manufacturer `0x68`, device `0x4018`: **16 MB**.
- Postojeći firmware prepoznaje **32 MB PSRAM**, radi s njim na **20 MHz**,
  ispisuje `SPI SRAM memory test OK` i dodaje 32768K u heap.
- CPU postojećeg firmwarea: **360 MHz**.
- Console UART: P4 GPIO38 RX / GPIO37 TX prema boot logu.

Dokaz: [ROM/flash očitanje](evidence/m0-com6-flash-id.log) i
[boot postojećeg firmwarea](evidence/m0-com6-boot.log).
Za Git su normalizirani završeci redaka i uklonjeni završni razmaci iz loga;
sadržaj poruka ostao je isti. Izvorno očitanje ostaje lokalno u `tmp/`.
PSRAM je potvrđen bootom postojeće aplikacije, ne NEKTAR-P4 testom;
ovo nije stres-test memorije ni potvrda viših PSRAM taktova.

## Postupak i postojeći sadržaj

Korišten esptool 5.3.1, COM6, 115200 baud, `--no-stub flash-id`.
Nakon toga uhvaćen je osamsekundni UART boot log uz reset preko RTS-a.
Nisu izvođeni erase, write-flash ni promjene eFuseova.

Na uređaju je `apta_cooperative_scheduler`, verzija
`v1.0.1-238-g046cb04`, ESP-IDF 6.0.2, build 2026-08-30.
Pri bootu pokreće UAC device i čeka PCM podatke. Zadržan je postojeći firmware.
Njegov image header deklarira samo 2 MB flasha; runtime upozorava da stvarni
flash ima 16 MB. To je konfiguracija stare slike, ne manjak fizičke memorije.

## Korekcija NEKTAR-P4 konfiguracije

Prvotni IDF default za reviziju 3.x nije kompatibilan s ovim v1.3 čipom.
`sdkconfig.defaults` sada eksplicitno bira:

```text
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
```

Podržani raspon tako postaje v1.0-v1.99. PSRAM u NEKTAR-P4 M0 ostaje isključen.
Generirani stari `sdkconfig` spremljen je u `tmp/sdkconfig-before-com6`;
novi je generiran iz ispravljenih defaults. Rezultat builda je u
[validaciji](M0_VALIDATION.md).

## Otvoreno

Silicijska revizija nije isto što i PCB revizija: fizička oznaka carrier ploče
još nije potvrđena. Serijska provjera ne potvrđuje ES8311 ACK/PA polaritet,
USB napajanje GX49, C6 veze niti audio put. Za to slijedi BSP/audio bring-up.
