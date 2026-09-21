cmake_minimum_required(VERSION 3.16)

project(host-computer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ⭐ 加上 SerialPort
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui

        titlebar.cpp
        titlebar.h

        settingspanel.cpp
        settingspanel.h

        backgrounddialog.cpp
        backgrounddialog.h

        cropwidget.cpp
        cropwidget.h

        cropdialog.cpp
        cropdialog.h

        # ⭐ Modbus RTU
        modbus_rtu.h
        modbus_rtu.cpp

        # ⭐ 设备提示卡片
        devicepromptwidget.h
        devicepromptwidget.cpp

        # ⭐ 资源文件（图片）
        resources.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(host-computer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(host-computer SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(host-computer
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# ⭐ 链接 SerialPort
target_link_libraries(host-computer PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.host-computer)
endif()

set_target_properties(host-computer PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS host-computer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(host-computer)
endif()