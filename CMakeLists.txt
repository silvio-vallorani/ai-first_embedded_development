cmake_minimum_required(VERSION 3.21)
project(ai_first_embedded_development VERSION 0.1.0 LANGUAGES C)

option(AIED_BUILD_TESTS "Build host unit tests" ON)

add_library(aied_boot_core STATIC
    firmware/boot/src/release_manifest.c
    firmware/boot/src/slot_swap.c
)
target_include_directories(aied_boot_core PUBLIC firmware/boot/include)
target_compile_features(aied_boot_core PUBLIC c_std_11)
target_compile_options(aied_boot_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(aied_log STATIC
    firmware/services/log/src/aied_log.c
    firmware/services/log/src/aied_log_file.c
)
target_include_directories(aied_log PUBLIC firmware/services/log/include)
target_compile_features(aied_log PUBLIC c_std_11)
target_compile_options(aied_log PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(AIED_BUILD_TESTS)
    enable_testing()
    add_executable(aied_unit_tests
        tests/unit/test_main.c
        tests/unit/test_release_manifest.c
        tests/unit/test_slot_swap.c
        tests/unit/test_log.c
        tests/unit/test_log_file.c
        tests/mocks/mock_swap_backend.c
        tests/mocks/fake_log_fs.c
    )
    target_include_directories(aied_unit_tests PRIVATE
        firmware/boot/include
        tests/mocks
    )
    target_link_libraries(aied_unit_tests PRIVATE aied_boot_core aied_log)
    target_compile_features(aied_unit_tests PRIVATE c_std_11)
    target_compile_options(aied_unit_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    add_test(NAME aied_unit_tests COMMAND aied_unit_tests)
endif()
