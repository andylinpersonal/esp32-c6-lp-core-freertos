enable_language(C CXX ASM)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# Print size
macro(ulp_print_size ulp_app)
    set(CMAKE_SIZE "riscv32-esp-elf-size")
    add_custom_command(TARGET ${ulp_app} POST_BUILD
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_SIZE} "$<TARGET_FILE:${ulp_app}>"
        VERBATIM)
endmacro(ulp_print_size)

macro(ulp_apply_default_sources_original ulp_app_name)
    ulp_apply_default_sources(${ulp_app_name})
endmacro(ulp_apply_default_sources_original)

# Export include paths to  vaariables
macro(__get_component_include_path)
    # Remove HP Core's freertos headers
    foreach(include ${COMPONENT_INCLUDES})
        if(${include} MATCHES "${IDF_PATH}/components/freertos/")
            continue()
        endif()

        list(APPEND component_includes   -I${include})
        list(APPEND ULP_COMPONENT_INCLUDES ${include})
    endforeach()

    list(REMOVE_DUPLICATES component_includes)
    list(REMOVE_DUPLICATES ULP_COMPONENT_INCLUDES)

    get_filename_component(sdkconfig_dir ${SDKCONFIG_HEADER} DIRECTORY)
endmacro(__get_component_include_path)

function(ulp_apply_default_sources ulp_app_name)
    if(NOT CONFIG_ULP_LP_CORE_FREERTOS)
        ulp_apply_default_sources_original(${ulp_app_name})
        return()
    endif(NOT CONFIG_ULP_LP_CORE_FREERTOS)

    # See ${IDF_PATH}/components/ulp/cmake/IDFULPProject.cmake
    function(create_arg_file arguments output_file)
        # Escape all spaces
        list(TRANSFORM arguments REPLACE " " "\\\\ ")

        # Create a single string with all args separated by space
        list(JOIN arguments " " arguments)

        # Write it into the response file
        file(WRITE ${output_file} ${arguments})
    endfunction()

    message(STATUS "Building ULP app ${ulp_app_name}")

    __get_component_include_path()

    list(APPEND ULP_PREPRO_ARGS ${component_includes})
    list(APPEND ULP_PREPRO_ARGS -I${COMPONENT_DIR})
    list(APPEND ULP_PREPRO_ARGS -I${sdkconfig_dir})
    list(APPEND ULP_PREPRO_ARGS -I${IDF_PATH}/components/esp_system/ld)

    target_include_directories(${ulp_app_name} PRIVATE ${ULP_COMPONENT_INCLUDES})

    # Pre-process the linker script
    set(ULP_LD_TEMPLATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../freertos/ld/lp_core_riscv.ld")

    get_filename_component(ULP_LD_SCRIPT ${ULP_LD_TEMPLATE} NAME)

    # Put all arguments to the list
    set(preprocessor_args -D__ASSEMBLER__ -E -P -xc -o ${ULP_LD_SCRIPT} ${ULP_PREPRO_ARGS} ${ULP_LD_TEMPLATE})
    set(compiler_arguments_file ${CMAKE_CURRENT_BINARY_DIR}/${ULP_LD_SCRIPT}_args.txt)
    create_arg_file("${preprocessor_args}" "${compiler_arguments_file}")

    add_custom_command(OUTPUT ${ULP_LD_SCRIPT}
        COMMAND ${CMAKE_C_COMPILER} @${compiler_arguments_file}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        MAIN_DEPENDENCY ${ULP_LD_TEMPLATE}
        DEPENDS ${SDKCONFIG_HEADER}
        COMMENT "Generating ${ULP_LD_SCRIPT} linker script..."
        VERBATIM)
    add_custom_target(ld_script DEPENDS ${ULP_LD_SCRIPT})
    add_dependencies(${ulp_app_name} ld_script)
    target_link_options(${ulp_app_name} PRIVATE SHELL:-T ${CMAKE_CURRENT_BINARY_DIR}/${ULP_LD_SCRIPT})

    # To avoid warning "Manually-specified variables were not used by the project"
    set(bypassWarning "${IDF_TARGET}")

    list(APPEND ULP_S_SOURCES
        "${IDF_PATH}/components/ulp/lp_core/lp_core/start.S"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/vector.S"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/port/${IDF_TARGET}/vector_table.S"
        "${IDF_PATH}/components/ulp/lp_core/shared/ulp_lp_core_memory_shared.c"
        "${IDF_PATH}/components/ulp/lp_core/shared/ulp_lp_core_lp_timer_shared.c"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/lp_core_startup.c"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/lp_core_utils.c"
        "${IDF_PATH}/components/hal/uart_hal_iram.c"
        "${IDF_PATH}/components/hal/uart_hal.c"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/lp_core_uart.c"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/lp_core_print.c"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/lp_core_panic.c"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/lp_core_interrupt.c"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/lp_core_i2c.c"
        "${IDF_PATH}/components/ulp/lp_core/lp_core/lp_core_spi.c")

    # Override files
    list(APPEND ULP_S_SOURCES
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../freertos/libc/abort.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../freertos/libc/int64.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../freertos/libc/string.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../freertos/lp_core/lp_core_interrupt.c")

    set(target_folder ${IDF_TARGET})

    target_link_options(${ulp_app_name}
        PRIVATE SHELL:-T ${IDF_PATH}/components/soc/${target_folder}/ld/${IDF_TARGET}.peripherals.ld)

    if(CONFIG_ESP_ROM_HAS_LP_ROM)
        target_link_options(${ulp_app_name}
            PRIVATE SHELL:-T ${IDF_PATH}/components/esp_rom/${IDF_TARGET}/ld/${IDF_TARGET}lp.rom.ld)
        target_link_options(${ulp_app_name}
            PRIVATE SHELL:-T ${IDF_PATH}/components/esp_rom/${IDF_TARGET}/ld/${IDF_TARGET}lp.rom.newlib.ld)
        target_link_options(${ulp_app_name}
            PRIVATE SHELL:-T ${IDF_PATH}/components/esp_rom/${IDF_TARGET}/ld/${IDF_TARGET}lp.rom.version.ld)
        target_link_options(${ULP_APP_NAME}
            PRIVATE SHELL:-T ${IDF_PATH}/components/esp_rom/${IDF_TARGET}/ld/${IDF_TARGET}lp.rom.api.ld)
    endif()

    target_sources(${ulp_app_name} PRIVATE ${ULP_S_SOURCES})
    target_include_directories(${ulp_app_name} PRIVATE
        "${IDF_PATH}/components/ulp/lp_core/lp_core/include"
        "${IDF_PATH}/components/ulp/lp_core/shared/include")
    target_compile_definitions(${ulp_app_name} PRIVATE IS_ULP_COCPU)

    # Override options for source files
    set_source_files_properties(
        "${IDF_PATH}/components/ulp/lp_core/lp_core/lp_core_utils.c"
        PROPERTIES
            COMPILE_DEFINITIONS "abort=abort__replaced")

    set_source_files_properties(
        "${IDF_PATH}/components/ulp/lp_core/lp_core/vector.S"
        PROPERTIES
            COMPILE_DEFINITIONS "_panic_handler=_panic_handler__real;_interrupt_handler=_interrupt_handler__replaced")
endfunction()

function(ulp_add_freertos ulp_app)
    set(CONFIG_ULP_LP_CORE_FREERTOS ON PARENT_SCOPE)
    target_compile_definitions(${ulp_app} PUBLIC -DCONFIG_ULP_LP_CORE_FREERTOS=1)

    __get_component_include_path()
    add_subdirectory("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../freertos" "${CMAKE_CURRENT_BINARY_DIR}/FreeRTOS")

    target_link_libraries(${ulp_app} PUBLIC freertos_kernel)
endfunction(ulp_add_freertos)
