cmake_minimum_required(VERSION 3.5)

project(ChunGeEr VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(COMMON_LIBRARIES
    windowsapp
    d3d11
    dxgi
    dwmapi
    user32
    gdi32
)
set(COMMON_DEFINITIONS
    _WINRT_MESSAGES
    WINRT_LEAN_AND_MEAN
)
set(COMMON_OPTIONS
    "/await"
    "/EHsc"
)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort)
add_library(ScreenCapture STATIC
    ${CMAKE_SOURCE_DIR}/../ScreenCapture/pch.cpp
    ${CMAKE_SOURCE_DIR}/../ScreenCapture/pch.h
    ${CMAKE_SOURCE_DIR}/../ScreenCapture/ScreenCapture.h
    ${CMAKE_SOURCE_DIR}/../ScreenCapture/ScreenCapture.cpp
    ${CMAKE_SOURCE_DIR}/../ScreenCapture/screencapturemanager.h
    ${CMAKE_SOURCE_DIR}/../ScreenCapture/screencapturemanager.cpp
)

target_link_libraries(ScreenCapture PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)



add_library(YoloOnnx STATIC
    ${CMAKE_SOURCE_DIR}/../YoloOnnx/inference.cpp
    ${CMAKE_SOURCE_DIR}/../YoloOnnx/inference.h
)

include_directories(YoloOnnx ${CMAKE_SOURCE_DIR}/../include/onnx)
target_link_libraries(YoloOnnx ${CMAKE_SOURCE_DIR}/../Libs/onnxruntime.lib)

add_library(XuLog SHARED
    ${CMAKE_SOURCE_DIR}/../Log/XuLog.cpp
    ${CMAKE_SOURCE_DIR}/../Log/XuLog.h
)
target_link_libraries(XuLog PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    )

include_directories(${CMAKE_SOURCE_DIR}/../include)
include_directories(${CMAKE_SOURCE_DIR}/../ScreenCapture/)
include_directories(${CMAKE_SOURCE_DIR}/../YoloOnnx/)
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

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    qt_add_executable(ChunGeEr
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        service/baseservice.h service/baseservice.cpp
        service/dungeon/dungeonservice.h service/dungeon/dungeonservice.cpp
        Detector/detectormanager.h Detector/detectormanager.cpp
        service/CatchMonsters/catchmonstersservice.h service/CatchMonsters/catchmonstersservice.cpp
        Ocr/ocrmnager.h Ocr/ocrmnager.cpp
        Ocr/paddleocr.h Ocr/paddleocr.cpp
        Ocr/tesseractocr.h Ocr/tesseractocr.cpp
        Control/mouselabel.h Control/mouselabel.cpp
        Ui/screensharewidget.h Ui/screensharewidget.cpp Ui/screensharewidget.ui
        Ui/gameitemcapturewidget.h Ui/gameitemcapturewidget.cpp
        gameutils.h gameutils.cpp
        gameslot.h gameslot.cpp
        slotscheduler.h slotscheduler.cpp
        autologin.h autologin.cpp
        accountdialog.h accountdialog.cpp
        Commons/safepacket.cpp Commons/safepacket.h
        KeyboardListener/keyboardlistener.cpp KeyboardListener/keyboardlistener.h
        StorageVideo/storagevidoemanager.cpp StorageVideo/StorageVidoeManager.h
        WsManager/wsmanager.cpp WsManager/wsmanager.h
        ScreenShare/screenshare.cpp ScreenShare/screenshare.h
        LeoControl/mousekeyboardmanager.cpp LeoControl/mousekeyboardmanager.h
        Encode/encodingmanager.cpp Encode/encodingmanager.h
        signalslotconnector.h signalslotconnector.cpp
        service/Record/record.h service/Record/record.cpp
        service/MainQuest/mainquestservice.h service/MainQuest/mainquestservice.cpp
        LeoControl/serialportmanager.h LeoControl/serialportmanager.cpp
    )
target_include_directories(ChunGeEr PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Control
)
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET ChunGeEr APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(ChunGeEr SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(ChunGeEr
            ${PROJECT_SOURCES}
        )
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug 配置
    target_link_libraries(ChunGeEr PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::SerialPort
        ScreenCapture
        opencv_core490d
        opencv_calib3d490d
        opencv_imgcodecs490d
        opencv_imgproc490d
        opencv_highgui490d
        hv
        windowsapp
        avcodec
        avdevice
        avfilter
        avutil
        swresample
        swscale
        avformat
        onnxruntime
        YoloOnnx
        opencv_dnn490d
        XuLog)
target_link_libraries(ChunGeEr PRIVATE Qt6::Core)
else()
    # Release 配置
    link_directories(${CMAKE_SOURCE_DIR}/../Libs/Release)
    target_link_libraries(ChunGeEr PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::SerialPort
        ScreenCapture
        opencv_core490
        opencv_calib3d490
        opencv_imgcodecs490
        opencv_imgproc490
        opencv_highgui490
        hv
        windowsapp
        avcodec
        avdevice
        avfilter
        avutil
        swresample
        swscale
        avformat
        onnxruntime
        YoloOnnx
        opencv_dnn490
        XuLog)
endif()


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ChunGeEr)
endif()
set_target_properties(ChunGeEr PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# UAC 管理员权限
target_link_options(ChunGeEr PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")

include(GNUInstallDirs)
install(TARGETS ChunGeEr
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ChunGeEr)
endif()

# Post-build: 复制 images 目录到输出目录
add_custom_command(TARGET ChunGeEr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/../images"
        "$<TARGET_FILE_DIR:ChunGeEr>/images"
    COMMENT "Copying images directory to output..."
)
