# M2 USB MIDI hardverski test

**Datum:** 2026-09-20

**Uređaj:** ESP32-S3-WROOM-1-N16R8 na COM3

**MIDI kontroler:** VID `0x2467`, PID `0x2033`

## Rezultat

- PASS: firmware je flashan i svi segmenti prošli su SHA provjeru.
- PASS: boot potvrđuje CPU 240 MHz, flash DIO/80 MHz/16 MB i 8 MB Octal PSRAM/80 MHz.
- PASS: USB host pronalazi Audio/MIDIStreaming interface `0x01/0x03` i bulk-IN endpoint `0x81`, MPS 64.
- PASS: Note On/Off i različite velocity vrijednosti stižu do synth enginea; broj aktivnih glasova prati note.
- PASS: reconnect nakon odspajanja radi bez restarta firmwarea.
- PASS: hot-unplug tijekom aktivne note pokreće All Notes Off; glasovi se vraćaju na nulu bez crasha.
- PASS: tijekom testa `dropped=0`, `I2S err=0`, `short=0`; heap se nakon cleanup-a vraća na početnu vrijednost.

## Korekcija otkrivena testom

Prvi hot-unplug vratio je `USB_TRANSFER_STATUS_ERROR` prije događaja `DEV_GONE`. Driver je pokušao resubmitati već nevažeći transfer i dobio `ESP_ERR_INVALID_STATE`. Callback je promijenjen tako da svaki neuspjeli bulk transfer odmah pokreće odgođeni cleanup bez ponovnog slanja. Ponovljeni reconnect/hot-unplug test prošao je bez URB greške.

## Preostalo

- Akustička potvrda izlaza i ponašanja SD_MODE pina.
- Pitch bend, mod wheel i ostale CC kontrole.
- Najmanje 25 uzastopnih hot-plug ciklusa.
- Tridesetominutni soak test uz praćenje heap/stack i audio brojača.
