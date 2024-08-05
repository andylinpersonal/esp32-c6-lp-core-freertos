cmake_minimum_required(VERSION 3.16)

# Project/target name is passed from the main project to allow IDF to have a dependency on this target
# as well as embed the binary into the main app
project(${ULP_APP_NAME})
add_executable(${ULP_APP_NAME} main.c)

# Set custom compile flags
add_compile_options(-msave-restore -fstack-usage)

# Import the ULP project helper functions
include(IDFULPProject)
include(cmake/ulp_tools.cmake)

# Apply default compile options
ulp_apply_default_options(${ULP_APP_NAME})

# Apply default sources provided by the IDF ULP component
ulp_add_freertos(${ULP_APP_NAME})
ulp_apply_default_sources(${ULP_APP_NAME})

# Add targets for building the binary, as well as the linkerscript which exports ULP shared variables to the main app
ulp_add_build_binary_targets(${ULP_APP_NAME})

ulp_print_size(${ULP_APP_NAME})
