# M0 - Hardware/BSP audit

Datum: 2026-09-12. Opseg: dokumentacijski audit, početni board component i build.
Status: dokumentacijski dio izrađen; fizička potvrda ploče ostaje otvorena.
Naknadna [COM6 provjera](M0_COM6_CHECK.md) potvrdila je P4 v1.3, 16 MB flasha
i 32 MB PSRAM-a u postojećem firmwareu. Nije proveden flash, mjerenje napajanja ni audio test.

## Izvori i razina povjerenja

Putanje ispod relativne su prema `DEVICES_MANUALS/JC-ESP32P4-M3-DEV0/`.

| ID | Izvor | Primjena |
| --- | --- | --- |
| S1 | `5-Schematic/1_PWR&SPEAKER.png` | NS4150, speaker, 3V3 regulator, ulaz napajanja |
| S2 | `5-Schematic/2_EXPAND_IO&BAT.png` | IP5306, baterija, expansion header, backlight |
| S3 | `5-Schematic/3_8311&TFCARD.png` | ES8311, SD napajanje i signali |
| S4 | `5-Schematic/4_ESP32P4.png` | Mapiranje modula na GPIO, BOOT, C6 enable |
| S5 | `5-Schematic/5_USB&485.png` | FS/HS/CH340C priključci, VBUS topologija |
| S8 | `5-Schematic/8_OTHER.png` | Touch dijeli I2C s codecom |
| P1 | `2-Specification/JC-ESP32P4-M3-DEV Specifications-EN.pdf`, str. 3-6 | Deklarirane memorije, napon i dimenzije |
| P2 | `6-User_Manual/Getting started JC-ESP32P4-M3-DEV.pdf` | Arduino upute; uvod navodi drugi model JC1060P470, zato nije autoritet za pinove |
| D1 | `1-Demo/ARDUINO-DEMO/mp3_player/mp3_player.ino`, redci 10-27, 83-100 | Audio/SD pinovi, codec adresa; primjer koristi 44.1 kHz |
| D2 | `1-Demo/IDF-DEMO/xiaozhi-esp32/main/boards/guition-jc-esp32p4-m3-dev/config.h` | Dodatna audio potvrda; 16 kHz je postavka aplikacije |
| D3 | `1-Demo/IDF-DEMO/NoDisplay/wifi_scan/sdkconfig`, redci 2271-2312 | C6 SDIO pinovi, slot 1, reset GPIO54 |
| D4 | `1-Demo/IDF-DEMO/NoDisplay/common_components/espressif__esp32_p4_function_ev_board/esp32_p4_function_ev_board.c`, redci 119-142 | SD slot 0, LDO kanal 4, bez CD/WP |

Shematski netovi imaju prednost pred kopiranim generičkim imenima BSP-a.
Primjeri se koriste kao dokaz konfiguracije, bez preuzimanja njihova izvornog koda.
Naziv direktorija `DEV0` nije dokaz revizije stvarne PCB ploče.

## Identitet i memorija

Plan i P1 ciljaju JC-ESP32P4-M3-DEV: ESP32-P4 + ESP32-C6, deklarirano 32 MB
PSRAM i 16 MB flash, napajanje 5 V, dimenzije 92 x 62 mm.
P1 je nedosljedan oko takta: tekst spominje 400 MHz, tablica 360 MHz.
COM6 provjera naknadno potvrđuje silicij v1.3, flash 16 MB i PSRAM 32 MB;
M0 defaults sada koriste 360 MHz. PCB revizija ostaje otvorena, a PSRAM timing
za NEKTAR-P4 još nije kvalificiran.

Lokalna instalacija `C:/Espressif/v6.0.2/esp-idf/tools/cmake/version.cmake`
potvrđuje ESP-IDF 6.0.2. Projekt je ciljan isključivo na `esp32p4`.

## Board profil

Smjerovi DIN/DOUT odnose se na P4, ne na codec.

| Funkcija | Vrijednost | Dokaz / ograničenje |
| --- | --- | --- |
| Codec | ES8311-S, I2C 7-bit adresa 0x18 | S3 + D1; ACK nije izmjeren; neke biblioteke koriste 8-bit 0x30 |
| I2C SDA / SCL | GPIO7 / GPIO8 | S4, D1, D2; zajednička sabirnica s touchom i expansion headerom |
| I2S MCLK / BCLK / WS | GPIO13 / GPIO12 / GPIO10 | S3/S4, D1/D2 |
| I2S TX prema codec DSDIN | GPIO9 | S3/S4, D1/D2 |
| I2S RX od codec ASDOUT | GPIO48 | S3/S4; net je naslijeđeno nazvan ES7210_SDOUT |
| PA_CTRL | GPIO11 | S1/S4, D1/D2; R19 10k prema GND; demo sugerira HIGH za enable, potvrditi prije M1 |
| Onboard audio | Jedan mono kanal, ES8311 OUTP/OUTN -> NS4150 -> CN1 | Diferencijalni izlazi nisu L/R; ne spajati speaker minus na signalnu masu |
| SD CLK / CMD | GPIO43 / GPIO44 | S3/S4, D1 |
| SD D0 / D1 / D2 / D3 | GPIO39 / 40 / 41 / 42 | S3/S4, D1 |
| SD host / power | Slot 0, LDO kanal 4 | S3 + D4; TF_VCC preko Q1; R10 prema GPIO45 označen NC |
| SD card detect / write protect | Nema dodijeljenog GPIO | S3 + D4; ne izmišljati detekciju vađenja kartice |
| C6 SDIO CLK / CMD | GPIO18 / GPIO19 | D3; unutarnje veze modula nisu prikazane u carrier shemi |
| C6 SDIO D0 / D1 / D2 / D3 | GPIO14 / 15 / 16 / 17 | D3, privremena rezervacija |
| C6 host / enable | Slot 1 / GPIO54 | D3; S4 potvrđuje GPIO54 -> C6_CHIP_PU |
| BOOT / RESET | GPIO35 / CHIP_PU | S4 SW1/SW2; ne koristiti D2 BOOT_BUTTON_GPIO=21 |
| Touch INT / RESET | GPIO21 / GPIO22 | S4/S8 |
| LCD backlight PWM | GPIO23 | S2/S4; M0 ga ne pokreće |
| USB VBUS enable GPIO | Nije pronađen | S5; NC u profilu znači da nema identificirane kontrole |

Interna stereo sinteza ostaje projektni zahtjev. Audio HAL će u M1 morati
definirati mono slot i kontrolirani downmix; par diferencijalnih izlaza ES8311
ne može dati stereo line-out.

## USB i napajanje

S5 razlikuje tri USB-C konektora: USB1 je USB-TTL preko CH340C,
USB2 je Full Speed (`USB1P1_P/N`), a USB3 je High Speed (`ESP_USB_P/N`).
Ovo su oznake konektora/netova sa sheme, ne ESP-IDF brojevi root portova.
Njihovo povezivanje s postojećim USB Host API-jem pripada M2.

Sva tri VBUS priključka na S5 spojena su na `USB5V_IN`; CC otpornici su 5.1k
prema masi. Nije prikazan zaseban programabilni host VBUS prekidač niti
pojedinačna zaštita struje za GX49. Podrška USB hosta u procesoru zato sama
ne potvrđuje ispravno napajanje i USB-C host povezivanje ovog carrier boarda.
Treba utvrditi stvarni kabel/adapter, port postojećeg hosta, izvor 5 V,
strujni budžet GX49 i moguće povratno napajanje pri istodobnom priključivanju.

S2 vodi `USB5V_IN` u IP5306; `VOUT-BAT` napaja NS4150 (S1) i preko R22
granu `VCC5V`, a TLV62569 daje 3V3. Iz toga ne slijedi da baterija napaja
USB VBUS klavijature: to su različito imenovani netovi.
Baterijski rad i napajanje GX49 ostaju zasebne provjere.

## Konflikti i odluke

- Audio GPIO7-13/48 i SD GPIO39-44 nemaju međusobno preklapanje.
- Privremeno rezervirani C6 GPIO14-19/54 ne preklapaju audio ni SD;
  SDMMC ima planirane slotove 0 za karticu i 1 za C6. Simultan rad nije testiran.
- GPIO7/8 dijele codec i touch: ubuduće jedan vlasnik I2C busa, bez dva
  neovisna inicijalizatora. Treba provjeriti pull-upove i adrese priključenih uređaja.
- GPIO35 je i `RMII_TXD1` prema S4: BOOT tipku ne uzorkovati kao običan UI input
  ako se poslije uključi Ethernet. Ethernet nije u početnom synth opsegu.
- GPIO45 nije potvrđeni SD power enable: R10 je NC. Ne upravljati njime kao napajanjem.
- D3 `RESET_ACTIVE_HIGH` naziv ne pretvarati u fizički aktivno-visoki reset:
  na S4 je C6_CHIP_PU s pull-upom. Semantiku drivera provjeriti u M7.
- Dokumentacija ne sadrži identificirano korisnikovo postojeće USB Host rješenje.
  U M2 treba dobiti njegov izvor/API, bez razvoja zamjenskog hosta.

## Što je implementirano

`components/board_jc_esp32p4_m3` vraća nepromjenjivi profil bez perifernih
side-effectova. Sadrži pinove audija, SD-a, rezervacije za C6, BOOT i odsutnost
identificiranog VBUS GPIO-a. Fizička revizija i unutarnje C6 veze eksplicitno su
označene kao nepotvrđene. Minimalni `app_main` samo ispisuje profil.

Nisu uključeni vendor demo projekti, Arduino, codec driver, USB driver,
Wi-Fi ni PSRAM. Zadana particijska tablica služi samo početnom buildu;
nije implementirana OTA strategija iz M15.

## Preostalo za zatvaranje M0 na hardveru

1. Zabilježiti oznaku/reviziju PCB-a i modula te povezane dodatke.
2. Potvrđeno na COM6: P4 v1.3 i 16 MB flash. Postojeći firmware prepoznaje
   32 MB PSRAM i prolazi početni memorijski test na 20 MHz.
3. Provjeriti 5 V/3V3 i način napajanja GX49, USB adapter i VBUS topologiju.
4. Potvrditi ES8311 ACK na 0x18 te PA_CTRL polaritet/isključeno stanje.
5. Za M7 potvrditi C6 unutarnje veze i firmware; prije M10 potvrditi TF napajanje.

Sljedeći razvojni korak je M1: audio HAL, kontrolirano uključivanje codeca i
pojačala, 48 kHz/440 Hz sinus, DMA/underrun brojači i volume test.
To nije izvedeno u M0. Fizički M0 ostaje otvoren dok se ne zabilježe provjere 1-4;
točke 5 ostaju eksplicitni uvjeti za kasnije faze.

## Validacija

Build i završni rezultat zapisani su u `M0_VALIDATION.md`.
