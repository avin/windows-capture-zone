cmake_minimum_required(VERSION 4.1)
project(windows-capture-zone LANGUAGES CXX RC)

add_executable(windows-capture-zone WIN32 main.cpp app.rc)

target_compile_features(windows-capture-zone PRIVATE cxx_std_23)
target_compile_definitions(windows-capture-zone PRIVATE UNICODE _UNICODE)
target_link_libraries(windows-capture-zone PRIVATE Dwmapi Magnification)

target_sources(windows-capture-zone
    PRIVATE
        FILE_SET CXX_MODULES FILES
            app.types.ixx
            app.layout.ixx
            app.graphics.ixx
            app.settings.ixx
            app.windows.ixx
)
