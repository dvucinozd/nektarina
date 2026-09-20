# Sljedeći koraci

**Datum ažuriranja:** 2026-09-20

## Trenutačno stanje

- Potvrđeni MAX98357A pinout: BCLK=GPIO45, WS/LRC=GPIO3, DIN=GPIO47, SD_MODE=GPIO14; GAIN nije spojen.
- Kanonski audio format: 44.1 kHz, 16-bit stereo I2S.
- Produkcijski boot je tih; kratki 440 Hz dijagnostički ton dostupan je kroz `CONFIG_NEKTARINA_BOOT_TEST_TONE` i zadano je isključen.
- Firmware pokreće MIDI queue i synth engine prije USB hosta; synth task je jedini stalni I2S writer.
- USB host prihvaća samo Audio/MIDIStreaming bulk-IN interface i odgađa cleanup dok transfer više nije aktivan.
- ESP-IDF 6.0.2 build i flash na COM3 prolaze; boot potvrđuje CPU 240 MHz, DIO/80 MHz flash i 8 MB Octal PSRAM/80 MHz.
- Nektar VID `2467`, PID `2033` enumerira se preko MIDIStreaming bulk-IN endpointa `0x81`.
- Note On/Off i velocity potvrđeni su uživo. Reconnect i hot-unplug tijekom aktivne note završavaju čistim All Notes Off bez crasha, dropova ili I2S grešaka.

## Sljedeća fizička provjera

1. Akustički potvrditi zvuk, glasnoću i odsutnost neželjenog tona pri tihom bootu.
2. Provjeriti pitch bend, mod wheel i podržane CC kontrole.
3. Napraviti najmanje 25 hot-plug ciklusa te 30-minutni soak test.
4. Tijekom testa pratiti MIDI drop count, I2S error/short-write brojače, heap i stack watermarke.

Osnovni hardverski rezultat zabilježen je u [M2_USB_MIDI_TEST.md](M2_USB_MIDI_TEST.md).

Detaljni kriteriji i preostali rad nalaze se u [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).
