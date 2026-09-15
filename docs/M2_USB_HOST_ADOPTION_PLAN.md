# M2 - USB MIDI Host Implementacija i Integracija

**Status:** **M2 Implementiran i Integriran na ESP32-S3**.

---

## 1. Arhitektura USB Host Stoga (`components/usb_midi_host`)

Za razliku od eksperimentalnog P4 koji je zahtijevao vanjske PHY sklopove i preusmjeravanje, ESP32-S3 posjeduje ugrađeni Full-Speed USB-OTG PHY kontroler:
- **D- (GPIO 19)** i **D+ (GPIO 20)**.
- Integriran s ESP-IDF upravljanom komponentom `espressif/usb: "^1.5.0"`.

---

## 2. Implementirani Slojevi
1. **Host Library Task (`usb_lib_task`):**
   - Poziva `usb_host_lib_handle_events` s dinamičkom obradom sistemskih zastavica (`FLAGS_NO_CLIENTS`, `FLAGS_ALL_FREE`).
2. **Host Client Task (`usb_client_task`):**
   - Registrira asinkroni MIDI klijent (`usb_host_client_register`).
   - Obrađuje `USB_HOST_CLIENT_EVENT_NEW_DEV` i `USB_HOST_CLIENT_EVENT_DEV_GONE`.
3. **Deskriptorski Parser i Enumeracija:**
   - Pronalazi konfiguracijski deskriptor uređaja.
   - Skenira Audio / MIDIStreaming sučelja (Class `0x01`, SubClass `0x03`).
   - Preuzima interface (`usb_host_interface_claim`) i pronalazi Bulk/Interrupt IN endpoint.
4. **4-bajtni USB-MIDI Paketni Parser:**
   - Obrađuje standardne 4-bajtne pakete: `Cable/CIN`, `Status`, `Data1`, `Data2`.
   - Izdvaja događaje: `Note On (0x90)`, `Note Off (0x80)`, `Control Change (0xB0)`, `Pitch Bend (0xE0)`.
   - Prosljeđuje strukturirane poruke `midi_message_t` u FreeRTOS Queue prema `synth_task`.
5. **Hot-Plug Otpornost:**
   - Sigurno otpuštanje transfera i interfacea pri odspajanju kabela bez curenja memorije i rušenja sustava.
