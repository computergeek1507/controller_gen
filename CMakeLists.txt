cmake_minimum_required(VERSION 3.20.4)

project(controller_gen VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set path to our custom CMAKE scripts
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

#add_compile_definitions(NO_ZLIB)

# Include Qt deployment helper function
include(windeployqt)

include(FetchContent)

FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.x
)

FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
  pugixml
  GIT_REPOSITORY https://github.com/zeux/pugixml.git
  GIT_TAG      v1.14 # Or a specific commit hash for a stable version
)

# Populate the content, making it available for use
FetchContent_MakeAvailable(pugixml)

FetchContent_Declare(
        zstd
        GIT_REPOSITORY https://github.com/facebook/zstd.git
        GIT_TAG        v1.5.2
)

FetchContent_GetProperties(zstd)
if (NOT zstd_POPULATED)
    FetchContent_Populate(zstd)
    add_subdirectory(${zstd_SOURCE_DIR}/build/cmake ${zstd_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

#FetchContent_Declare(ZLIB
#  GIT_REPOSITORY https://github.com/madler/zlib.git
#  GIT_TAG 51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf # v1.3.1 (2024-01-22)
#  OVERRIDE_FIND_PACKAGE
#)

FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1 # Or the desired zlib version
)

FetchContent_MakeAvailable(zlib)

FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json
  GIT_TAG v3.12.0)

FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
  FetchContent_Populate(nlohmann_json)
  add_subdirectory( ${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

set(SKIP_INSTALL_ALL ON)
set(ZLIB_BUILD_EXAMPLES OFF)


#set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

configure_file(src/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/config.h)
configure_file(res/installer/${PROJECT_NAME}.iss.in ${CMAKE_CURRENT_SOURCE_DIR}/res/installer/${PROJECT_NAME}.iss)

file( GLOB_RECURSE BASE_SRC src/*cpp src/*h)
file( GLOB_RECURSE BASE_RES res/*ui res/*qrc)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
qt6_wrap_ui(BASE_SRC res/mainwindow.ui)
else()
qt5_wrap_ui(BASE_SRC res/mainwindow.ui)
endif()


if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${BASE_SRC}
        ${BASE_RES}
        res/${PROJECT_NAME}.qrc
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()

        add_executable(${PROJECT_NAME}
            ${BASE_SRC}
            ${BASE_RES}
            res/${PROJECT_NAME}.qrc
        )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network spdlog::spdlog pugixml::pugixml zlib libzstd_shared nlohmann_json::nlohmann_json)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_sources(${PROJECT_NAME} PRIVATE res/resource.rc)

if(WIN32)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${BASE_SRC})
source_group(res FILES ${BASE_RES})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
 else()   
    if(WIN32)
        windeployqt(${PROJECT_NAME})
    endif()
endif()