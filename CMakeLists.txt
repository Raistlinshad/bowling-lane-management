vcmake_minimum_required(VERSION 3.16)
project(Canadian5PinBowling VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required Qt5 components
find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Multimedia
    MultimediaWidgets
    Gui
)

# Find additional components if available
find_package(Qt5 QUIET COMPONENTS
    OpenGL
    Svg
)

# Set Qt5 automatic MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Include Qt5 directories
include_directories(${Qt5Core_INCLUDE_DIRS})
include_directories(${Qt5Widgets_INCLUDE_DIRS})
include_directories(${Qt5Network_INCLUDE_DIRS})
include_directories(${Qt5Multimedia_INCLUDE_DIRS})
include_directories(${Qt5MultimediaWidgets_INCLUDE_DIRS})

# Add Qt5 definitions
add_definitions(${Qt5Core_DEFINITIONS})
add_definitions(${Qt5Widgets_DEFINITIONS})
add_definitions(${Qt5Network_DEFINITIONS})
add_definitions(${Qt5Multimedia_DEFINITIONS})
add_definitions(${Qt5MultimediaWidgets_DEFINITIONS})

# Source files
set(SOURCES
    main.cpp
    LaneClient.cpp
    QuickGame.cpp
    BowlingWidgets.cpp
    MediaManager.cpp
)

# Header files
set(HEADERS
    LaneClient.h
    QuickGame.h
    BowlingWidgets.h
    MediaManager.h
)

# Resource files (if any)
set(RESOURCES
    # resources.qrc  # Uncomment if you have resources
)

# Create the executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

# Link Qt5 libraries
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    Qt5::Gui
)

# Link optional components if found
if(Qt5OpenGL_FOUND)
    target_link_libraries(${PROJECT_NAME} Qt5::OpenGL)
endif()

if(Qt5Svg_FOUND)
    target_link_libraries(${PROJECT_NAME} Qt5::Svg)
endif()

# Platform-specific settings
if(WIN32)
    # Windows specific settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    # Copy Qt5 DLLs to output directory for Windows
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt5::Core>
            $<TARGET_FILE:Qt5::Widgets>
            $<TARGET_FILE:Qt5::Network>
            $<TARGET_FILE:Qt5::Multimedia>
            $<TARGET_FILE:Qt5::MultimediaWidgets>
            $<TARGET_FILE:Qt5::Gui>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
    
elseif(UNIX AND NOT APPLE)
    # Linux specific settings
    
    # Find additional Linux dependencies
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GSTREAMER gstreamer-1.0)
        if(GSTREAMER_FOUND)
            target_include_directories(${PROJECT_NAME} PRIVATE ${GSTREAMER_INCLUDE_DIRS})
            target_link_libraries(${PROJECT_NAME} ${GSTREAMER_LIBRARIES})
        endif()
    endif()
    
    # Set RPATH for proper library loading
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
    
elseif(APPLE)
    # macOS specific settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.bowlingcenter.canadian5pin"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
endif()

# Install settings
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)

# Copy Python files to build directory
configure_file(${CMAKE_SOURCE_DIR}/machine_interface.py ${CMAKE_BINARY_DIR}/machine_interface.py COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/MachineFunctions.py ${CMAKE_BINARY_DIR}/MachineFunctions.py COPYONLY)
configure_file(${CMAKE_SOURCE_DIR}/active_ball_detector.py ${CMAKE_BINARY_DIR}/active_ball_detector.py COPYONLY)

# Copy settings file if it exists
if(EXISTS ${CMAKE_SOURCE_DIR}/settings.ini)
    configure_file(${CMAKE_SOURCE_DIR}/settings.ini ${CMAKE_BINARY_DIR}/settings.ini COPYONLY)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/settings.json)
    configure_file(${CMAKE_SOURCE_DIR}/settings.json ${CMAKE_BINARY_DIR}/settings.json COPYONLY)
endif()

# Create media directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/media/videos)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/media/images)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/media/effects)

# Copy any existing media files
if(EXISTS ${CMAKE_SOURCE_DIR}/media)
    file(GLOB_RECURSE MEDIA_FILES "${CMAKE_SOURCE_DIR}/media/*")
    foreach(MEDIA_FILE ${MEDIA_FILES})
        file(RELATIVE_PATH REL_PATH ${CMAKE_SOURCE_DIR}/media ${MEDIA_FILE})
        configure_file(${MEDIA_FILE} ${CMAKE_BINARY_DIR}/media/${REL_PATH} COPYONLY)
    endforeach()
endif()

# Copy pin images if they exist
if(EXISTS ${CMAKE_SOURCE_DIR}/5pin_up.png)
    configure_file(${CMAKE_SOURCE_DIR}/5pin_up.png ${CMAKE_BINARY_DIR}/5pin_up.png COPYONLY)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/5pin_down.png)
    configure_file(${CMAKE_SOURCE_DIR}/5pin_down.png ${CMAKE_BINARY_DIR}/5pin_down.png COPYONLY)
endif()

# Development targets
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Add debug information
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG_BUILD)
    
    # Enable all warnings for debug builds
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE /W4)
    endif()
endif()

# Custom target for cleaning Python cache
add_custom_target(clean-python
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/__pycache__
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/*.pyc
    COMMENT "Cleaning Python cache files"
)

# Custom target for checking Python dependencies
add_custom_target(check-python-deps
    COMMAND python3 -c "import RPi.GPIO; import busio; import board; import adafruit_ads1x15.ads1115; print('Python dependencies OK')"
    COMMENT "Checking Python dependencies"
)

# Print configuration summary
message(STATUS "")
message(STATUS "Canadian 5-Pin Bowling Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt5 version: ${Qt5_VERSION}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Qt5 Components:")
message(STATUS "  Core: ${Qt5Core_VERSION}")
message(STATUS "  Widgets: ${Qt5Widgets_VERSION}")
message(STATUS "  Network: ${Qt5Network_VERSION}")
message(STATUS "  Multimedia: ${Qt5Multimedia_VERSION}")
message(STATUS "  MultimediaWidgets: ${Qt5MultimediaWidgets_VERSION}")

if(Qt5OpenGL_FOUND)
    message(STATUS "  OpenGL: ${Qt5OpenGL_VERSION}")
endif()

if(Qt5Svg_FOUND)
    message(STATUS "  Svg: ${Qt5Svg_VERSION}")
endif()

message(STATUS "")

# Packaging (CPack)
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Canadian 5-Pin Bowling Lane Client")
set(CPACK_PACKAGE_VENDOR "Bowling Center")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Canadian 5-Pin Bowling")
    set(CPACK_NSIS_PACKAGE_NAME "Canadian5PinBowling")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Bowling Center <admin@bowlingcenter.com>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt5-default, python3, python3-rpi.gpio")
endif()

include(CPack)