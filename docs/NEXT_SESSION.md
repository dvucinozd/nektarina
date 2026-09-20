# Sljedeći koraci

**Datum ažuriranja:** 2026-09-20

## Trenutačno stanje

- Potvrđeni MAX98357A pinout: BCLK=GPIO45, WS/LRC=GPIO3, DIN=GPIO47, SD_MODE=GPIO14; GAIN nije spojen.
- Kanonski audio format: 44.1 kHz, 16-bit stereo I2S.
- Produkcijski boot je tih; kratki 440 Hz dijagnostički ton dostupan je kroz `CONFIG_NEKTARINA_BOOT_TEST_TONE` i zadano je isključen.
- Firmware pokreće MIDI queue i synth engine prije USB hosta; synth task je jedini stalni I2S writer.
- USB host prihvaća samo Audio/MIDIStreaming bulk-IN interface i odgađa cleanup dok transfer više nije aktivan.
- ESP-IDF 6.0.2 build prolazi. Hardverska provjera nove integracije još nije provedena.

## Sljedeća fizička provjera

1. Flashati aktualni firmware na ESP32-S3.
2. Potvrditi tihi boot bez USB MIDI uređaja i provjeriti heartbeat telemetriju.
3. Spojiti Nektar klavijaturu i provjeriti Note On/Off, velocity, pitch bend i CC kontrole.
4. Odspojiti klavijaturu tijekom aktivne note i potvrditi All Notes Off bez crasha.
5. Napraviti najmanje 25 hot-plug ciklusa te 30-minutni soak test.
6. Zabilježiti MIDI drop count, I2S error/short-write brojače, heap i stack watermarke.

Detaljni kriteriji i preostali rad nalaze se u [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).
