# Compiles every shaders/{vs,ps,cs,rs}_*.hlsl with DXC into <bin>/shaders/<name>.cso.
# The file prefix picks the profile:
#   vs_/ps_/cs_  shader stage, Shader Model 6.6, entry point main
#   rs_          root signature only (rootsig_1_1); the entry is the #define that holds the root
#                signature string, GLOBAL_ROOT_SIGNATURE (common/root_signature.hlsli)
# A shader error or warning fails the build.
#
# SHADER_VARIANTS compiles some shaders a second time with a #define, each into its own .cso. A PSO picks a variant
# by its file name. Each entry is three values: the source's name, the variant's name, the define.

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsl")
file(GLOB_RECURSE SHADER_INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsli")

set(SHADER_VARIANTS
        # Discards the pixels whose alpha is below the material's cutoff
        ps_light ps_light_alpha_test ALPHA_TEST=1
)

set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
set(COMPILED_SHADERS)

# Compiles SHADER (a path) into <SHADER_OUTPUT_DIR>/<OUT_NAME>.cso, with DEFINE (NAME=VALUE) if it is not empty.
# Appends the .cso to COMPILED_SHADERS.
function(compile_shader SHADER OUT_NAME DEFINE)
    get_filename_component(NAME_WE "${SHADER}" NAME_WE)

    if (NAME_WE MATCHES "^(vs|ps|cs)_")
        set(PROFILE "${CMAKE_MATCH_1}_6_6")
        set(ENTRY main)
        set(HAS_CODE 1)
    elseif (NAME_WE MATCHES "^rs_")
        set(PROFILE rootsig_1_1)
        set(ENTRY GLOBAL_ROOT_SIGNATURE)
        set(HAS_CODE 0) # no code to optimise or debug; DXC warns about -Zi here
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
            # Debug: no optimisation, debug info embedded in the .cso so PIX can show HLSL source.
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
math(EXPR LAST_VARIANT "${VARIANT_VALUES} - 3")
foreach (I RANGE 0 ${LAST_VARIANT} 3)
    math(EXPR NAME_INDEX "${I} + 1")
    math(EXPR DEFINE_INDEX "${I} + 2")
    list(GET SHADER_VARIANTS ${I} SOURCE_NAME)
    list(GET SHADER_VARIANTS ${NAME_INDEX} VARIANT_NAME)
    list(GET SHADER_VARIANTS ${DEFINE_INDEX} VARIANT_DEFINE)

    set(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_NAME}.hlsl")
    if (NOT EXISTS "${SOURCE}" OR EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${VARIANT_NAME}.hlsl")
        message(FATAL_ERROR "Shader variant ${VARIANT_NAME}: needs ${SOURCE_NAME}.hlsl and a name no source has")
    endif ()
    compile_shader("${SOURCE}" "${VARIANT_NAME}" "${VARIANT_DEFINE}")
endforeach ()

add_custom_target(compile_shaders ALL DEPENDS ${COMPILED_SHADERS} SOURCES ${SHADER_SOURCES} ${SHADER_INCLUDES})
