# Trenutno stanje - ESP32-S3-WROOM-1-N16R8 Migracija

## Cilj i aktivna faza
Samostalni USB-MIDI synthesizer: Nektar klavijatura -> USB Host (Full-Speed OTG GPIO 19/20) -> MIDI Queue -> Polyphonic Synth Engine (Core 1) -> I2S MAX98357A (BCLK=45, WS=3, DOUT=47, SD_MODE=14).

**Status:**
- Uspješno migrirano s eksperimentalne P4 platforme na produkcijski ESP32-S3 (N16R8).
- Uklonjen P4 specifičan BSP (`board_jc_esp32p4_m3`).
- Audio podsustav refaktoriran na MAX98357A čist I2S mono izlaz (bez I2C/registara).
- Implementiran moderni USB MIDI Host stog s podrškom za hot-plug.
- Implementiran hibridni synth engine: 16-glasovni 80s Virtual Analog + TinySoundFont SF2 u PSRAM-u.
- Zabilježene striktne zabrane: GPIO 33–37 zabranjeni zbog interne Octal SPI sabirnice.
- Hardverski potvrđeni boot na 240 MHz, Nektar USB MIDI enumeracija, Note On/Off, velocity te reconnect/hot-unplug tijekom aktivne note; osnovni test nema MIDI dropova ni I2S grešaka.

## Hardverska identifikacija
- **MCU:** ESP32-S3 Dual-Core Xtensa LX7 @ 240 MHz
- **Memorija:** 16 MB Flash DIO/80 MHz, 8 MB Octal PSRAM/80 MHz
- **Audio:** MAX98357A: BCLK=GPIO45, WS=GPIO3, DOUT=GPIO47, SD_MODE=GPIO14, GAIN nije spojen, 44.1 kHz
- **USB:** Ugrađeni USB-OTG PHY: D-=GPIO19, D+=GPIO20

## Dokumentacija
- [M0-S3 Hardware & Architecture Audit](M0_S3_MIGRATION_AUDIT.md)
- [Izvorni M1 Audio Bringup Plan (P4 referenca)](M1_AUDIO_BRINGUP_PLAN.md)
- [M2 USB MIDI hardverski test](M2_USB_MIDI_TEST.md)
