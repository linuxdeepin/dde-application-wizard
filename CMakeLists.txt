# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: CC0-1.0

cmake_minimum_required(VERSION 3.11)

project(dde-application-wizard)

set(CMAKE_CXX_STANDARD 20) # for QCoro. Might switch back to 17 if we can switch to Qt6 and use Qt6's async/await
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON) # ensure adapter class can include launcheri1compat.h

find_package(Qt5 REQUIRED COMPONENTS Core LinguistTools)
find_package(Dtk REQUIRED COMPONENTS Core Gui)
find_package(QCoro5 COMPONENTS Core REQUIRED)
find_package(AppStreamQt REQUIRED)
find_package(PackageKitQt5 REQUIRED)

set(BIN_NAME dde-application-wizard)

qt_add_dbus_adaptor(DBUS_ADAPTER_FILES dbus/org.deepin.dde.daemon.Launcher1.xml dbus/launcher1compat.h Launcher1Compat)

add_executable(${BIN_NAME})

set(SOURCE_FILES
    main.cpp
    pkutils.cpp pkutils.h
    dbus/launcher1compat.cpp dbus/launcher1compat.h
)

target_sources(${BIN_NAME}
PRIVATE
    ${SOURCE_FILES}
    ${DBUS_ADAPTER_FILES}
)

target_link_libraries(${BIN_NAME} PRIVATE
    Dtk::Core
    Dtk::Gui
    QCoro::Core
    PK::packagekitqt5
)
