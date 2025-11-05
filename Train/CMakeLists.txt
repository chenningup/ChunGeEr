cmake_minimum_required(VERSION 3.5)

project(Train VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core)


include_directories(${CMAKE_SOURCE_DIR}/../Log)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug 配置
    link_directories(${CMAKE_SOURCE_DIR}/../Libs/Debug)
else()
    # Release 配置
    link_directories(${CMAKE_SOURCE_DIR}/../Libs/Release)
endif()
link_directories(${CMAKE_SOURCE_DIR}/../Libs/)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)
include_directories(${CMAKE_SOURCE_DIR}/../include)
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Train
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Train APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Train SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Train
            ${PROJECT_SOURCES}
        )
    endif()
endif()


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug 配置
    target_link_libraries(Train PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        tesseract55d
        lstmevalxu
        org.sw.demo.danbloomberg.leptonica-1.85.0
        tesseractrain
        opencv_core490d
        opencv_calib3d490d
        opencv_imgcodecs490d
        opencv_imgproc490d
        opencv_highgui490d
        XuLog)
else()
    target_link_libraries(Train PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        tesseract55
        lstmevalxu
        org.sw.demo.danbloomberg.leptonica-1.85.0
        tesseractrain
        opencv_core490
        opencv_calib3d490
        opencv_imgcodecs490
        opencv_imgproc490
        opencv_highgui490
        XuLog)
endif()
# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Train)
endif()
set_target_properties(Train PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Train
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Train)
endif()
