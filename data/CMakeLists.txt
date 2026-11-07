# data/: small runtime files the apps load, committed to git (unlike the downloaded models in assets/).
# Copied to <bin>/data/ with the same layout, next to the executables and shaders/, so every app finds them
# relative to its own exe.

file(GLOB_RECURSE DATA_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")
list(FILTER DATA_FILES EXCLUDE REGEX "/CMakeLists\\.txt$")

set(DATA_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data")
set(COPIED_DATA_FILES)
foreach (DATA_FILE ${DATA_FILES})
    file(RELATIVE_PATH RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${DATA_FILE}")
    set(OUT_FILE "${DATA_OUTPUT_DIR}/${RELATIVE}")
    get_filename_component(OUT_DIR "${OUT_FILE}" DIRECTORY)
    add_custom_command(
            OUTPUT "${OUT_FILE}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DATA_FILE}" "${OUT_FILE}"
            DEPENDS "${DATA_FILE}"
            COMMENT "Copying data/${RELATIVE}"
            VERBATIM)
    list(APPEND COPIED_DATA_FILES "${OUT_FILE}")
endforeach ()

add_custom_target(copy_data ALL DEPENDS ${COPIED_DATA_FILES} SOURCES ${DATA_FILES})
