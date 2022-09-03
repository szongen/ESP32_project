idf_component_register(SRCS "APP_Display.c" "E2213JS0C1.c"
                    INCLUDE_DIRS .
                    REQUIRES spi_flash)


target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=missing-braces -Wno-error=parentheses -Wno-error=implicit-function-declaration -Wno-error=return-type -Wno-error=unused-variable -Wno-error=misleading-indentation -Wno-error=pointer-sign -Wno-error=format=-Wno-error=unused-value -Wno-error=maybe-uninitialized -Wno-error=reorder)


