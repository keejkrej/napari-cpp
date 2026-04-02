cmake_minimum_required(VERSION 3.24)

project(napari-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Test)
find_package(TIFF REQUIRED)

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif()

set(NAPARI_CPP_SOURCES
    src/core/camera2d.cpp
    src/core/dims_model.cpp
    src/core/image_layer.cpp
    src/core/layer.cpp
    src/core/nd_image.cpp
    src/core/slice_extractor.cpp
    src/core/viewer_model.cpp
    src/io/image_reader.cpp
    src/render/canvas_widget.cpp
    src/ui/dims_widget.cpp
    src/ui/layer_list_model.cpp
    src/ui/main_window.cpp
)

set(NAPARI_CPP_HEADERS
    src/core/camera2d.h
    src/core/dims_model.h
    src/core/image_layer.h
    src/core/layer.h
    src/core/nd_image.h
    src/core/slice_extractor.h
    src/core/viewer_model.h
    src/io/image_reader.h
    src/render/canvas_widget.h
    src/ui/dims_widget.h
    src/ui/layer_list_model.h
    src/ui/main_window.h
)

add_library(napari_cpp_lib ${NAPARI_CPP_SOURCES} ${NAPARI_CPP_HEADERS})
target_include_directories(napari_cpp_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(
    napari_cpp_lib
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        TIFF::TIFF
)

add_executable(napari-cpp src/main.cpp)
target_link_libraries(napari-cpp PRIVATE napari_cpp_lib)

enable_testing()

add_executable(
    napari_cpp_tests
    tests/test_main.cpp
    tests/test_suite.cpp
)
target_link_libraries(napari_cpp_tests PRIVATE napari_cpp_lib Qt6::Test)
add_test(NAME napari_cpp_tests COMMAND napari_cpp_tests)
set_tests_properties(napari_cpp_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
