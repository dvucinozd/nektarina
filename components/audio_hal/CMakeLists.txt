idf_component_register(SRCS "audio_hal.c" INCLUDE_DIRS "include"
    REQUIRES board_jc_esp32p4_m3 esp_driver_i2c esp_driver_i2s esp_timer esp_codec_dev)
