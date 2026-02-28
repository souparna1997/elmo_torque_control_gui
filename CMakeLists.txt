cmake_minimum_required(VERSION 3.19)
project(elmo_torque_control_gui LANGUAGES CXX)
set(CMAKE_PREFIX_PATH "/home/maestro/Qt/6.10.2/gcc_64")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools Charts)

qt_standard_project_setup()

qt_add_executable(elmo_torque_control_gui
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_include_directories(elmo_torque_control_gui PRIVATE "/home/maestro/Projects/ecat_elmo_control/shared_include")

qt_add_translations(
    TARGETS elmo_torque_control_gui
    TS_FILES elmo_torque_control_gui_en_US.ts
)

target_link_libraries(elmo_torque_control_gui
    PRIVATE
        Qt::Core
        Qt::Widgets
)

include(GNUInstallDirs)

install(TARGETS elmo_torque_control_gui
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET elmo_torque_control_gui
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

target_link_libraries(elmo_torque_control_gui
    PRIVATE
        Qt6::Widgets
        Qt6::Charts
)
