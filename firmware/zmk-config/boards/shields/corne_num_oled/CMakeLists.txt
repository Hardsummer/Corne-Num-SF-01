if(CONFIG_ZMK_DISPLAY AND CONFIG_CORNE_NUM_OLED_VERTICAL)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_sources(vertical_status.c)
endif()
