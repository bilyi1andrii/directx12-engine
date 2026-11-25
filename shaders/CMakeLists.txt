# Compiles every shader in this folder into <bin>/shaders/<name>.cso.
# The file name's prefix picks the stage (vs_, ps_, cs_) or marks a root signature (rs_).
# SHADER_VARIANTS compiles some shaders again with a #define, each into its own file per entry.

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsl")
file(GLOB_RECURSE SHADER_INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsli")

set(SHADER_VARIANTS
        ps_light ps_light_alpha_test ALPHA_TEST=1
        ps_light ps_light_blend ALPHA_BLEND=1
)

set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
set(COMPILED_SHADERS)

function(compile_shader SHADER OUT_NAME DEFINE)
    get_filename_component(NAME_WE "${SHADER}" NAME_WE)

    if (NAME_WE MATCHES "^(vs|ps|cs)_")
        set(PROFILE "${CMAKE_MATCH_1}_6_6")
        set(ENTRY main)
        set(HAS_CODE 1)
    elseif (NAME_WE MATCHES "^rs_")
        set(PROFILE rootsig_1_1)
        set(ENTRY GLOBAL_ROOT_SIGNATURE)
        set(HAS_CODE 0)
    else ()
        message(FATAL_ERROR "Unknown shader stage (expected vs_/ps_/cs_/rs_ prefix): ${SHADER}")
    endif ()

    set(DEFINE_ARGS)
    if (DEFINE)
        set(DEFINE_ARGS -D "${DEFINE}")
    endif ()

    set(OUT_CSO "${SHADER_OUTPUT_DIR}/${OUT_NAME}.cso")
    add_custom_command(
            OUTPUT "${OUT_CSO}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
            COMMAND "${DIRECTX_DXC_TOOL}" -nologo -T ${PROFILE} -E ${ENTRY} -WX
            -I "${CMAKE_CURRENT_SOURCE_DIR}"
            ${DEFINE_ARGS}
            "$<$<BOOL:${HAS_CODE}>:$<IF:$<CONFIG:Debug>,-Od;-Zi;-Qembed_debug,-O3>>"
            -Fo "${OUT_CSO}" "${SHADER}"
            DEPENDS "${SHADER}" ${SHADER_INCLUDES}
            COMMENT "DXC ${PROFILE}: ${OUT_NAME}.cso"
            COMMAND_EXPAND_LISTS
            VERBATIM)
    set(COMPILED_SHADERS ${COMPILED_SHADERS} "${OUT_CSO}" PARENT_SCOPE)
endfunction()

foreach (SHADER ${SHADER_SOURCES})
    get_filename_component(NAME_WE "${SHADER}" NAME_WE)
    compile_shader("${SHADER}" "${NAME_WE}" "")
endforeach ()

list(LENGTH SHADER_VARIANTS VARIANT_VALUES)
math(EXPR VARIANT_REMAINDER "${VARIANT_VALUES} % 3")
if (NOT VARIANT_REMAINDER EQUAL 0)
    message(FATAL_ERROR "SHADER_VARIANTS needs three values per variant (source, variant name, define), "
            "but has ${VARIANT_VALUES} values: ${SHADER_VARIANTS}")
endif ()
set(REMAINING_VARIANTS ${SHADER_VARIANTS})
while (REMAINING_VARIANTS)
    list(POP_FRONT REMAINING_VARIANTS SOURCE_NAME VARIANT_NAME VARIANT_DEFINE)

    set(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_NAME}.hlsl")
    if (NOT EXISTS "${SOURCE}" OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${VARIANT_NAME}.hlsl")
        message(FATAL_ERROR "Shader variant ${VARIANT_NAME}: needs ${SOURCE_NAME}.hlsl and a name no source has")
    endif ()
    compile_shader("${SOURCE}" "${VARIANT_NAME}" "${VARIANT_DEFINE}")
endwhile ()

add_custom_target(compile_shaders ALL DEPENDS ${COMPILED_SHADERS} SOURCES ${SHADER_SOURCES} ${SHADER_INCLUDES})
