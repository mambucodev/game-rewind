cmake_minimum_required(VERSION 3.16)

project(game-rewind VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(BUILD_TESTS "Build unit tests" OFF)

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql Concurrent)
find_package(yaml-cpp REQUIRED)

# Library sources (everything except main.cpp) -- shared between app and tests
set(LIB_SOURCES
    # Core
    src/core/gameinfo.cpp
    src/core/database.cpp
    src/core/savemanager.cpp
    src/core/profiledetector.cpp
    # Steam
    src/steam/steamutils.cpp
    src/steam/manifestmanager.cpp
    src/steam/gamedetector.cpp
    # UI
    src/ui/mainwindow.cpp
    src/ui/gamecarddelegate.cpp
    src/ui/backupitemdelegate.cpp
    src/ui/backupdialog.cpp
    src/ui/addgamedialog.cpp
    src/ui/gameconfigdialog.cpp
    src/ui/gameicon.cpp
    src/ui/onboardingdialog.cpp
    src/ui/profiledialog.cpp
    src/ui/settingsdialog.cpp
    src/ui/bulkbackupdialog.cpp
)

set(LIB_HEADERS
    # Core
    src/core/gameinfo.h
    src/core/database.h
    src/core/savemanager.h
    src/core/profiledetector.h
    # Steam
    src/steam/steamutils.h
    src/steam/manifestmanager.h
    src/steam/gamedetector.h
    # UI
    src/ui/mainwindow.h
    src/ui/gamecarddelegate.h
    src/ui/backupitemdelegate.h
    src/ui/backupdialog.h
    src/ui/addgamedialog.h
    src/ui/gameconfigdialog.h
    src/ui/gameicon.h
    src/ui/onboardingdialog.h
    src/ui/profiledialog.h
    src/ui/settingsdialog.h
    src/ui/bulkbackupdialog.h
)

set(UI_FILES
    src/ui/mainwindow.ui
)

# Static library for linking into both the app and tests
add_library(game-rewind-lib STATIC
    ${LIB_SOURCES}
    ${LIB_HEADERS}
    ${UI_FILES}
)

target_include_directories(game-rewind-lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/steam
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(game-rewind-lib PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    yaml-cpp::yaml-cpp
)

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE game-rewind-lib)

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
