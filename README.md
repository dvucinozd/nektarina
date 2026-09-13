# NEKTAR-P4

Samostalni ESP32-P4 USB-MIDI synthesizer za Nektar Impact GX49.

Arhitektura: [izvorni plan](ESP32-P4_MIDI_SYNTH_PLAN.md).
Trenutni korak: [M0 hardware/BSP audit](docs/M0_HARDWARE_BSP_AUDIT.md).

Za nastavak prvo pročitaj [trenutno stanje](docs/STATE_SUMMARY.md) i
[sljedeću sesiju](docs/NEXT_SESSION.md). M1 je pripremljen u
[audio bring-up planu](docs/M1_AUDIO_BRINGUP_PLAN.md).

M0 sadrži pasivni board component i minimalni ESP-IDF projekt. Boot samo
ispisuje profil; ne pokreće codec, I2S, USB, C6, SD ni pojačalo.
Dokumentacija dobavljača ostaje lokalno u `DEVICES_MANUALS` i nije dio Gita ni builda.
Pravila sadržaja: [repozitorij i lokalni izvori](docs/REPOSITORY_CONTENTS.md).

## Build (PowerShell, instalirani ESP-IDF 6.0.2)

```powershell
. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1
idf.py build
```

M0 koristi internu memoriju i početnu tvorničku particijsku tablicu ESP-IDF-a.
Ovo nije OTA konfiguracija. PSRAM, frekvencije, OTA particije i periferije
uvode se u pripadajućim fazama uz provjeru stvarnog hardvera.

**Build je provjera kompilacije, ne slika odobrena za ovu fizičku ploču.**
Na COM6 potvrđen je P4 v1.3; defaults su usklađeni na rev. 1.0-1.99 i 360 MHz.
Detalji: [serijska provjera](docs/M0_COM6_CHECK.md). Firmware nije flashan.
Rezultat: [M0 validacija](docs/M0_VALIDATION.md).
