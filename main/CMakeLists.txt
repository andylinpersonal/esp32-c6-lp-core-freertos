idf_component_register(
    SRCS 
        "main.cpp"
    REQUIRES
        "driver"
        "ulp"
    WHOLE_ARCHIVE)

set(ulp_app_name lp_core_freertos)
set(ulp_sources  "ulp/main.c")
set(ulp_exp_dep_srcs "main.cpp")
ulp_embed_binary(${ulp_app_name} "${ulp_sources}" "${ulp_exp_dep_srcs}")
