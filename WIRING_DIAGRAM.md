# NEKTARINA - Hardverska Shema Spajanja (Wiring Diagram & Pinout)

**Projekt:** NEKTARINA - Samostalni ESP32-S3 USB-MIDI Synthesizer  
**Ciljni hardver:** ESP32-S3-WROOM-1-N16R8 (16 MB Octal Flash, 8 MB Octal PSRAM)  
**Dokument:** Hardverska shema povezivanja komponenata  
**Verzija:** 0.2.0-s3-midi  

---

## 1. Pregled Sustava

NEKTARINA sustav sastoji se od četiri ključne hardverske cjeline:
1. **ESP32-S3 mikrokontroler (N16R8)** – Mozak sustava, izvodi FreeRTOS, USB Host MIDI stog i 16-glasovni DSP synth engine.
2. **MAX98357A I2S 3W D-Class mono pojačalo** – Prima digitalni I2S PCM audio stream izravno s ESP32-S3 i pretvara ga u analogni zvučni signal visoke učinkovitosti.
3. **Zvučnik (4 Ω ili 8 Ω, 3 W – 5 W)** – Spojen izravno na diferencijalne izlaze pojačala.
4. **Nektar MIDI klavijatura (npr. Impact GX49)** – Spojena na USB-OTG port mikrokontrolera u USB Host načinu rada.

```
       +---------------------------------------------+
       |           Nektar Impact GX49                |
       |             MIDI Klavijatura                |
       +----------------------+----------------------+
                              |
                     [USB-C / USB-A Kabel]
                              |
                              v (USB Host: GPIO 19/20, 5V, GND)
       +---------------------------------------------+
       |           ESP32-S3-WROOM-1-N16R8            |
       |  - 240 MHz Dual Core                        |
       |  - 8 MB Octal PSRAM                         |
       |  - Core 1: 16-Voice VA DSP Synth Engine     |
       +----------------------+----------------------+
                              |
                     [I2S Sabirnica]
                     - GPIO 16: BCLK
                     - GPIO 17: LRC (WS)
                     - GPIO 18: DIN
                     - +5V & GND
                              |
                              v
       +---------------------------------------------+
       |         MAX98357A I2S Audio Pojačalo        |
       |  - GAIN: Ostavljen nepospojen (12 dB)       |
       |  - SD_MODE: Ostavljen nepospojen (L+R sum)  |
       +----------------------+----------------------+
                              |
                     [Diferencijalni BTL]
                     - SPK+ / SPK-
                              |
                              v
       +---------------------------------------------+
       |          Pasivni zvučnik (4Ω - 8Ω)          |
       +---------------------------------------------+
```

---

## 2. ⚠️ KRITIČNE HARDVERSKE ZABRANE I NAPOMENE

> [!CAUTION]
> **GPIO 33, 34, 35, 36 i 37 SU NA STROGOJ ZABRANI KORIŠTENJA!**
> Na pločama s **N16R8** modulima ovi su pinovi interno izravno povezani na brzu sabirnicu Octal Flasha i Octal PSRAM-a.
> - **NIKADA** nemojte spajati nikakve žice, senzore ili otpornike na ove pinove!
> - Bilo kakva promjena stanja na ovim pinovima uzrokuje trenutni rušenje sustava (`Guru Meditation Error / Cache Panic`).

> [!WARNING]
> **DIFERENCIJALNI IZLAZ ZA ZVUČNIK (BTL):**
> MAX98357A koristi mosni (Bridge-Tied Load - BTL) izlaz bez zajedničke mase:
> - **NIKADA NE SPAJAJTE SPK- NA GND (MASU)!**
> - Spajanje `SPK-` terminala na masu ili međusobni kratki spoj `SPK+` i `SPK-` trajno će uništiti pojačalo.

---

## 3. Kompletna Tablica Povezivanja (Pinout)

### 3.1 I2S MAX98357A Pojačalo

| ESP32-S3 Pin | MAX98357A Pin | Smjer Signala | Tip Signala | Preporučena Boja | Funkcija / Opis |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **GPIO 16** | **BCLK** | ESP32 $\to$ MAX | Digitalni takt | Žuta | I2S Bit Clock (1.4112 MHz za 44.1 kHz 16-bit stereo) |
| **GPIO 17** | **LRC** | ESP32 $\to$ MAX | Digitalni takt | Plava | I2S Word Select / Left-Right Clock (44.1 kHz) |
| **GPIO 18** | **DIN** | ESP32 $\to$ MAX | Digitalni podatci | Zelena | I2S Serial PCM Data stream |
| **5V / VIN** | **VIN** | Napajanje | +5V DC | Crvena | Glavno napajanje pojačala (preporučeno 5V za punu snagu od 3W) |
| **GND** | **GND** | Masa | 0V DC | Crna | Zajednička referentna masa |
| *Nije spojen* | **GAIN** | Konfiguracija | Plutajući (NC) | — | Tvornički zadano pojačanje od **12 dB** (ostaviti nepovezano) |
| *Nije spojen* | **SD_MODE** | Konfiguracija | Plutajući (NC) | — | Stereo downmix u mono: $(L + R)/2$ (ostaviti nepovezano) |

#### MAX98357A Izlazi za Zvučnik
| MAX98357A Pin | Odredište | Opis |
| :--- | :--- | :--- |
| **SPK +** | Pozitivni terminal zvučnika (+) | Pozitivna diferencijalna faza |
| **SPK -** | Negativni terminal zvučnika (-) | Negativna diferencijalna faza (**NE SPAJATI NA GND!**) |

---

### 3.2 USB Host MIDI Sučelje (Nektar Impact GX49)

Ako vaša ESP32-S3 pločica ima namjenski drugi USB-C port označen kao **"USB"** (native USB-OTG), Nektar klavijaturu možete spojiti izravno USB-C kabelom ili preko USB-A na USB-C OTG adaptera.

Ukoliko povezujete USB žensku utičnicu (USB-A Female breakout) izravno na pinove pločice:

| ESP32-S3 Pin | USB-A Ženski Pin | Boja USB Žice | Funkcija / Opis |
| :--- | :--- | :--- | :--- |
| **GPIO 19** | **D- (Data -)** | Bijela | Full-Speed USB-OTG Data Minus |
| **GPIO 20** | **D+ (Data +)** | Zelena | Full-Speed USB-OTG Data Plus |
| **5V / VBUS** | **VBUS (+5V)** | Crvena | Napajanje za Nektar klavijaturu (iz 5V linije) |
| **GND** | **GND** | Crna | Zajednička masa USB sabirnice |

---

### 3.3 Serijska Konzola i Programiranje (UART0)

| ESP32-S3 Pin | Namjena | Opis |
| :--- | :--- | :--- |
| **GPIO 43** | **U0TXD** | UART 0 TX – Logiranje u serijsku konzolu (COM12, 115200 baud) |
| **GPIO 44** | **U0RXD** | UART 0 RX – Prijem naredbi |

*(Napomena: Na većini razvojnih pločica, GPIO 43 i 44 su već interno povezani na ugrađeni USB-to-UART čip, npr. CH340 ili CP2102, spojen na USB port označen kao "UART").*

---

## 4. Vizualni Dijagram Spajanja

### 4.1 ASCII Shema Spajanja

```text
                  +-------------------------+
                  |     ESP32-S3 N16R8      |
                  |                         |
                  | [5V]               [16] |-----> [BCLK] MAX98357A
                  | [GND]              [17] |-----> [LRC ] MAX98357A
                  |                    [18] |-----> [DIN ] MAX98357A
                  |                         |
                  | [19]               [5V] |=====> [VIN ] MAX98357A
                  | [20]              [GND] |=====> [GND ] MAX98357A
                  +----+---------------+----+
                       |               |
             +---------+               +----------------------+
             |                                                |
             v                                                v
      USB-A Ženski Port                               MAX98357A Pojačalo
   +---------------------+                        +-------------------------+
   | Pin 1: VBUS (+5V)   | <=== (iz 5V rail)      | VIN  : +5V              |
   | Pin 2: D-   (GPIO19)| <--- (bijela žica)     | GND  : GND              |
   | Pin 3: D+   (GPIO20)| <--- (zelena žica)     | BCLK : GPIO 16          |
   | Pin 4: GND          | <=== (iz GND rail)     | LRC  : GPIO 17          |
   +----------+----------+                        | DIN  : GPIO 18          |
              |                                   | GAIN : Nepospojen (12dB)|
              v                                   | SD   : Nepospojen (mono)|
   +---------------------+                        | SPK+ : Zvučnik (+)      |
   | Nektar Impact GX49  |                        | SPK- : Zvučnik (-)      |
   |   USB-MIDI Port     |                        +------------+------------+
   +---------------------+                                     |
                                                               v
                                                      +-----------------+
                                                      | Pasivni Zvučnik |
                                                      |  4Ω ili 8Ω 3W   |
                                                      +-----------------+
```

---

## 5. Napajanje i Savjeti za Smanjenje Audio Šuma

MAX98357A je vrlo učinkovito Class-D pojačalo, no digitalni mikrokontroleri s brzim sabirnicama (kao što je 80 MHz Octal PSRAM) mogu unijeti visoke frekvencije na liniju napajanja.

Za najbolju kvalitetu zvuka i uklanjanje šuma/zujanja preporučujemo:

1. **Decoupling kondenzator na pojačalu:**
   - Spojite **elektrolitski kondenzator od 220 µF do 470 µF (10V–16V)** paralelno između `VIN` i `GND` pinova na samoj MAX98357A pločici.
   - Po želji dodajte i manji **keramički kondenzator od 100 nF** paralelno s elektrolitskim za apsorpciju visokofrekventnih smetnji.
2. **Kratki I2S vodovi:**
   - Žice za `BCLK` (GPIO 16), `LRC` (GPIO 17) i `DIN` (GPIO 18) neka budu što kraće (preporučeno do 10–15 cm) kako bi se spriječilo preslušavanje i degradacija takta od 1.4 MHz.
3. **Kvalitetno 5V napajanje:**
   - Pojačalo troši vršno do 600 mA pri glasnom zvuku na 4 Ω zvučniku. Nektar klavijatura troši oko 100–250 mA.
   - Koristite stabilno 5V napajanje od **minimalno 2A** spojeno na ESP32-S3 pločicu.
4. **Zajednička točka mase (Star Ground):**
   - Spojite masu USB klavijature, masu ESP32-S3 i masu pojačala u jednu zajedničku čvrstu točku kako biste izbjegli petlje mase (*ground loops*).

---

## 6. Postupak Prvog Uključivanja i Provjere (Checklist)

1. **Provjera ožičenja bez spojenog zvučnika:**
   - Dvostruko provjerite da `GPIO 33–37` nisu nigdje spojeni.
   - Provjerite da `SPK-` nije spojen na masu (GND).
2. **Povezivanje zvučnika:**
   - Spojite zvučnik (4 Ω ili 8 Ω) na `SPK+` i `SPK-` terminale pojačala.
3. **Uključivanje na računalo / napajanje (COM12):**
   - Nakon spajanja USB kabela na UART port (COM12), unutar 1 sekunde trebate čuti **1.5-sekundni testni ton (440 Hz - komorni ton A4)**.
   - U serijskom monitoru (`idf.py -p COM12 monitor`) potvrdite ispis:
     ```text
     I (MAIN) Playing 1.5s 440Hz Sine Test Tone via I2S...
     I (MAIN) Test tone finished. System ready for MIDI!
     ```
4. **Spajanje Nektar klavijature:**
   - Uključite Nektar Impact GX49 u USB Host port.
   - U serijskom monitoru potvrdite poruku o detekciji uređaja:
     ```text
     I (MIDI) >>> Nektar MIDI keyboard CONNECTED and active <<<
     ```
   - Pritisnite tipku na klavijaturi i poslušajte Virtual Analog zvuk generiran u stvarnom vremenu bez latencije.
