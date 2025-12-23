set(LV_UI_DIR ui)
file(GLOB_RECURSE LV_UI_SOURCES ${LV_UI_DIR}/*.c)
set(APP_DIR app)
file(GLOB_RECURSE APP_SOURCES ${APP_DIR}/*.c)
set(LOTTIE_DIR lottie_player)
file(GLOB_RECURSE LOTTIE_SOURCES ${LOTTIE_DIR}/*.c)

set(ESPBAUD  "2000000")
idf_component_register(

    SRCS "main.c" ${LV_UI_SOURCES} ${APP_SOURCES} ${LOTTIE_SOURCES}

    INCLUDE_DIRS "." ${LV_UI_DIR} ${APP_DIR} ${LOTTIE_DIR})

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format" "-Wno-deprecated-declarations" "-Wno-stringop-truncation")

target_compile_definitions(${COMPONENT_TARGET} PRIVATE "-D RMAKER_DEMO_PROJECT_NAME=\"${CMAKE_PROJECT_NAME}\"")

set_source_files_properties(
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE)

spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)

spiffs_create_partition_assets(
    animation
    "../lottie_assets"
    FLASH_IN_PROJECT
    MMAP_FILE_SUPPORT_FORMAT ".json"
)
