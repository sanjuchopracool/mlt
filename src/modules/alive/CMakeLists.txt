add_library(alive MODULE
  factory.c
  producer_alive.c
)

file(GLOB YML "*.yml")
add_custom_target(Other_alive_Files SOURCES
  ${YML}
)

target_compile_options(alive PRIVATE ${MLT_COMPILE_OPTIONS})

target_link_libraries(alive PRIVATE mlt)

set_target_properties(alive PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${MLT_MODULE_OUTPUT_DIRECTORY}")

install(TARGETS alive LIBRARY DESTINATION ${MLT_INSTALL_MODULE_DIR})

install(FILES
  producer_alive.yml
  DESTINATION ${MLT_INSTALL_DATA_DIR}/decklink
)
