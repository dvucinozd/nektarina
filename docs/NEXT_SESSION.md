# Nastavak nakon završenog M1 - 2026-09-13

## Prva radnja

Pregledati Git stanje i [STATE_SUMMARY](STATE_SUMMARY.md), zatim
[M2 plan integracije](M2_USB_HOST_ADOPTION_PLAN.md). M1 0.1.1-m1-mic na COM6
je kvalificirani funkcionalni baseline, utišan, PA off i DMA stop.
Točan hash, završni soak i ograničenja mjerenja su u [M1 izvještaju](M1_AUDIO_TEST.md).

## Sljedeći razvojni korak

Izdvojiti minimalni host service i MIDI client/adapter iz postojećeg
`D:/AI/PAJONIIIR-M3`, referentni commit
`ccebdb96e302d8be8bf2b1269ee8afdad583e430`. Izvor je pregledan bez izmjena;
nije još kopiran niti integriran. Točne datoteke i testovi su u M2 planu.

1. Provjeriti izvorni commit/stanje i lokalne upute prije prijenosa.
2. Zadržati host lifecycle; prilagoditi FLX4 device filter i MIDI callback
   GX49 adapteru, bez UAC/MSC/LED/control_link i tuđih BSP pretpostavki.
3. Build za P4 v1.3 i ciljani descriptor/lifecycle testovi.
4. Prije fizičkog M2 testa potvrditi USB priključak, kabel/adapter i napajanje
   GX49; zatim snimiti stvarne deskriptore i connect/disconnect/reconnect.
5. Spremiti mjerljive brojače/dokaze. Parser i synth note pripadaju M3/M4.

## Što ne treba ponavljati

- Backup imagea korisnik već ima. Ton i uredne prijelaze već je potvrdio.
- Završni desetominutni soak aktualne mic verzije dovršen je; ne ponavljati ga
  bez nove promjene, greške ili razloga za regresiju.
- Ne flashati isti image bez potrebe. Boot i dalje treba biti tih.
- Ne preuzimati pinove, napajanje ili PSRAM postavke iz drugog projekta.

PCB fizička oznaka i neovisna clock kalibracija ostaju nezabilježeni.
C6, SD, SoundFont, Wi-Fi i OTA nisu dio sljedećeg koraka.
