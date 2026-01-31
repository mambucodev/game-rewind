cmake_minimum_required(VERSION 3.16)

project(game-rewind VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql Concurrent)
find_package(yaml-cpp REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
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

set(HEADERS
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

# Create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

# Include directories so headers can find each other across subdirectories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/steam
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    yaml-cpp::yaml-cpp
)

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
