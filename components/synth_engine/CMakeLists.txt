idf_component_register(SRCS "synth_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES audio_hal usb_midi_host esp_driver_i2s esp_timer esp_hw_support freertos log)
