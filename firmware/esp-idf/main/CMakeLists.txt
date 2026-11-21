set(EVOLVING_GARDEN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

idf_component_register(
    SRCS
        "main.cpp"
        "audio_controller.cpp"
    INCLUDE_DIRS
        "."
        "${EVOLVING_GARDEN_ROOT}"
    REQUIRES
        spiffs
        esp_codec_dev
)

spiffs_create_partition_image(
    storage
    "${EVOLVING_GARDEN_ROOT}/assets/sounds"
    FLASH_IN_PROJECT
)
