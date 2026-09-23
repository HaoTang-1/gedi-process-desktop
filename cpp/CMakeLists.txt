cmake_minimum_required(VERSION 3.16)
project(GEDIProcessDesktopCpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6 — set -DQT_ROOT=... or rely on CMAKE_PREFIX_PATH / Qt6_DIR
set(QT_ROOT "" CACHE PATH "Optional Qt 6 install prefix (e.g. .../mingw_64)")
if(QT_ROOT)
    list(APPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Concurrent Network)

# zlib (MinGW ships it) for optional inflate; not required for dyn-HDF5 path
find_package(ZLIB QUIET)

add_executable(GEDIProcessDesktopCpp WIN32
    src/main.cpp
    app.rc
    src/core/I18n.cpp
    src/core/TransformRegistry.cpp
    src/data/H5Dyn.cpp
    src/data/GediLoader.cpp
    src/data/FieldDocs.cpp
    src/data/RasterInfo.cpp
    src/data/GeoTiffLite.cpp
    src/processing/WaveformOps.cpp
    src/processing/Metrics.cpp
    src/processing/Transforms.cpp
    src/processing/Calculator.cpp
    src/map/TileCache.cpp
    src/map/BasemapManager.cpp
    src/map/ImageTilePyramid.cpp
    src/map/MapWidget.cpp
    src/ui/MainWindow.cpp
    src/ui/FileTreePanel.cpp
    src/ui/InfoPanel.cpp
    src/ui/WaveformWidget.cpp
    src/ui/PropertyPanel.cpp
    src/ui/Hdf5ViewerDialog.cpp
    src/ui/RasterPropsDialog.cpp
    src/ui/WaveformCalculatorDialog.cpp
    src/ui/BatchDialog.cpp
    src/ui/ParamDialog.cpp
)

target_include_directories(GEDIProcessDesktopCpp PRIVATE src)
target_link_libraries(GEDIProcessDesktopCpp PRIVATE
    Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Network
)

if(ZLIB_FOUND)
    target_link_libraries(GEDIProcessDesktopCpp PRIVATE ZLIB::ZLIB)
    target_compile_definitions(GEDIProcessDesktopCpp PRIVATE HAS_ZLIB=1)
endif()

# Windows: shcore for DPI
if(WIN32)
    target_link_libraries(GEDIProcessDesktopCpp PRIVATE shcore user32 gdi32 shell32 ole32)
endif()

install(TARGETS GEDIProcessDesktopCpp RUNTIME DESTINATION bin)
