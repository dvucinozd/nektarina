# Sadržaj Repozitorija i Struktura Projekta (nektarina)

Repozitorij sadrži firmware, komponente i dokumentaciju za samostalni ESP32-S3 USB-MIDI sintetizator.

## Struktura Koda

```text
├── CMakeLists.txt              # Vršni CMake konfiguracijski file (target esp32s3)
├── sdkconfig.defaults          # Postavke za 16MB Flash, 8MB Octal PSRAM, USB Host, FreeRTOS 1000Hz
├── AGENTS.md                   # Vodič i pravila za nastavak razvoja s AI agentima
├── WIRING_DIAGRAM.md           # Hardverska shema spajanja i pinout (MAX98357A, USB, zvučnik)
├── main/
│   ├── CMakeLists.txt          # Registracija aplikacije
│   ├── Kconfig.projbuild       # Opcionalni, zadano isključeni boot test-ton
│   ├── app_main.c              # Audio/MIDI/synth boot sekvenca i telemetrija
│   └── idf_component.yml       # Upravljane ovisnosti (espressif/usb)
├── components/
│   ├── audio_hal/              # I2S master driver za MAX98357A mono pojačalo
│   │   ├── CMakeLists.txt
│   │   ├── include/audio_hal.h
│   │   └── audio_hal.c
│   ├── usb_midi_host/          # USB Host MIDI stog s paketnim parserom i hot-plugom
│   │   ├── CMakeLists.txt
│   │   ├── include/usb_midi_host.h
│   │   └── usb_midi_host.c
│   └── synth_engine/           # 16-glasovni 80s Virtual Analog + TinySoundFont (PSRAM)
│       ├── CMakeLists.txt
│       ├── include/synth_engine.h
│       ├── include/tsf.h
│       └── synth_engine.c
└── docs/                       # Arhitektura, auditi, verifikacija i testni rezultati
    ├── M0_S3_MIGRATION_AUDIT.md
    ├── M0_COM12_S3_CHECK.md
    ├── M0_VALIDATION.md
    ├── M1_AUDIO_BRINGUP_PLAN.md
    ├── M1_AUDIO_TEST.md
    ├── M2_USB_MIDI_TEST.md
    ├── M2_USB_HOST_ADOPTION_PLAN.md
    ├── NEXT_SESSION.md
    ├── REPOSITORY_CONTENTS.md
    └── STATE_SUMMARY.md
```

## Pravila i Ignorirane Datoteke (`.gitignore`)
- `build*/`: Privremeni binarni artefakti
- `sdkconfig`: Lokalna generirana Kconfig konfiguracija
- `managed_components/`: Automatski preuzete komponente (IDF Component Manager)
- `tmp/`, `backups/`: Privremene datoteke
