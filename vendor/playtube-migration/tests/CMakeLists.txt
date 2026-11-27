file(GLOB PLAYTUBE_UNIT_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/unit/*_test.cpp)
add_executable(playtube_tests unit/main.cpp ${PLAYTUBE_UNIT_TESTS})
target_link_libraries(playtube_tests PRIVATE playtube_core doctest)
target_include_directories(playtube_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(playtube_tests PRIVATE PLAYTUBE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    PLAYTUBE_PROJECT_VERSION="${PROJECT_VERSION}")
playtube_set_warnings(playtube_tests)
add_library(playtube_test_support STATIC support/fake_server.cpp support/fake_youtube.cpp)
target_link_libraries(playtube_test_support PUBLIC playtube_net httplib stb)
target_include_directories(playtube_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(playtube_test_support PUBLIC PLAYTUBE_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
playtube_set_warnings(playtube_test_support)
add_executable(fake_youtube support/fake_youtube_main.cpp)
target_link_libraries(fake_youtube PRIVATE playtube_test_support)
playtube_set_warnings(fake_youtube)
target_link_libraries(playtube_tests PRIVATE playtube_test_support stb)
add_test(NAME unit COMMAND playtube_tests)


set(PLAYTUBE_HEADLESS_ENV "SDL_VIDEODRIVER=offscreen;MESA_LOADER_DRIVER_OVERRIDE=llvmpipe")
add_test(NAME app_smoke COMMAND playtube --hidden --frames 5 --data-dir ${CMAKE_BINARY_DIR}/test-data/smoke)
set_tests_properties(app_smoke PROPERTIES ENVIRONMENT "${PLAYTUBE_HEADLESS_ENV}" TIMEOUT 60)

add_test(NAME ui_gallery
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/ui/run.sh $<TARGET_FILE:playtube> $<TARGET_FILE:playtube_imgdiff>
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/gallery.script ${CMAKE_CURRENT_SOURCE_DIR}/ui/references/gallery
        ${CMAKE_BINARY_DIR}/ui-out/gallery --screen gallery)
set_tests_properties(ui_gallery PROPERTIES TIMEOUT 180)

# the same script through NanoVG's GLES2 backend: the references are its output, the software renderer must match
add_test(NAME ui_gallery_gl
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/ui/run.sh $<TARGET_FILE:playtube> $<TARGET_FILE:playtube_imgdiff>
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/gallery.script ${CMAKE_CURRENT_SOURCE_DIR}/ui/references/gallery
        ${CMAKE_BINARY_DIR}/ui-out/gallery-gl --screen gallery --renderer gl)
set_tests_properties(ui_gallery_gl PROPERTIES TIMEOUT 180)

# end to end: the shell against the fake YouTube (TLS, test CA)
add_test(NAME ui_shell
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/ui/run_shell.sh $<TARGET_FILE:playtube> $<TARGET_FILE:playtube_imgdiff>
        $<TARGET_FILE:fake_youtube> ${CMAKE_CURRENT_SOURCE_DIR}/ui/shell.script
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/references/shell ${CMAKE_BINARY_DIR}/ui-out/shell)
set_tests_properties(ui_shell PROPERTIES TIMEOUT 180)

# signed in (tests/ui/signed.account): the feeds
add_test(NAME ui_signed
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/ui/run_shell.sh $<TARGET_FILE:playtube> $<TARGET_FILE:playtube_imgdiff>
        $<TARGET_FILE:fake_youtube> ${CMAKE_CURRENT_SOURCE_DIR}/ui/signed.script
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/references/signed ${CMAKE_BINARY_DIR}/ui-out/signed)
set_tests_properties(ui_signed PROPERTIES TIMEOUT 180)

# the player on the test clip served by the fake (tests/ui/player.fake: --playable)
add_test(NAME ui_player
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/ui/run_shell.sh $<TARGET_FILE:playtube> $<TARGET_FILE:playtube_imgdiff>
        $<TARGET_FILE:fake_youtube> ${CMAKE_CURRENT_SOURCE_DIR}/ui/player.script
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/references/player ${CMAKE_BINARY_DIR}/ui-out/player)
set_tests_properties(ui_player PROPERTIES TIMEOUT 180)

# no usable data folder: the app still starts (nothing is saved) instead of quitting
add_test(NAME app_without_data_folder
    COMMAND playtube --hidden --frames 5 --data-dir /dev/null/playtube)
set_tests_properties(app_without_data_folder PROPERTIES ENVIRONMENT "${PLAYTUBE_HEADLESS_ENV}" TIMEOUT 60)

# a test script without an output folder would write its screenshots to "/": refused before anything starts
add_test(NAME cli_test_output_required
    COMMAND playtube --hidden --test-script ${CMAKE_CURRENT_SOURCE_DIR}/ui/gallery.script
        --data-dir ${CMAKE_BINARY_DIR}/test-data/cli)
set_tests_properties(cli_test_output_required PROPERTIES ENVIRONMENT "${PLAYTUBE_HEADLESS_ENV}" TIMEOUT 30
    PASS_REGULAR_EXPRESSION "--test-script needs --test-output")

add_test(NAME init_failure_logged
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli/init_failure_logged.sh $<TARGET_FILE:playtube>
        ${CMAKE_BINARY_DIR}/test-data/init-failure)
set_tests_properties(init_failure_logged PROPERTIES TIMEOUT 60)
