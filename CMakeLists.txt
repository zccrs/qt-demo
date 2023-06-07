cmake_minimum_required(VERSION 3.14)

project(qt-demo LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(qt-demo
  main.cpp
)
target_link_libraries(qt-demo Qt${QT_VERSION_MAJOR}::Core)

target_include_directories(qt-demo
    PRIVATE
    ${Qt${QT_VERSION_MAJOR}Core_PRIVATE_INCLUDE_DIRS}
)

install(TARGETS qt-demo
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
