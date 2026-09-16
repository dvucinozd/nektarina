idf_component_register(SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES audio_hal usb_midi_host synth_engine esp_driver_i2s esp_driver_gpio esp_hw_support freertos log)
