# M1 - audio bring-up plan

Status: implementirano i flashano; smoke tišine i DMA stop/start prolaze.
Ton, slušni prijelazi i mikrofon potvrđeni su. Soak 601.970 s prolazi na
0.1.0-m1; za aktualni 0.1.1-m1-mic preostaje zasebni soak.
Dokazi i ograničenja u [M1_AUDIO_TEST](M1_AUDIO_TEST.md).
Cilj je stabilan 440 Hz
sinus preko onboard ES8311, uz 48 kHz audio. MIDI i USB integracija su M2/M3.

## Preduvjeti

- Provjeren backup cijelog flasha i potvrđena odgovarajuća rev1.x build konfiguracija.
- Potvrđen ES8311 na 7-bit I2C adresi 0x18 i PA_CTRL polaritet.
- Provjeren zvučnik i napajanje. NS4150 speaker izlaz je diferencijalan, nije line-out.
- Pinovi se uzimaju iz BSP-a, ne dupliciraju po driverima.

## Implementacija

1. `audio_hal`: jedan vlasnik I2C sabirnice GPIO7/8, codec kontrola i eksplicitno
   isključeno pojačalo tijekom inicijalizacije i pri pogrešci.
2. ES8311 driver prilagoditi IDF 6.0.2 i njegovoj adresnoj konvenciji; provjeriti
   MCLK/sample-rate postavke prije uključivanja izlaza. Prvo nizak gain.
3. I2S TX: GPIO13 MCLK, 12 BCLK, 10 WS, 9 DOUT, 48 kHz; slot/PCM format
   potvrditi prema codecu. Za početak ciljati 16-bitni mono izlaz.
4. Interna obrada float32 stereo, blok 128 frameova. Eksplicitni downmix
   `(L + R) / 2`, kontrolirani master gain i zasićenje pri PCM konverziji.
5. Unaprijed alocirani interni DMA bufferi i audio task, početno core 1.
   Nema alokacija, filesystema ni logiranja u audio petlji.
6. Najprije tišina/stabilan DMA, zatim postupna promjena amplitude 440 Hz sinusa
   i kontrolirano uključivanje pojačala. Definirati mute/stop/error put.
7. Dijagnostiku izvan audio taska: broj blokova, maksimalno vrijeme rendera,
   I2S greške, nepotpuni upisi i underrun/deadline brojači s jasnom definicijom.

## Predloženi kriteriji prihvaćanja M1

Ovo su kriteriji za budući test, ne postignuti rezultati:

- Build prolazi za v1.3; zapisani hash, konfiguracija i stvarni flash/boot dokaz.
- Potvrđeno 48 kHz i 440 Hz (mjerenjem ako je oprema dostupna; sluh sam nije
  dokaz točne sample-rate frekvencije).
- Najmanje 10 minuta kontinuiranog tona bez reseta, I2S grešaka, nepotpunih
  upisa i zabilježenih underruna; prijaviti početne/završne brojače.
- Render blok 128/48000 mora biti kraći od 2.667 ms; cilj za ovaj jednostavni
  test je maksimalno 1.333 ms kako bi ostala rezerva. To ne mjeri ukupnu latenciju.
- Mute, stop/start i promjena glasnoće rade bez primjetnih klikova; korisnik
  potvrđuje slušni rezultat. Dokumentirati što nije bilo moguće izmjeriti.

Pri pogrešci utišati izlaz i sačuvati brojače/log; ne označiti M1 kao PASS.
Key-to-audio latencija i USB reconnect provjeravaju se tek nakon MIDI integracije.
