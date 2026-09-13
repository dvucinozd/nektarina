# Nastavak 2026-09-13 - prvi korak M1

## Prva radnja

Pregledati Git stanje i [STATE_SUMMARY](STATE_SUMMARY.md), zatim provjeriti je li
isti P4 još na COM6. **Prije prvog flasha napraviti backup svih 16 MB postojećeg
flasha u lokalni ignorirani `backups/`.** Zapisati alat, datum, veličinu i SHA256.
Ponovnim očitanjem ili provjerom prema uređaju potvrditi sadržaj backupa.
Ne prepisati jedinu kopiju postojećeg firmwarea.

## Redoslijed rada

1. Potvrditi uređaj, PCB oznaku, povezani zvučnik i napajanje.
2. Izraditi i provjeriti backup. Postojeći firmware je APTA testna aplikacija.
3. Implementirati minimalni I2C/ES8311 probe s pojačalom u isključenom stanju.
4. Nastaviti prema [M1_AUDIO_BRINGUP_PLAN](M1_AUDIO_BRINGUP_PLAN.md).
5. Zapisati stvarni flash identitet, rezultate i neriješene točke.

## Što ne treba ponavljati

- Ne pretpostavljati rev3.x: silicij v1.3 je već očitan s uređaja.
- Ne pretraživati druge projekte radi GPIO-a; lokalni M0 audit već ima izvore.
- Ne prikazivati postojeći PSRAM test kao naš audio/runtime PASS.
- Ne pokretati M2, Wi-Fi, SoundFont ili OTA dok ne dođu na red.

## Potrebno od korisnika tijekom razvoja

- Fizička oznaka ploče i potvrda priključenog zvučnika/napajanja ako nisu vidljivi.
- Slušna potvrda 440 Hz tona pri kontroliranoj niskoj glasnoći.
- Za kasniji M2: putanja postojećeg USB Host rješenja i USB/GX49 povezivanje.

Nije postavljen automatski podsjetnik; nastavak počinje novim korisnikovim zahtjevom.
