# NEKTARINA — implementacijski plan popravaka

**Datum plana:** 2026-09-19
**Cilj:** dovesti projekt od trenutačnog dijagnostičkog firmwarea do pouzdanog ESP32-S3 USB-MIDI sintetizatora, uz reproducibilan build i dokazanu hot-plug stabilnost.

## 1. Trenutačna polazna točka

- Projekt se uspješno gradi s ESP-IDF 6.0.2 iz `C:\esp\v6.0.2\esp-idf`.
- Generirani firmware koristi 16 MB flash u DIO/80 MHz načinu i Octal PSRAM na 80 MHz.
- `app_main` trenutačno pokreće beskonačni dijagnostički ton kao jedinog vlasnika I2S izlaza.
- MIDI red i USB host se pokreću, ali `synth_engine_init()` i `synth_engine_start()` nisu pozvani, pa se MIDI poruke ne pretvaraju u zvuk.
- Potvrđeni kanonski MAX98357A I2S pinout je `BCLK=GPIO45`, `WS/LRC=GPIO3`, `DOUT=GPIO47`; dokumentacija još navodi zastarjele `GPIO 16/17/18`.
- Potvrđena kontrolna veza MAX98357A je `SD_MODE=GPIO14`; `GAIN` nije spojen i firmware ne smije upravljati GPIO21 kao GAIN signalom.
- Postoje dodatne proturječnosti između koda i dokumentacije:
  - kod: 16 kHz, dokumentacija: 44,1 kHz;
  - kod ispravno upravlja `SD_MODE` na GPIO 14, ali pogrešno upravlja GPIO21 kao nespojenim `GAIN` signalom;
  - dokumentacija flash naziva Octal/OPI, a efektivni build koristi DIO.

## 2. Pravila izvedbe

1. Svaka faza mora završiti čistim buildom i zasebnim, preglednim diffom.
2. U svakom trenutku samo jedan task smije pisati u I2S kanal.
3. Hardverski pinovi ne mijenjaju se dok se ne potvrdi stvarno ožičenje ploče.
4. USB resurs se ne smije osloboditi dok je transfer aktivan ili callback još može pristupiti resursu.
5. Dokumentacija smije tvrditi samo ono što je dokazano buildom ili hardverskim testom; povijesni rezultati moraju biti jasno označeni.
6. Nakon svake funkcionalne faze provjeriti da Git diff ne sadrži automatske ili nepovezane promjene.

## 3. Faza 0 — dovršavanje hardverskog izvora istine

### Posao

- MAX98357A I2S pinovi potvrđeni su 2026-09-19: `BCLK=GPIO45`, `LRC/WS=GPIO3`, `DIN=GPIO47`.
- MAX98357A kontrolna veza potvrđena je 2026-09-19: `SD_MODE=GPIO14`; `GAIN` nije spojen.
- Dokumentirati da nepovezani GAIN ostaje na hardverskom defaultu MAX98357A i ne pripada nijednom MCU GPIO-u.
- Potvrditi točnu oznaku ESP32-S3 modula i 16 MB flash / 8 MB PSRAM konfiguraciju.
- Kanonski audio sample rate je 44,1 kHz. Prije završnog prihvata izmjeriti opterećenje i underrunove sa svih 16 glasova; niži rate koristiti samo kao dokumentirani fallback ako mjerenje pokaže da je nužan.
- Rezultat zapisati u jednu kratku tablicu hardverske konfiguracije koja će postati izvor za kod i dokumentaciju.

### Kriterij prihvata

- Nema otvorenih proturječnosti o I2S pinovima, SD/GAIN vezama, flash načinu ili sample rateu.
- Dokumentirati da potvrđeni I2S pinovi uključuju strapping GPIO 3 i GPIO 45 te provjeriti da vanjski MAX98357A sklop ne remeti njihove boot razine.

## 4. Faza 1 — konfiguracija i reproducibilan build

### Datoteke

- `sdkconfig.defaults`
- `dependencies.lock`
- po potrebi novi `Kconfig.projbuild`
- build upute u `README.md` i `AGENTS.md`

### Posao

- Ukloniti neučinkoviti `CONFIG_ESPTOOLPY_FLASHMODE_OPI=y` i eksplicitno postaviti potvrđeni flash način, trenutno DIO/80 MHz/16 MB.
- Zadržati potvrđeni Octal PSRAM/80 MHz način.
- Namjerno regenerirati i prihvatiti odgovarajuću verziju `dependencies.lock` za ESP-IDF 6.0.2, umjesto da je svaki build iznova mijenja.
- Dodati Kconfig opciju za jednokratni boot test-ton, zadano isključenu u produkcijskom buildu.
- Ispraviti lokalne build putanje na `C:\esp\v6.0.2\esp-idf`; ne ugrađivati korisničku Python putanju kao obveznu projektnu postavku.
- Dokumentirati offline ponašanje upravitelja komponenti i postojeću zaključanu verziju `espressif/usb`.

### Provjera

- Napraviti čisti `fullclean`/build iz `sdkconfig.defaults`.
- Provjeriti efektivni `sdkconfig`: target ESP32-S3, DIO/80 MHz/16 MB, Octal PSRAM/80 MHz i FreeRTOS 1000 Hz.
- Build ne smije ostaviti neočekivane promjene u praćenim datotekama.

## 5. Faza 2 — ponovno povezivanje stvarnog audio/MIDI toka

### Datoteke

- `main/app_main.c`
- `components/synth_engine/synth_engine.c`
- odgovarajuća zaglavlja

### Ciljani boot slijed

1. Zabilježiti memorijsku telemetriju.
2. Konfigurirati samo potvrđeni `SD_MODE=GPIO14`; ne konfigurirati GPIO za nespojeni GAIN.
3. Inicijalizirati Audio HAL i uključiti pojačalo nakon stabilizacije taktova.
4. Stvoriti MIDI queue.
5. Pozvati `synth_engine_init(midi_queue)`.
6. Pokrenuti `synth_engine_start()` kao jedinog stalnog I2S producenta.
7. Inicijalizirati USB MIDI host.
8. Ući u nadzorni heartbeat bez drugog audio taska.

### Posao

- Ukloniti beskonačni `i2s_audio_task` iz produkcijskog toka.
- Ako je boot test-ton uključen, reproducirati ga jednokratno prije pokretanja synth taska ili kroz sam synth engine; nikada ne dopustiti dva paralelna I2S writera.
- Svaki init korak provjeriti i prekinuti daljnje pokretanje ako mu ovisi funkcija nije spremna.
- Dodati smisleno gašenje/mute ponašanje pri fatalnoj inicijalizacijskoj grešci.
- Na USB disconnect poslati `All Notes Off` kako nijedan glas ne bi ostao zaglavljen.
- Heartbeat proširiti korisnim brojačima: USB stanje, queue high-water/drop count, I2S greške, slobodni heap i minimalni stack watermark.

### Kriterij prihvata

- Bez USB uređaja izlaz je tišina i firmware ostaje stabilan.
- `Note On`, `Note Off`, CC i pitch bend iz queuea dolaze do synth enginea.
- Nema poruka `MIDI queue full` tijekom normalnog sviranja.
- Samo synth task poziva kontinuirani `audio_hal_write()`.

## 6. Faza 3 — siguran USB MIDI host i hot-plug lifecycle

### Datoteke

- `components/usb_midi_host/usb_midi_host.c`
- `components/usb_midi_host/include/usb_midi_host.h`

### Posao

- Uvesti eksplicitna stanja, primjerice `STOPPED`, `WAITING`, `OPEN`, `STREAMING` i `DISCONNECTING`.
- Na disconnectu prvo zaustaviti ponovno slanje transfera, zatim kroz podržani ESP-IDF lifecycle pričekati završni `CANCELED`/`NO_DEVICE` callback, pa tek onda osloboditi transfer, otpustiti interface i zatvoriti uređaj.
- Sav cleanup učiniti idempotentnim kako dvostruki disconnect ili djelomični init ne bi uzrokovali double-free.
- Ukloniti fallback koji prihvaća bilo koje sučelje s IN endpointom.
- Prihvatiti samo USB Audio/MIDIStreaming sučelje (`class 0x01`, `subclass 0x03`) i odgovarajući IN endpoint s podržanim transfer tipom.
- Ako je sučelje claimano, a odgovarajući endpoint nije pronađen, odmah ga otpustiti prije nastavka pretrage.
- Validirati deskriptorske granice, alternate setting i maksimalnu veličinu paketa prije alokacije transfera.
- Parser izdvojiti u zasebnu funkciju pogodnu za testiranje te validirati CIN/status kombinacije i veličinu paketa.
- Zamijeniti log-spam kod punog queuea brojačem odbačenih poruka i rate-limitiranim upozorenjem.
- Dodati `deinit`/rollback putanju za greške nakon instalacije hosta, registracije klijenta ili kreiranja taska.
- Spremiti task handleove i spriječiti dvostruku inicijalizaciju.

### Testovi

- Podržani Nektar kontroler: connect, događaji, disconnect.
- Deset uzastopnih connect/disconnect ciklusa.
- Isključivanje tijekom aktivnog sviranja i tijekom aktivnog IN transfera.
- Nepodržani USB uređaj mora biti odbijen bez claima pogrešnog interfacea i bez curenja resursa.
- Ponovno spajanje nakon greške mora vratiti normalan MIDI tok.

## 7. Faza 4 — stabilnost synth/DSP enginea

### Datoteke

- `components/synth_engine/synth_engine.c`
- `components/synth_engine/include/synth_engine.h`

### Posao

- Vezati maksimalni cutoff uz odabrani sample rate i sigurnu granicu ispod Nyquista, umjesto hardkodiranih 18 kHz.
- Ograničiti SVF koeficijente i rezonanciju tako da ostanu stabilni kroz cijeli CC 71/74 raspon.
- Release izračunavati iz trenutačne razine envelopea kako rano otpuštena nota ne bi imala pogrešno trajanje ili skok.
- Definirati očekivano ponašanje ponovljenog `Note On` za istu notu i provjeriti voice-stealing pri svih 16 zauzetih glasova.
- Ograničiti sve MIDI vrijednosti na dopuštene raspone prije korištenja u DSP-u.
- Provjeravati rezultat i broj zapisanih bajtova iz `audio_hal_write()`; voditi brojače grešaka i kratkih zapisa.
- Ako synth task ne uspije alocirati DMA buffer, očistiti task handle/stanje kako ponovni start ne bi lažno vratio `ESP_OK`.
- Sinkronizirati promjenu moda i učitavanje/oslobađanje SoundFonta sa synth taskom; audio task ne smije koristiti `s_tsf` dok ga drugi kontekst zatvara ili zamjenjuje.
- Jasno definirati ponašanje SoundFont moda bez učitane banke: odbiti promjenu moda ili sigurno ostati na VA engineu.
- Dodati status API za aktivne glasove, audio greške i stanje enginea radi dijagnostike.

### Automatizirani testovi

- MIDI note/frequency i pitch-bend granice.
- ADSR prijelazi, uključujući `Note Off` tijekom attack/decay faze.
- Voice allocation, retrigger, voice-stealing i `All Notes Off`.
- CC 1/7/71/74 te ekstremne vrijednosti.
- DSP blokovi ne smiju sadržavati NaN/Inf niti prekoračiti izlazni int16 raspon.
- Cutoff mora ostati ispod sigurne Nyquist granice za odabrani sample rate.

## 8. Faza 5 — Audio HAL i upravljanje hardverom

### Datoteke

- `components/audio_hal/audio_hal.c`
- `components/audio_hal/include/audio_hal.h`

### Posao

- Uskladiti pin makroe, komentare, logove i stvarno ožičenje potvrđeno u Fazi 0.
- Uskladiti sample rate u kodu i dokumentaciji.
- Dodati `audio_hal_deinit()` kako bi djelomični init i testovi mogli uredno osloboditi kanal.
- Dodati provjeru poravnanja PCM buffera na cijele stereo frameove.
- Definirati jedan jasan timeout kontrakt u tickovima na javnom API-ju i milisekundama prema ESP-IDF driveru.
- Po potrebi dodati `audio_hal_mute()` koji upravlja samo stvarno spojenim SD_MODE signalom.
- SD_MODE izbaciti iz `app_main` u audio/BSP konfiguraciju i zadržati na GPIO14. Potpuno ukloniti softversko upravljanje nespojenim GAIN signalom i GPIO21.

### Kriterij prihvata

- Init/deinit/init ciklus prolazi bez curenja kanala.
- Tišina, test-ton i synth blokovi reproduciraju se bez kratkih I2S zapisa ili timeouta.
- Kod ne dira nijedan GPIO koji nije dio potvrđene konfiguracije.

## 9. Faza 6 — dokumentacija i uklanjanje zastarjelih tvrdnji

### Datoteke

- `README.md`
- `AGENTS.md`
- `WIRING_DIAGRAM.md` i tipfeler-duplikat `WIREING_DIAGRAM.md`
- `docs/STATE_SUMMARY.md`
- `docs/NEXT_SESSION.md`
- M0/M1/M2 dokumenti i evidence indeks

### Posao

- Postaviti jednu kanonsku wiring datoteku; drugi naziv ukloniti ili pretvoriti u kratku poveznicu radi kompatibilnosti.
- Svugdje uskladiti I2S pinove, SD/GAIN, sample rate, flash DIO i Octal PSRAM.
- Ispraviti ESP-IDF putanju i ukloniti zastarjele Python/env pretpostavke.
- Promijeniti tvrdnje poput “implementirano i potvrđeno” ako trenutačni firmware tu funkciju ne pokreće ili dokaz pripada starijoj konfiguraciji.
- Povijesne M0/M1 rezultate zadržati, ali označiti datum, firmware revision i hardver na kojem su dobiveni.
- Ažurirati arhitekturu stvarnim tokom: USB host → MIDI queue → synth task → Audio HAL → MAX98357A.
- Dokumentirati kako uključiti boot test-ton, kako odabrati synth mod i kako čitati runtime telemetriju.

### Kriterij prihvata

- Pretraga repozitorija ne nalazi međusobno proturječne aktivne pinove, sample rate ili build putanju.
- Novi developer može iz čistog checkouta napraviti build koristeći samo dokumentirane korake.

## 10. Faza 7 — završna verifikacija

### Softverska provjera

- Čisti ESP-IDF 6.0.2 build iz default konfiguracije.
- Provjera veličine firmwarea i particijskih granica.
- Pokretanje svih host/Unity testova.
- `git diff --check` i pregled da build nije promijenio praćene datoteke.

### Hardverska provjera

- Boot bez USB uređaja: bez crasha, bez neželjenog tona i bez rasta potrošnje memorije.
- Boot sa spojenim kontrolerom i spajanje nakon boota.
- Funkcionalni test svih nota, velocityja, pitch benda i podržanih CC kontrola.
- Brzi arpeggio/chord test za svih 16 glasova i kontrolirano voice-stealing ponašanje.
- Disconnect dok note sviraju: trenutačni `All Notes Off`, bez zaglavljenog zvuka.
- Najmanje 25 hot-plug ciklusa i 30 minuta kontinuiranog sviranja.
- Zabilježiti minimalni slobodni heap, stack watermarke, MIDI drop count, I2S error count i neočekivane resete.
- Poslušati/izmjeriti pucketanje, dropout i očitu nestabilnost filtra pri ekstremnim CC vrijednostima.

### Završni kriterij

Projekt je spreman za sljedeću funkcionalnu fazu tek kada:

- MIDI tipka pouzdano proizvodi i zaustavlja odgovarajući ton;
- hot-plug ne uzrokuje crash, use-after-free, curenje ili zaglavljenu notu;
- nema I2S grešaka ni MIDI dropova u normalnom radu;
- dokumentacija odgovara stvarnom buildu i stvarnom ožičenju;
- Git radno stablo je čisto nakon builda i testova.

## 11. Predloženi redoslijed zasebnih promjena

1. `config: align ESP32-S3 flash, PSRAM and build defaults`
2. `bsp: establish canonical audio pins and sample rate`
3. `app: replace diagnostic tone loop with synth pipeline`
4. `usb: make MIDI descriptor selection and hot-plug cleanup safe`
5. `synth: stabilize DSP limits, envelope and runtime lifecycle`
6. `audio: add validated write handling and teardown`
7. `test: add MIDI parser and synth engine coverage`
8. `docs: align wiring, build instructions and validation status`

Faza 0 je zaključena: I2S ostaje na GPIO `45/3/47`, `SD_MODE=GPIO14`, GAIN nije spojen, a sample rate je 44,1 kHz. Sutra prvo treba provesti konfiguracijsko čišćenje iz Faze 1, zatim funkcionalnu integraciju iz Faze 2 i ispraviti zastarjele reference u dokumentaciji.
