# M1 audio test - 2026-09-13

Korisnik je potvrdio da već posjeduje backup instaliranog imagea i odobrio
nastavak. Nova kopija backupa nije izrađena niti je postojeća kopija pregledana.

## Implementacija i korištenje

`audio_hal` je jedini vlasnik codec I2C-a i I2S TX-a. ES8311 se konfigurira
driverom `espressif/esp_codec_dev` **1.6.2**, Apache-2.0, iz službenog
[Component Registryja](https://components.espressif.com/components/espressif/esp_codec_dev/versions/1.6.2).
Verzija i hash su u `dependencies.lock`; paket ostaje u ignoriranom
`managed_components/`. Nisu preuzimani dobavljački demo izvori.

Format: 48000 Hz, MCLK 256fs (12.288 MHz), Philips I2S, 16-bitni mono lijevi slot,
6 DMA descriptora po 128 frameova. Float32 L/R se downmixa prije PCM konverzije.
Sinus koristi tablicu 1200 uzoraka i korak 11: matematički 440 Hz pri 48 kHz.
Codec gain je -12 dB, softverski ton početno 1% amplitude. Točna izlazna
frekvencija još zahtijeva mjerenje; postavka nije mjerenje.

Boot drži GPIO11 nisko, provjerava ACK na 0x18, inicijalizira codec i šalje
tišinu. Driver prima 8-bitnu adresu 0x30; native I2C probe prima 7-bitnu 0x18.
Audio task na core 1 nema I2C ni logiranje. Sve se unaprijed alocira;
I2S write ima konačni timeout 20 ms. Na grešci se PA isključuje i fault ostaje
latched do restarta. Inicijalizacija se ne pokušava ponavljati nakon pogreške.

Serijski terminal COM6, 115200 baud (jedan vlasnik porta):

| Tipka | Radnja |
| --- | --- |
| `t` ili `1` | 440 Hz ton na 1% amplitude |
| `2` | Ton na 2% amplitude |
| `m` | Rampa do tišine; nakon pražnjenja DMA-a PA off |
| `x` | Rampa do tišine, PA off i zaustavljanje DMA-a |
| `s` | Pokretanje DMA-a s tišinom |
| `?` | Ispis brojača |
| `r` | Jednosekundna mikrofonska snimka i UART izvoz |

Glasnoća se mijenja po uzorku, bez naglog skoka. Prije gašenja PA-a šalje se
najmanje osam tihih blokova. Status ispisuje upravljački task približno svakih
10 sekundi. Reset uvijek počinje tišinom; nema automatskog tona pri bootu.

## Značenje brojača

- `blocks`: potpuno predani blokovi; `dma`: završeni DMA descriptor događaji.
- `render_max_us`: maksimum render+konverzije bez čekanja I2S upisa.
- `deadlines`: render trajao >=2667 us.
- `write_errors` / `short`: I2S greška ili nepotpun upis, zaustavljaju test.
- `tx_q_ovf`: IDF TX event queue overflow. Ovo je pokazatelj nepravodobnog
  servisiranja, ne izravni hardverski underrun brojač.
- `gaps`: razmak između dovršenih upisa >16 ms, nakon početka svake sesije.
- `failed`: latched neuspjeh audio taska; `running`: stanje DMA streama.

Brojači isključuju početnu codec konfiguraciju. Za test koristiti delte nakon
stabilizacije. Nulti brojači ne zamjenjuju slušni pregled, mjerenje clocksignala
ili dokaz realne key-to-audio latencije.

## Rezultati

Build `0.1.0-m1` prolazi na ESP-IDF 6.0.2, bez compiler warning/error poruka.
Aplikacija: 285536 bajtova (`0x45b60`), 73% app particije slobodno.
SHA256: `d66677c52ca01cbce5a9ae96cac5d3e0df4d280524153b2e5a9924c8e22da752`.

Flash na COM6 završen s exit code 0 i esptool hash verifikacijom sva tri
zapisana segmenta: bootloader, particijska tablica i aplikacija.
Dokaz: [flash log](evidence/m1-flash.log).
Završeci redaka i završni razmaci spremljenih logova normalizirani su za Git.

[30-sekundni boot/smoke](evidence/m1-boot-smoke.log) potvrđuje:

- `nektar_p4` / `0.1.0-m1`, silicij v1.3, 360 MHz i header flash 16 MB.
- ES8311 ACK na 0x18 i readback SDP registra 0x09 = 0x0c.
- Slanje tišine, `x` zaustavlja DMA (`running=0`), `s` ga ponovno pokreće.
- Završno 9665 blokova / 9665 DMA completion događaja; render maksimum 70 us.
- Nula render deadlinea, write grešaka, short writeova, TX queue overflowa
  i >16ms service gapova; `failed=0`.

Na UART-u se pri inicijalizaciji vidio jedan nepoznati znak; nije aktivirao
nijednu audio naredbu. Port je nakon capturea zatvoren. Izlaz je ostao tih,
PA off, DMA aktivan. Nije poslana naredba za ton.

Naknadno je korisnik potvrdio priključen zvučnik/napajanje. Naredbom `t` uključen
je ton; korisnik potvrđuje **čist i stabilan ton bez pucketanja ili prekida**.
Nakon slijeda `2 -> m -> t -> x -> s -> t` potvrđuje i **uredne prijelaze bez
primjetnih klikova/pucketanja**. Time je funkcionalno potvrđen audio put i
aktivno-visoko upravljanje PA-om. Električna mjerenja nisu izvedena.
Dokaz početka: [tone-start log](evidence/m1-tone-start.log).

Desetominutni tone soak na `0.1.0-m1` je **PASS za softverske brojače**:
601.970 s, 225739 blokova/DMA completiona, 28894592 frameova. Maksimalni render
70 us, sve delte grešaka nula. Na kraju mute i DMA stop, `running=0, failed=0`.
Dokaz: [soak log](evidence/m1-tone-soak.log) i
[strojno provjeren sažetak](evidence/m1-tone-soak-summary.json).

Generički flash driver pri bootu daje informativno
upozorenje za Boya flash; konfiguracija stvarnog kapaciteta sada je ispravna.

## Mikrofon - 0.1.1-m1-mic

Na korisnikov zahtjev dodan je ES8311 ADC preko GPIO48, gain 24 dB, analogni
mikrofon, lijevi mono slot. Interna DAC referenca eksplicitno je isključena:
`no_dac_ref=true`, readback registra 0x44 = 0x08. Naredba `r` snima 48000 uzoraka
u interni buffer, provjerava RX overflow/upise i prenosi ih kroz UART kao hex.
Snimanje i prijenos radi control task, dok TX nastavlja na core 1.
Tijekom prijenosa (oko 18 s) control task ne obrađuje nove naredbe; ovo je
dijagnostički alat, ne konačni interaktivni UI.

Build prolazi; na COM6 je flashana verzija **0.1.1-m1-mic** s hash verifikacijom.
Aplikacija 288256 bajtova; SHA256
`a5a4b4571886f3624d8f28d5af34f502c279c3691c18f76df7af9b03afa59cff`.
Dokaz: [mic flash](evidence/m1-mic-flash.log), [mic capture](evidence/m1-microphone.log).
Prvi test ove verzije bio je 70-sekundni smoke s dvije snimke i mute/stop.
Zaseban završni soak aktualnog imagea opisan je na kraju ovog dokumenta.

| Snimka | Vrh 100-2000 Hz | RMS | Amplituda na 440 Hz | Clipping |
| --- | --- | --- | --- | --- |
| Utišani izlaz | 650.718 Hz (pozadina) | -57.51 dBFS | -95.63 dBFS | 0 |
| 440 Hz ton, 1% gain | **440.001 Hz** | -39.26 dBFS | -36.28 dBFS | 0 |

Komponenta na 440 Hz je oko **59.35 dB iznad iste komponente utišane snimke**.
Analiza izostavlja prvih 100 ms ADC starta, uklanja DC te koristi Hann FFT s
interpolacijom vrha. Zaokruženi broj je procjena frekvencije, ne deklaracija
metrologijske točnosti. ADC i DAC dijele clock, pa rezultat potvrđuje frekvenciju
u odnosu na nominalnih 48 kHz, ali ne kalibrira apsolutnu frekvenciju oscilatora.
Interni DAC reference loopback je isključen; električni crosstalk nije zasebno
izmjeren. Slušna potvrda korisnika dodatno potvrđuje stvarni zvuk sa zvučnika.

[JSON analiza](evidence/m1-microphone-summary.json),
[tišina WAV](evidence/m1-microphone-1.wav), [ton WAV](evidence/m1-microphone-2.wav).
Ponovljivo: `tools/analyze_microphone.py` (numpy) i `tools/analyze_m1_capture.py`.
Sve TX greške nula tijekom capturea, render maksimum 67 us. Nakon testa
**PA off, DMA zaustavljen, COM6 oslobođen**.

## Završna kvalifikacija M1 - aktualni image

**M1 funkcionalno završen 2026-09-13.** Prije završnog testa aplikacija je
očitana iz flasha od 0x10000, duljine 288256 bajtova. SHA256 readbacka jednak je
`a5a4b4571886f3624d8f28d5af34f502c279c3691c18f76df7af9b03afa59cff`.
Nije bilo promjene koda, rebuilda ni novog flashanja tijekom ovog zatvaranja.
[Boot identitet](evidence/m1-final-identity.log) potvrđuje 0.1.1-m1-mic i codec
readback. Početak identity loga sadrži fragment prethodnog UART ispisa prije
namjernog reseta; taj reset nije dio soak prozora.

Aktualni image prošao je **602.020 s** neprekinutog tona između eksplicitnih
statusnih checkpointa. Napredak: **225758 blokova**, **28897024
frameova**, **225758 DMA completion događaja**. Maksimum renderiranja
**66 us** naspram cilja 1333 us; sve delte deadlines/write_errors/short/
tx_q_ovf/gaps su **0**, nema reseta ni failed stanja.

Tijekom istog neprekinutog tona izvedene su dvije mikrofonske snimke po 48000
uzoraka (naredbe na 180 s i 420 s host vremena), uključujući UART izvoz:

| Snimka | Procijenjeni vrh | RMS | Clipping |
| --- | --- | --- | --- |
| 1 | 440.001 Hz | -38.95 dBFS | 0 |
| 2 | 439.998 Hz | -39.18 dBFS | 0 |

Ovo je potvrda tona relativno prema zajedničkom ADC/DAC clocku. Apsolutna
kalibracija 48 kHz/440 Hz neovisnim instrumentom nije provedena. TX queue i
service-gap brojači nisu izravno mjerenje hardverskog underruna. Ranije
korisničke potvrde čistog tona i urednih prijelaza ostaju slušni dokaz.

Dokazi: [završni soak log](evidence/m1-final-soak.log),
[soak JSON](evidence/m1-final-soak-summary.json),
[mikrofonska analiza](evidence/m1-final-mic-summary.json),
[WAV 1](evidence/m1-final-mic-1.wav), [WAV 2](evidence/m1-final-mic-2.wav).
Ponoviti analizu s `tools/analyze_m1_capture.py` i `tools/analyze_microphone.py`.
Nakon testa `m`, zatim `x`: **izlaz tih, PA off, DMA stop, running=0, failed=0**.
Serijski capture je završen i COM6 oslobođen.

Preostale fizičke M0 provjere (PCB oznaka, USB napajanje, C6/SD) ostaju zasebne.
Nema još USB/MIDI ni key-to-audio latencijskog testa.
