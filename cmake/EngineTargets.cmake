# Helpers shared by all engine and app targets: strict warnings and Win32 defines, and copying the Agility SDK
# and ASan runtimes next to an exe.

function(engine_target_defaults target)
    target_compile_options(${target} PRIVATE
            /W4 /permissive- /WX
            /utf-8
            /external:anglebrackets /external:W0)
    target_link_options(${target} PRIVATE /WX)
    target_compile_definitions(${target} PRIVATE
            UNICODE _UNICODE
            NOMINMAX
            WIN32_LEAN_AND_MEAN)
endfunction()

function(engine_copy_agility_runtime target)
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/D3D12"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Microsoft::DirectX12-Core>"
            "$<$<CONFIG:Debug>:$<TARGET_FILE:Microsoft::DirectX12-Layers>>"
            "$<TARGET_FILE_DIR:${target}>/D3D12/"
            COMMAND_EXPAND_LISTS
            COMMENT "Copying Agility SDK runtime for ${target}")
endfunction()

# The runtime ships beside cl.exe, which is only on PATH in a Developer shell
function(engine_copy_asan_runtime target)
    if(NOT CMAKE_CXX_FLAGS MATCHES "/fsanitize=address")
        return()
    endif()
    cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH compilerDir)
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${compilerDir}/clang_rt.asan_dynamic-x86_64.dll"
            "$<TARGET_FILE_DIR:${target}>"
            COMMENT "Copying ASan runtime for ${target}")
endfunction()
