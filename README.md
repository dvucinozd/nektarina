# NEKTAR-P4

Samostalni ESP32-P4 USB-MIDI synthesizer za Nektar Impact GX49.

Arhitektura: [izvorni plan](ESP32-P4_MIDI_SYNTH_PLAN.md).
M1 funkcionalno završen; sljedeći korak je [M2 integracija USB hosta](docs/M2_USB_HOST_ADOPTION_PLAN.md).

Za nastavak prvo pročitaj [trenutno stanje](docs/STATE_SUMMARY.md) i
[sljedeću sesiju](docs/NEXT_SESSION.md). M1 kriteriji su u
[audio bring-up planu](docs/M1_AUDIO_BRINGUP_PLAN.md).

Projekt sada sadrži BSP i M1 audio HAL. Boot provjerava ES8311 i šalje tišinu
preko I2S-a s isključenim pojačalom. Ton se uključuje serijskom naredbom.
[M1 naredbe i rezultati](docs/M1_AUDIO_TEST.md). USB, C6 i SD nisu pokrenuti.
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

Na COM6 potvrđen je P4 v1.3; defaults su usklađeni na rev. 1.0-1.99 i 360 MHz.
Instaliran je M1 `0.1.1-m1-mic`: ton i mikrofon potvrđeni, izmjereno 440.001 Hz
u snimci. Aktualni image prošao je 602.020 s tona bez zabilježenih softverskih grešaka.
Granice dokaza i aktualni hash: [M1 rezultati](docs/M1_AUDIO_TEST.md).
Povijest: [M0 validacija](docs/M0_VALIDATION.md).
