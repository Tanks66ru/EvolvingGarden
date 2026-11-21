set(EVOLVING_GARDEN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS
        "."
        "${EVOLVING_GARDEN_ROOT}"
)
