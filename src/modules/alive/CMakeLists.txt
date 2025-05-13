add_library(alive MODULE
  producer_alive.cpp
)

file(GLOB YML "*.yml")
add_custom_target(Other_alive_Files SOURCES
  ${YML}
)

target_compile_options(alive PRIVATE ${MLT_COMPILE_OPTIONS})

target_include_directories(alive PRIVATE /home/sanju/PROJECTS/inae_core/src/c_wrapper/)
target_link_directories(alive PRIVATE /home/sanju/PROJECTS/inae_core/build/Desktop-Debug/Debug/lib/)

target_link_libraries(alive PRIVATE mlt core c_wrapper)

set_target_properties(alive PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${MLT_MODULE_OUTPUT_DIRECTORY}")

install(TARGETS alive LIBRARY DESTINATION ${MLT_INSTALL_MODULE_DIR})

install(FILES
  producer_alive.yml
  DESTINATION ${MLT_INSTALL_DATA_DIR}/alive
)
