cmake_minimum_required(VERSION 3.16)
project(al_muslim_cpp VERSION 0.1.0 LANGUAGES CXX)

# Options
option(ALMUSLIM_USE_STATIC "Prefer static runtime where possible" OFF)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform specifics
if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  if(ALMUSLIM_USE_STATIC)
    # Static runtime on MSVC
    foreach(flag_var CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG)
      if(${flag_var} MATCHES "/MD")
        string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif()
    endforeach()
  endif()
endif()

add_executable(al-muslim
  src/main.cpp
  src/platform.cpp
  src/ui.cpp
  src/hijri.cpp
)

# Install rules
install(TARGETS al-muslim RUNTIME DESTINATION bin)

# Default install prefix hint
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(WIN32)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install path" FORCE)
  endif()
endif()

# Copy data directory next to the executable for runtime access
set(ALMUSLIM_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
if (EXISTS "${ALMUSLIM_DATA_DIR}")
  add_custom_command(TARGET al-muslim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ALMUSLIM_DATA_DIR}"
            "$<TARGET_FILE_DIR:al-muslim>/data"
    COMMENT "Copying data directory to output")
endif()
