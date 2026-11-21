set(EVOLVING_GARDEN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

idf_component_register(
    SRCS
        "main.cpp"
        "audio_controller.cpp"
        "${EVOLVING_GARDEN_ROOT}/app/creature_app.cpp"
        "${EVOLVING_GARDEN_ROOT}/platform/esp32/motion.cpp"
    INCLUDE_DIRS
        "."
        "${EVOLVING_GARDEN_ROOT}"
    REQUIRES
        spiffs
        esp_codec_dev
        qmi8658
)

spiffs_create_partition_image(
    storage
    "${EVOLVING_GARDEN_ROOT}/assets/sounds"
    FLASH_IN_PROJECT
)
