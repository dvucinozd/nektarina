# Trenutno stanje - 2026-09-13

## Cilj i aktivna faza

Samostalni USB-MIDI synthesizer: Nektar GX49 -> postojeći USB Host -> MIDI queue
-> synth -> I2S/ES8311. Prvi cilj: stabilna 16-glasovna sinteza na 48 kHz.

**M0 dokumentacijski i build dio je završen. Serijska identifikacija je završena.
Fizički M0 je djelomično otvoren. M1 audio put, slušni test i mikrofon su potvrđeni.
Soak 601.970 s prolazi na 0.1.0-m1; noviji mic image ima zaseban kratki smoke.**
Korisnik je odobrio nastavak 2026-09-13 i potvrdio postojeći backup.

## Što postoji

- Minimalni ESP-IDF 6.0.2 projekt i pasivni `board_jc_esp32p4_m3` component.
- Shematski potkrijepljeni audio/SD pinovi, rezervacije za C6 i popis konflikata.
- M1 aplikacija inicijalizira ES8311 i I2S te počinje tišinom, s PA off.
- Ispravljena konfiguracija: P4 v1.0-v1.99, 360 MHz, 16 MB flash DIO/80 MHz.
- PSRAM ostaje isključen; codec driver esp_codec_dev 1.6.2 je pinan lock datotekom.
- Nema USB/Wi-Fi integracije. Aktualni build i flash: [M1_AUDIO_TEST](M1_AUDIO_TEST.md).

## Stvarni uređaj

COM6: ESP32-P4 silicij **v1.3**, flash **16 MB**. Prethodni APTA firmware prepoznaje
**32 MB PSRAM-a**, prolazi početni test na **20 MHz** i radi na **360 MHz**.
To nije potvrda PSRAM-a ili runtimea našeg firmwarea.

Instalirano: `nektar_p4` / `0.1.1-m1-mic`, ESP-IDF 6.0.2, flash header 16 MB.
Prethodno je bio `apta_cooperative_scheduler` / `v1.0.1-238-g046cb04`;
korisnik ima backup tog imagea, koji agent nije pregledao niti ponovno izrađivao.
Aktualni flash hash je u M1 izvještaju. Izlaz utišan, DMA zaustavljen, COM6 slobodan.
Mikrofon: ton 440.001 Hz relativno prema nominalnom sample rateu, bez clippinga;
komponenta 440 Hz je 59.35 dB iznad utišane snimke. ADC/DAC dijele clock.

## Otvorene provjere

| Stavka | Sljedeća radnja |
| --- | --- |
| PCB revizija | Potvrditi fizičku oznaku; silicijska v1.3 nije PCB revizija |
| ES8311 i PA | ACK/format i aktivno-visoki PA funkcionalno potvrđeni |
| Audio | Slušno čist ton/prijelazi, soak starog imagea PASS; mic image kratki smoke PASS |
| Mjerenja | Apsolutni clock treba neovisnu referencu; mic nije neovisna kalibracija |
| Napajanje | 5 V/3V3, speaker i način USB napajanja GX49 |
| USB Host | Dobiti lokaciju postojećeg izvornog koda/API-ja prije M2 |
| C6/SD | C6 veze/firmware i TF napajanje potvrditi prije M7/M10 |

## Dokumenti

- [Handoff za sljedeću sesiju](NEXT_SESSION.md)
- [M1 plan i kriteriji](M1_AUDIO_BRINGUP_PLAN.md)
- [M1 implementacija, naredbe i dokazi](M1_AUDIO_TEST.md)
- [Hardware/BSP audit](M0_HARDWARE_BSP_AUDIT.md)
- [COM6 provjera](M0_COM6_CHECK.md)
- [Build validacija](M0_VALIDATION.md)
- [Lokalni izvori i pravila repozitorija](REPOSITORY_CONTENTS.md)

Repo: `https://github.com/dvucinozd/nektar-midi.git`, privatni repozitorij.
Točan objavljeni commit provjerava se kroz Git; ovaj dokument ne nosi vlastiti SHA.
