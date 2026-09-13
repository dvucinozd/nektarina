# Sadržaj repozitorija i lokalni izvori

Repozitorij sadrži NEKTAR-P4 kod, konfiguraciju, planove i odabrane dokaze.
Primarni framework je ESP-IDF 6.0.2; dobavljački demo projekti nisu build dependency.

## Lokalno, izvan Gita

- `DEVICES_MANUALS/`: paket dobavljača JC-ESP32P4-M3-DEV0 i Nektar PDF.
  U ovom workspaceu to je stvarno ime mape. Ignorirana je i alternativna
  putanja `DEVICES/_MANUALS/` koju je korisnik naveo.
- `AGENTS.md`: lokalne upute agentu, izričito isključene po zahtjevu korisnika.
- `build*/`, `sdkconfig`, `sdkconfig.old`, `managed_components/`, `tmp/`:
  generirani build/dependency sadržaj i privremene datoteke.
- `backups/`: buduće kopije stvarnog firmwarea; ne objavljivati ih u repozitorij.

Klon repozitorija dovoljan je za build uz instalirani IDF, ali ne sadrži
vendor priručnike. Za ponovno detaljno provjeravanje hardvera lokalno vratiti
isti paket dokumentacije u `DEVICES_MANUALS/`. Popis točnih korištenih izvora
i putanja nalazi se u [M0 auditu](M0_HARDWARE_BSP_AUDIT.md).

Dobavljački paket uključuje tuđe primjere, alate i generirane konfiguracije;
ne tretirati ih kao naš izvorni kod ni automatski prenositi u repozitorij.
Prije dodavanja vanjskog drivera zapisati verziju/commit, izvor i njegovu licencu.

## Objavljivanje

Pregledati staging i pokrenuti `git diff --cached --check`. Build je potreban
za promjene koda/configa; za dokumentacijski checkpoint koristiti postojeći
potvrđeni build ako se kod nije promijenio. Nakon pusha usporediti lokalni HEAD
s `git ls-remote origin refs/heads/main`. Ne commitati lokalne backupove ili tajne.
