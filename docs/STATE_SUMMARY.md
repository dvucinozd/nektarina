# Trenutno stanje - 2026-09-12

## Cilj i aktivna faza

Samostalni USB-MIDI synthesizer: Nektar GX49 -> postojeći USB Host -> MIDI queue
-> synth -> I2S/ES8311. Prvi cilj: stabilna 16-glasovna sinteza na 48 kHz.

**M0 dokumentacijski i build dio je završen. Serijska identifikacija je završena.
Fizički M0 je djelomično otvoren. M1 audio bring-up nije započet.**
Korisnik je nastavak hardverskog razvoja odgodio za sutra (2026-09-13).

## Što postoji

- Minimalni ESP-IDF 6.0.2 projekt i pasivni `board_jc_esp32p4_m3` component.
- Shematski potkrijepljeni audio/SD pinovi, rezervacije za C6 i popis konflikata.
- M0 aplikacija samo ispisuje board profil; ne inicijalizira periferije.
- Ispravljena konfiguracija: P4 v1.0-v1.99, 360 MHz, 16 MB flash DIO/80 MHz.
- PSRAM je u našem M0 buildu isključen; nema codec/I2S/USB/Wi-Fi drivera.
- Build prolazi; SHA i veličina su u [M0_VALIDATION](M0_VALIDATION.md).

## Stvarni uređaj

COM6: ESP32-P4 silicij **v1.3**, flash **16 MB**. Postojeći firmware prepoznaje
**32 MB PSRAM-a**, prolazi početni test na **20 MHz** i radi na **360 MHz**.
To nije potvrda PSRAM-a ili runtimea našeg firmwarea.

Instalirano: `apta_cooperative_scheduler`, `v1.0.1-238-g046cb04`, ESP-IDF 6.0.2.
Njegov image header deklarira 2 MB flasha, iako čip ima 16 MB.
Izvedeni su ROM/flash očitanje i reset radi boot loga; **nije ništa flashano**.
Backup firmwarea još nije napravljen.

## Otvorene provjere

| Stavka | Sljedeća radnja |
| --- | --- |
| PCB revizija | Potvrditi fizičku oznaku; silicijska v1.3 nije PCB revizija |
| ES8311 i PA | I2C ACK na 7-bit 0x18, polaritet GPIO11, kontroliran start |
| Audio | 440 Hz / 48 kHz, DMA, glasnoća, underrun i slušni test |
| Napajanje | 5 V/3V3, speaker i način USB napajanja GX49 |
| USB Host | Dobiti lokaciju postojećeg izvornog koda/API-ja prije M2 |
| C6/SD | C6 veze/firmware i TF napajanje potvrditi prije M7/M10 |

## Dokumenti

- [Handoff za sljedeću sesiju](NEXT_SESSION.md)
- [M1 plan i kriteriji](M1_AUDIO_BRINGUP_PLAN.md)
- [Hardware/BSP audit](M0_HARDWARE_BSP_AUDIT.md)
- [COM6 provjera](M0_COM6_CHECK.md)
- [Build validacija](M0_VALIDATION.md)
- [Lokalni izvori i pravila repozitorija](REPOSITORY_CONTENTS.md)

Repo: `https://github.com/dvucinozd/nektar-midi.git`, privatni repozitorij.
Točan objavljeni commit provjerava se kroz Git; ovaj dokument ne nosi vlastiti SHA.
