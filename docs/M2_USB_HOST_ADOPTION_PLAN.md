# M2 - integracija postojećeg USB hosta

Status 2026-09-13: izvor pronađen i pregledan; integracija još nije implementirana.
Korisnik je naveo `D:/AI/PAJONIIIR-M3`. Pregled je bio samo za čitanje.
Referentni commit: `ccebdb96e302d8be8bf2b1269ee8afdad583e430`.
Prije preuzimanja provjeriti taj commit i lokalne promjene izvora.

## Što preuzimamo i prilagođavamo

Putanje u tablici su relativne prema `D:/AI/PAJONIIIR-M3`.

| Izvor | Uloga u NEKTAR-P4 |
| --- | --- |
| `firmware/main-deck-p4/components/p4_flx4_host/p4_flx4_host.c` | Polazište USB MIDI clienta: registracija, enumeracija, IN transfer, readiness, ograničeni retry i odgođeni disconnect cleanup |
| `firmware/main-deck-p4/components/p4_flx4_host/include/p4_flx4_host.h` | Referenca API-ja; novi adapter izlaže MIDI ulaz i stanje veze, bez FLX4 audio/LED API-ja |
| `firmware/main-deck-p4/components/usb_storage/usb_storage.c` (`usb_lib_task`) | Postojeća instalacija host biblioteke i obrada library događaja; izdvojiti od MSC/recovery logike |
| `firmware/main-deck-p4/components/usb_storage/idf_component.yml` | Referenca pinanih verzija: ESP-IDF 6.0.2 i `espressif/usb` 1.5.0; MSC 1.2.0 nije potreban za M2 |
| `firmware/main-deck-p4/cmake/apply_espressif_usb_fifo_patch.cmake` | Obvezan pregled pri izdvajanju: M3 raspodjela FIFO-a služi istodobnom HS MSC i FS UAC/MIDI prometu; opravdati konfiguraciju za NEKTAR MIDI-only workload |
| `firmware/main-deck-p4/components/usb_storage/usb_dwc_hal_compat.c` | Apache-2.0 HAL wrapper za BNA bez CHHLTD; prije preuzimanja provjeriti primjenjivost i linker wrap na pinanom IDF-u |
| `firmware/main-deck-p4/components/p4_flx4_host/p4_flx4_midi_gate.c` | Referenca generation gatea za odbacivanje MIDI poruka iz prethodne sesije |
| `tests/p4_flx4_host/test_p4_flx4_lifecycle.c` i pripadajući stubovi | Osnova ciljanih testova stvarnih callbackova za allocation/submit/claim failure, bounded retry i disconnect cleanup |

M3 root licenca je MIT; pojedine datoteke imaju zasebnu licencu (HAL wrapper
navodi Apache-2.0). Pri stvarnom prijenosu izvora sačuvati pripadajuće
copyright/licencne obavijesti i zapisati podrijetlo promjena. Pregledani testovi
nisu u ovoj sesiji pokretani; njihovo postojanje nije NEKTAR validation PASS.

## Potrebne prilagodbe

1. Postojeći client prihvaća FLX4 VID/PID `2b73:0045`. Najprije zapisati stvarni
   GX49 device/configuration descriptor, VID/PID, interface/alternate setting,
   MIDIStreaming class/subclass, bulk IN endpoint i maksimalnu veličinu paketa.
   Identitet GX49 ne pogađati iz naziva uređaja.
2. Zadržati transportni lifecycle; ukloniti FLX4 UAC interface 1/alt 2,
   audio OUT, LED mapiranje i `control_link` ovisnost. `in_transfer_cb` predaje
   sirove četverobajtne USB-MIDI pakete adapteru, uz cable/CIN i timestamp.
   Sintetizator se ne poziva iz USB callbacka. Semantički parser pripada M3 fazi.
3. Validirati duljine deskriptora, vrstu endpointa, granice paketa i kapacitet
   transfer buffera pri svakom spajanju. Postojeći FLX4 buffer se zadržava među
   sesijama; različit MPS ne smije dovesti do upisa izvan alokacije.
4. Readiness znači uspješan interface claim i aktivan IN transfer, odvojeno od
   prisutnosti uređaja. Na canceled/no-device callbacku zaustaviti resubmit;
   nakon DEV_GONE pričekati povrat transfera i uspješan interface release prije
   zatvaranja handlea i obrade novog uređaja.
5. Adapter ima unaprijed ograničenu izlaznu queue, drop/error brojače i session
   identitet. Disconnect poništava stare pakete; M3 parser/synth kasnije mora
   dobiti reset stanja da ne ostanu aktivne note. Callback ne čeka audio task.
6. Host service instalirati samo jednom, s odabranim kontrolerom iz lokalnog
   BSP-a/audita. Ne preuzimati M3 dvostruki `peripheral_map`, PHY/power GPIO,
   PCM5102, C6, display ili SD konfiguraciju. Pregledati ostale M3 USB build
   transformacije i `usb_dwc_hal_compat.c` prije odluke što adapter treba.

## Prvi izvedbeni korak

Izdvojiti minimalni host service i MIDI client/adapter iz navedenih izvora u
NEKTAR component, pinati ovisnost i dodati ciljane lifecycle/descriptor testove.
Prvo ponašanje je enumeracija i dijagnostički prijem sirovih MIDI paketa uz
postojeći tihi M1 audio izlaz. Build mora ostati za silicij v1.3.

Prije hardware testa potvrditi točan USB priključak na NEKTAR ploči i način
napajanja GX49. Logički root-port power API nije dokaz fizički upravljivog VBUS-a;
lokalni audit zasad ne identificira VBUS GPIO switch. Ne napajati prema M3
pretpostavkama. GX49 treba biti fizički priključen za dokaz deskriptora/prijema.

## Kriteriji zatvaranja M2

- Build i ciljane provjere adaptera prolaze; dokumentiran točan flash identitet.
- Stvarni GX49 descriptor i uspješno preuzeti IN paketi spremljeni u evidence.
- Connect, disconnect i reconnect vraćaju readiness bez stale handlea/paketa.
- Ponovljeni ciklusi i kontinuirani MIDI promet ne povećavaju neočekivane
  transfer/queue/audio fault brojače; prijaviti trajanje, broj ciklusa i delte.
- Audio zadržava M1 ograničenja. Sviranje prve note i key-to-audio latencija
  nisu M2 rezultat; dolaze nakon parsera i prvog synth glasa.

M2 se ne označava završenim na temelju M3 testova ili samog builda.
