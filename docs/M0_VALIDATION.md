# M0 validacija - 2026-09-12

## Aktualni build nakon COM6 provjere

PASS: ponovni `idf.py build`, exit code 0, ESP-IDF 6.0.2.
Bez compiler warning/error poruka u `tmp/m0-rev1-build.log`.

- Konfiguracija: P4 rev. 1.0-1.99, CPU 360 MHz, flash 16 MB DIO/80 MHz.
- `esptool image-info build/nektar_p4.bin` potvrđuje rev. 1.0-1.99 i 16 MB.
- Veličina: 180016 bajtova (`0x2bf30`), 83% app particije slobodno.
- SHA256: `bbb19b12202d9ae86603105c358bdd575380e577e87c1df7bc37dadde77ba45e`.
- Nije flashano. Postojeći firmware na COM6 ostao je sačuvan.

Ovo je aktualni build; tablica ispod bilježi povijest prvotnog builda.

## Prvotni dokumentacijski build (prije COM6 provjere)

PASS: `idf.py build` završio je s exit code 0 na lokalnom ESP-IDF 6.0.2.
Izgrađeni su board component, minimalna aplikacija, bootloader i particijska
tablica. U pregledanom logu nema compiler warning/error poruka.
Nije proveden flash niti runtime test NEKTAR-P4 aplikacije.
Naknadna [COM6 provjera](M0_COM6_CHECK.md) očitala je stvarni silicij i memoriju;
prvotni build opisan u tablici ispod time je zamijenjen konfiguracijom za v1.3.

## Ponovljiv postupak

Iz korijena projekta u PowerShellu:

```powershell
. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1
idf.py build
```

Lokalni puni log: `tmp/m0-build.log` (generirani dokaz, ignoriran u Gitu).
U trenutku rada workspace nije Git repozitorij; nema commit identiteta.

| Provjera | Rezultat |
| --- | --- |
| IDF | 6.0.2, potvrđeno lokalnim `tools/cmake/version.cmake` |
| Target | esp32p4 |
| Aplikacija | `build/nektar_p4.bin`, 181312 bajtova (0x2c440) |
| App particija | 0x100000, preostalo 0xd3bc0 (83%) |
| SHA256 aplikacije | `70060a6dc2e75b4a60241302c5bbdd41d1e85f6c5edb36fb7726591d51fe9d11` |
| Flash konfiguracija | Deklariranih 16 MB, DIO, 80 MHz; nije fizički izmjereno |
| PSRAM | Isključena u M0 buildu |
| CPU i chip revizija | IDF default: 400 MHz, min 3.1 / max 3.99 |
| GPIO/net provjera | Audio i SD uspoređeni sa shemom i demo konfiguracijama |
| Audio/USB/Wi-Fi/SD test | Nije izveden |

## Ograničenje builda

Ovaj binarni artefakt potvrđuje kompilaciju, a ne kompatibilnost sa stvarnim
primjerkom ploče. Lokalni IDF `components/esp_system/port/soc/esp32p4/Kconfig.cpu`
veže 360 MHz uz odabir revizije prije v3, a novijem izboru daje 400 MHz.
Ne biramo raniji ili noviji silicij na temelju naziva direktorija `DEV0`.
COM6 provjerom očitana je revizija v1.3 i usklađeni su `sdkconfig.defaults`
na v1.0-v1.99 i 360 MHz. M0 runtime nije testiran ni na jednoj reviziji.

Za otvorene hardverske točke i sljedeći korak pogledati
[M0 audit](M0_HARDWARE_BSP_AUDIT.md).
