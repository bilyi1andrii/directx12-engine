# Helpers shared by all engine and app targets.

# Strict warnings and common Win32 defines.
function(engine_target_defaults target)
    target_compile_options(${target} PRIVATE
            /W4 /permissive- /WX
            /utf-8
            # Headers included with <...> (Windows SDK, vcpkg packages) are third-party: no warnings from them.
            /external:anglebrackets /external:W0)
    target_link_options(${target} PRIVATE /WX)
    target_compile_definitions(${target} PRIVATE
            UNICODE _UNICODE        # wide-char Win32 APIs
            NOMINMAX                # stop windows.h from defining min/max macros
            WIN32_LEAN_AND_MEAN)
endfunction()

# Agility SDK: ships a newer D3D12 runtime (D3D12Core.dll) than the one in Windows.
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
