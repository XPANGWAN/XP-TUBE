cmake_minimum_required(VERSION 3.20)

option(PLATFORM_PS4 "Build for the PS4 with the OpenOrbis toolchain" OFF)
if(PLATFORM_PS4 AND NOT CMAKE_TOOLCHAIN_FILE)
    if(NOT DEFINED ENV{OPENORBIS})
        message(FATAL_ERROR "OPENORBIS is not set")
    endif()
    set(CMAKE_TOOLCHAIN_FILE "$ENV{OPENORBIS}/cmake/ps4.cmake" CACHE FILEPATH "")
endif()

project(Playtube VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PLAYTUBE_DEV "Development app (PLTB00001, folder PlaytubeDev) installed beside the release app" OFF)
# Diagnostic builds only (never a release): the app closes by itself a second after its first frame, so that a
# script can start it again and again (to look for a crash that shows once in many starts).
option(PLAYTUBE_START_LOOP "Diagnostic: close a second after the first frame" OFF)
if(PLAYTUBE_START_LOOP)
    add_compile_definitions(PLAYTUBE_START_LOOP=1)
endif()
if(PLAYTUBE_DEV)
    add_compile_definitions(PLAYTUBE_DEV=1)
endif()
option(PLAYTUBE_TESTS "Build the unit tests and the UI test harness" ON)
option(PLAYTUBE_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(PLAYTUBE_FUZZ "Build the libFuzzer targets (clang)" OFF)
if(PLATFORM_PS4)
    # our copy of the toolchain's linker script (keeps .data.* and prioritised constructors, see the file)
    string(REPLACE "$ENV{OPENORBIS}/link.x" "${CMAKE_SOURCE_DIR}/scripts/ps4/link.x" CMAKE_EXE_LINKER_FLAGS
                   "${CMAKE_EXE_LINKER_FLAGS}")
    if(NOT CMAKE_EXE_LINKER_FLAGS MATCHES "scripts/ps4/link.x")
        message(FATAL_ERROR "the toolchain's linker script option was not found: ${CMAKE_EXE_LINKER_FLAGS}")
    endif()
    # the PS4 CPU (AMD Jaguar): SSE4.1 rounding instead of libm calls, AVX; every PS4 has this CPU
    add_compile_options(-march=btver2)
    set(PLAYTUBE_TESTS OFF CACHE BOOL "" FORCE)
    set(PLAYTUBE_FUZZ OFF CACHE BOOL "" FORCE)
endif()

include(cmake/warnings.cmake)

if(PLAYTUBE_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()
if(PLAYTUBE_FUZZ)
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
include(cmake/dependencies.cmake)

add_subdirectory(third_party)

# core utilities: no SDL, no network
add_library(playtube_base STATIC
    src/core/boot_trace.cpp
    src/core/version.cpp
    src/core/redact.cpp
    src/core/data_dir.cpp
    src/core/file_io.cpp
    src/core/json.cpp
    src/core/log.cpp
    src/core/result.cpp
    src/core/signals.cpp
    src/core/strings.cpp
    src/core/task.cpp
    src/core/utf.cpp
)
target_include_directories(playtube_base PUBLIC src)
target_compile_definitions(playtube_base PRIVATE PLAYTUBE_VERSION="${PROJECT_VERSION}")
if(PLATFORM_PS4)
    target_compile_definitions(playtube_base PUBLIC PLAYTUBE_PS4=1)
endif()
target_link_libraries(playtube_base PUBLIC Threads::Threads yyjson)
playtube_set_warnings(playtube_base)

# network: URLs, HTTP over curl, certificate hashing; depends on core only (+ curl, crypto)
add_library(playtube_net STATIC
    src/net/url.cpp
    src/net/crypto.cpp
    src/net/http.cpp
)
if(PLATFORM_PS4)
    target_sources(playtube_net PRIVATE src/net/crypto_mbedtls.cpp)
else()
    target_sources(playtube_net PRIVATE src/net/crypto_openssl.cpp)
endif()
target_link_libraries(playtube_net PUBLIC playtube_base PRIVATE playtube_curl playtube_crypto)
playtube_set_warnings(playtube_net)

# window, input, rendering helpers, system keyboard
# CPU rendering (no SDL, no GL): the NanoVG software backend and its rasterizer
add_library(playtube_render STATIC
    src/render/nvg_sw.cpp
    src/render/raster.cpp
    src/render/composite.cpp
)
target_link_libraries(playtube_render PUBLIC playtube_base nanovg)
# the per-pixel hot path: optimised in debug builds too (tests and Linux runs stay usable)
target_compile_options(playtube_render PRIVATE -O2)
playtube_set_warnings(playtube_render)

add_library(playtube_platform STATIC
    src/platform/input.cpp
    src/platform/events.cpp
    src/platform/remote.cpp
    src/platform/language.cpp
    src/platform/sdl_platform.cpp
    src/render/canvas.cpp
)
if(PLATFORM_PS4)
    target_sources(playtube_platform PRIVATE src/platform/ps4/backend_ps4.cpp src/platform/ps4/firmware_shaders.cpp
        src/platform/ps4/present_ps4.cpp src/platform/ps4/text_input_ps4.cpp
        src/platform/ps4/remote_ps4.cpp)
    option(PLAYTUBE_GPU_PROBE "PS4: log GPU and firmware shader diagnostics at startup" OFF)
    if(PLAYTUBE_GPU_PROBE)
        target_sources(playtube_platform PRIVATE src/platform/ps4/gpu_probe_ps4.cpp)
        target_compile_definitions(playtube_platform PRIVATE PLAYTUBE_GPU_PROBE=1)
    endif()
else()
    target_sources(playtube_platform PRIVATE src/platform/gl_present.cpp src/platform/linux/backend_linux.cpp
        src/platform/linux/text_input_linux.cpp)
endif()
target_link_libraries(playtube_platform PUBLIC playtube_base playtube_render playtube_sdl2 nanovg stb)
playtube_set_warnings(playtube_platform)

# UI toolkit: depends on the platform layer only (never on the network library)
add_library(playtube_ui STATIC
    src/ui/theme.cpp
    src/ui/halo.cpp
    src/ui/icon_button.cpp
    src/ui/sprites.cpp
    src/ui/svg_shapes.cpp
    src/ui/animation.cpp
    src/ui/text.cpp
    src/ui/bidi.cpp
    src/ui/i18n.cpp
    src/ui/context.cpp
    src/ui/view.cpp
    src/ui/box.cpp
    src/ui/label.cpp
    src/ui/focus.cpp
    src/ui/scroll.cpp
    src/ui/list.cpp
    src/ui/reveal.cpp
    src/ui/images.cpp
    src/ui/image_view.cpp
    src/ui/widgets.cpp
    src/ui/hints.cpp
    src/ui/application.cpp
    src/ui/nvg_backend.cpp
)
target_link_libraries(playtube_ui PUBLIC playtube_platform yyjson playtube_fribidi)
playtube_set_warnings(playtube_ui)

# video playback: libmpv with its software renderer (no UI)
add_library(playtube_player STATIC
    src/player/engine.cpp
    src/player/repeats.cpp
    src/player/seek.cpp
    src/player/trickplay.cpp
    src/player/webp.cpp
)
target_link_libraries(playtube_player PUBLIC playtube_base PRIVATE playtube_mpv playtube_av)
playtube_set_warnings(playtube_player)

# YouTube: its private API, sign-in and SponsorBlock
add_library(playtube_youtube STATIC
    src/youtube/account.cpp
    src/youtube/auth.cpp
    src/youtube/client.cpp
    src/youtube/links.cpp
    src/youtube/parse.cpp
    src/youtube/playback.cpp
    src/youtube/streams.cpp
    src/youtube/storyboard.cpp
    src/youtube/search_filters.cpp
)
target_link_libraries(playtube_youtube PUBLIC playtube_net)
playtube_set_warnings(playtube_youtube)

# app, screens and test harness
add_library(playtube_core STATIC
    src/app/app.cpp
    src/app/gallery.cpp
    src/app/settings.cpp
    src/app/update.cpp
    src/app/profiles.cpp
    src/app/progress.cpp
    src/app/shell/cards.cpp
    src/app/shell/channel_screen.cpp
    src/app/shell/playlist_screen.cpp
    src/app/shell/chat_panel.cpp
    src/app/shell/comments_panel.cpp
    src/app/shell/autoplay.cpp
    src/app/shell/chrome.cpp
    src/app/shell/clock.cpp
    src/app/shell/screensaver.cpp
    src/app/shell/trickplay_preview.cpp
    src/app/shell/watch_history.cpp
    src/app/shell/player_screen.cpp
    src/app/shell/player_settings.cpp
    src/app/shell/row_screens.cpp
    src/app/shell/licenses.cpp
    src/app/shell/nav_bar.cpp
    src/app/shell/settings_screen.cpp
    src/app/shell/search_screen.cpp
    src/app/shell/session.cpp
    src/app/shell/signin_screen.cpp
    src/app/shell/states.cpp
    src/app/shell/subscriptions_screen.cpp
    src/app/shell/tab_screens.cpp
    src/app/shell/video_pages.cpp
)
target_link_libraries(playtube_core PUBLIC playtube_ui playtube_net playtube_player playtube_youtube)
playtube_set_warnings(playtube_core)

add_executable(playtube src/main.cpp)
target_link_libraries(playtube PRIVATE playtube_core)
playtube_set_warnings(playtube)
if(PLATFORM_PS4)
    # in the executable itself (not an archive): it must win over libkernel's clock_gettime
    target_sources(playtube PRIVATE src/platform/ps4/clock_ps4.cpp src/platform/ps4/socket_ps4.cpp
                                    src/platform/ps4/resolver_ps4.cpp)
    # every socket()/socketpair() call of the executable (curl included) goes through socket_ps4.cpp, every
    # getaddrinfo() call (FFmpeg's included) through resolver_ps4.cpp
    target_link_options(playtube PRIVATE --wrap=socket --wrap=socketpair --wrap=getaddrinfo)
    include(cmake/ps4_pkg.cmake)
    playtube_add_ps4_pkg(playtube)
endif()
if(NOT PLATFORM_PS4)
    target_sources(playtube_core PRIVATE src/testing/script.cpp src/testing/image.cpp)
    target_compile_definitions(playtube_core PUBLIC PLAYTUBE_TESTING=1)
    add_executable(playtube_imgdiff src/testing/imgdiff_main.cpp)
    target_link_libraries(playtube_imgdiff PRIVATE playtube_core)
    add_executable(playtube_yt_check src/testing/yt_check_main.cpp)
    target_link_libraries(playtube_yt_check PRIVATE playtube_youtube)
    target_compile_definitions(playtube_yt_check PRIVATE
        PLAYTUBE_CA_FILE="${CMAKE_SOURCE_DIR}/resources/certs/cacert.pem")
    playtube_set_warnings(playtube_yt_check)
    playtube_set_warnings(playtube_imgdiff)
    # every build (not only when playtube relinks): an edited string or licence reaches the tests
    add_custom_target(playtube_resources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/fonts $<TARGET_FILE_DIR:playtube>/resources/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/certs $<TARGET_FILE_DIR:playtube>/resources/certs
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/i18n $<TARGET_FILE_DIR:playtube>/resources/i18n
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources/licenses
            $<TARGET_FILE_DIR:playtube>/resources/licenses
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES $<TARGET_FILE_DIR:playtube>/resources/)
    add_dependencies(playtube playtube_resources)
endif()

if(PLAYTUBE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PLAYTUBE_FUZZ)
    enable_testing()
    foreach(name url redact raster image youtube)
        add_executable(fuzz_${name} fuzz/fuzz_${name}.cpp)
        target_link_libraries(fuzz_${name} PRIVATE playtube_net playtube_render playtube_ui playtube_youtube)
        target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer)
        playtube_set_warnings(fuzz_${name})
        # first directory: writable corpus in the build tree; second: the committed seeds (read only)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/${name} ${CMAKE_BINARY_DIR}/fuzz-artifacts)
        add_test(NAME fuzz_${name}
            COMMAND fuzz_${name} -max_total_time=30 -rss_limit_mb=2048
                -artifact_prefix=${CMAKE_BINARY_DIR}/fuzz-artifacts/${name}-
                ${CMAKE_BINARY_DIR}/corpus/${name} ${CMAKE_SOURCE_DIR}/fuzz/seeds/${name})
        set_tests_properties(fuzz_${name} PROPERTIES LABELS fuzz TIMEOUT 120)
    endforeach()
endif()
