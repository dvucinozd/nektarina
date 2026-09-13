# Nastavak nakon M1 audio/mikrofon testa - 2026-09-13

## Prva radnja

Pregledati Git stanje i [STATE_SUMMARY](STATE_SUMMARY.md). M1 `0.1.1-m1-mic`
je na COM6, utišan i s DMA stop. Korisnik je potvrdio postojeći backup.
**Pregledati rezultate i odlučiti o sljedećem razvojnom koraku s korisnikom.**
Funkcionalni ton/prijelazi i mikrofon potvrđeni su; ne tražiti iste potvrde ponovno.
Soak 601.970 s pripada 0.1.0-m1. Ako mic verzija ostaje baseline, ponoviti soak
na njoj. Sljedeća razvojna faza M2 traži lokaciju postojećeg USB Host izvora/API-ja.

## Redoslijed rada

1. Pročitati [M1 izvještaj](M1_AUDIO_TEST.md), uključujući hash granice dokaza.
2. Ne ponavljati flash bez promjene koda; boot ostaje tih.
3. Za mic capture prvo `s`, zatim `r` za tišinu ili `t` pa `r` za ton.
4. Zatvoriti preostale kriterije ili prijeći na korisnički odobrenu M2 integraciju.
5. Zapisati stvarni flash identitet, rezultate i neriješene točke.

## Što ne treba ponavljati

- Ne pretpostavljati rev3.x: silicij v1.3 je već očitan s uređaja.
- Ne pretraživati druge projekte radi GPIO-a; lokalni M0 audit već ima izvore.
- Ne prikazivati postojeći PSRAM test kao naš audio/runtime PASS.
- Ne pokretati M2, Wi-Fi, SoundFont ili OTA dok ne dođu na red.

## Potrebno od korisnika tijekom razvoja

- Fizička oznaka/revizija PCB-a još nije potvrđena; zvučnik, napajanje i slušni test jesu.
- Za kasniji M2: putanja postojećeg USB Host rješenja i USB/GX49 povezivanje.

Nije postavljen automatski podsjetnik; nastavak počinje novim korisnikovim zahtjevom.
