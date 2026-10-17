# Compiles every shaders/{vs,ps,cs,rs}_*.hlsl with DXC into <bin>/shaders/<name>.cso.
# The file prefix picks the profile:
#   vs_/ps_/cs_  shader stage, Shader Model 6.6, entry point main
#   rs_          root signature only (rootsig_1_1); the entry is the #define that holds the root
#                signature string, GLOBAL_ROOT_SIGNATURE (common/root_signature.hlsli)
# A shader error or warning fails the build.

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsl")
file(GLOB_RECURSE SHADER_INCLUDES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hlsli")

set(SHADER_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
set(COMPILED_SHADERS)

foreach (SHADER ${SHADER_SOURCES})
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

    set(OUT_CSO "${SHADER_OUTPUT_DIR}/${NAME_WE}.cso")
    add_custom_command(
            OUTPUT "${OUT_CSO}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
            COMMAND "${DIRECTX_DXC_TOOL}" -nologo -T ${PROFILE} -E ${ENTRY} -WX
            -I "${CMAKE_CURRENT_SOURCE_DIR}"
            # Debug: no optimisation, debug info embedded in the .cso so PIX can show HLSL source.
            "$<$<BOOL:${HAS_CODE}>:$<IF:$<CONFIG:Debug>,-Od;-Zi;-Qembed_debug,-O3>>"
            -Fo "${OUT_CSO}" "${SHADER}"
            DEPENDS "${SHADER}" ${SHADER_INCLUDES}
            COMMENT "DXC ${PROFILE}: ${NAME_WE}.hlsl"
            COMMAND_EXPAND_LISTS
            VERBATIM)
    list(APPEND COMPILED_SHADERS "${OUT_CSO}")
endforeach ()

add_custom_target(compile_shaders ALL DEPENDS ${COMPILED_SHADERS} SOURCES ${SHADER_SOURCES} ${SHADER_INCLUDES})
