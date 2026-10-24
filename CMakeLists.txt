# Skate 2 - ReXGlue Recompiled Project, iOS build.
#
# The runtime is the iOS port of the ReXGlue SDK, found through rexglue_DIR
# (see CMakePresets.json). SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(skate2 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Keep build-machine paths (they contain the user name) out of the binary:
# source file names end up in log and assert messages.
add_compile_options("-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=skate2")

include(generated/rexglue.cmake)

add_executable(skate2 src/main.cpp src/game_check.cpp src/frame_rate.cpp)

# The image digest of the default.xex the code was generated from
# (skate2_manifest.toml), for src/game_check.cpp: a disc with any other executable
# is refused instead of run against code that does not match it.
set(SKATE2_XEX "${CMAKE_CURRENT_SOURCE_DIR}/../game/default.xex")
if(NOT EXISTS "${SKATE2_XEX}")
    message(FATAL_ERROR "${SKATE2_XEX} not found: put your copy of the game's default.xex there "
                        "(see README.md).")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SKATE2_XEX}")
file(READ "${SKATE2_XEX}" SKATE2_XEX_SECURITY_OFFSET OFFSET 16 LIMIT 4 HEX)
math(EXPR SKATE2_XEX_DIGEST_OFFSET "0x${SKATE2_XEX_SECURITY_OFFSET} + 0x114")
file(READ "${SKATE2_XEX}" SKATE2_XEX_IMAGE_DIGEST OFFSET ${SKATE2_XEX_DIGEST_OFFSET} LIMIT 20 HEX)
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/skate2_xex_id.h"
     CONTENT "#pragma once\n#define SKATE2_XEX_IMAGE_DIGEST \"${SKATE2_XEX_IMAGE_DIGEST}\"\n")
target_include_directories(skate2 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# skate2_register.cpp is one flat function of function-pointer registrations
# (tens of thousands), run once at startup, in a single basic block. LLVM
# is quadratic on it at -O1 and above, and so is -O0 on arm64, where
# GlobalISel's Localizer pass walks the whole block for every constant. -O0
# with the classic instruction selector compiles it quickly. The -O3
# precompiled header cannot be used at -O0, so it is skipped here.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/generated/default/skate2_register.cpp
    PROPERTIES COMPILE_OPTIONS "-O0;-mllvm;-global-isel=0" SKIP_PRECOMPILE_HEADERS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    target_sources(skate2 PRIVATE src/iso_import.mm src/ios_screen.mm src/touch_layout_skate2.cpp)
    set_source_files_properties(src/iso_import.mm src/ios_screen.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(skate2 PRIVATE "-framework UIKit" "-framework UniformTypeIdentifiers")
    # The GPU plugin is embedded in Frameworks/ below and loaded from there; a
    # loose copy next to the executable would not be covered by the signature.
    rexglue_setup_target(skate2)
else()
    rexglue_setup_target(skate2 GPU_PLUGINS xenos)
endif()

# =============================================================================
#  iOS: a .app bundle that embeds the SDK's runtime dylibs. Game data is not
#  bundled; it goes in the app's Documents/game folder (Files app).
# =============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(SKATE2_IOS_BUNDLE_ID "com.devz.skate2" CACHE STRING "iOS bundle identifier")
    set_target_properties(skate2 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${SKATE2_IOS_BUNDLE_ID}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${SKATE2_IOS_BUNDLE_ID}"
        # The SDK helper sets @executable_path, right for a flat macOS layout;
        # inside a bundle the dylibs live in Frameworks/.
        INSTALL_RPATH "@executable_path/Frameworks"
        BUILD_WITH_INSTALL_RPATH ON
    )
    # iOS only loads dylibs from inside the signed bundle.
    get_filename_component(SKATE2_IOS_SDK_LIB "${rexglue_DIR}/../.." ABSOLUTE)
    set(SKATE2_IOS_EMBED
        "${SKATE2_IOS_SDK_LIB}/librexruntime.dylib"
        "${SKATE2_IOS_SDK_LIB}/librexgpu-xenos.dylib"
        "${SKATE2_IOS_SDK_LIB}/libMoltenVK.dylib")
    add_custom_target(skate2_ios_bundle ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:skate2>/Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SKATE2_IOS_EMBED}
                "$<TARGET_BUNDLE_CONTENT_DIR:skate2>/Frameworks"
        DEPENDS skate2 ${SKATE2_IOS_EMBED}
        COMMENT "Refreshing runtime dylibs in skate2.app/Frameworks"
        VERBATIM)

    # App icons. Without an asset catalog iOS resolves icons by file name from
    # the bundle root (CFBundleIcons in Info.plist), so the PNGs go in flat.
    file(GLOB SKATE2_IOS_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/ios/icons/*.png")
    if(SKATE2_IOS_ICONS)
        add_custom_command(TARGET skate2_ios_bundle POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SKATE2_IOS_ICONS}
                    "$<TARGET_BUNDLE_CONTENT_DIR:skate2>"
            COMMENT "Copying app icons into skate2.app"
            VERBATIM)
    endif()
endif()
