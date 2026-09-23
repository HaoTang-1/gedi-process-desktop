# GEDI Process Desktop

星载激光雷达 **GEDI L1B / L2A** 光斑浏览与波形处理桌面工具。

本仓库同时提供两套实现：

| 目录 | 技术栈 | 说明 |
|------|--------|------|
| [`python/`](python/) | Python 3.10+ / PySide6 | 完整功能，易扩展（注册中心） |
| [`cpp/`](cpp/) | C++17 / Qt 6 | 高性能重写，在线底图 + 本地瓦片金字塔 |

两套界面工作流一致：左侧图层树 → 中间光斑地图 → 右侧波形（高度 ↑ / 强度 →）。

## 功能

- 导入 GEDI **L1B / L2A** HDF5，显示光斑位置
- 点击光斑查看 **回波波形**（L1B `rxwaveform`）或 **相对高度剖面**（L2A `rh`）
- 地图：拖动平移、滚轮缩放（光标为锚点）、点选光斑；显示 **EPSG:4326**（plate carrée，与 QGIS 一致）
- **在线底图**（可关）：Esri 世界影像 / 山体阴影、NASA Blue Marble、OpenStreetMap（异步 XYZ 瓦片）
- **本地栅格 / GeoTIFF**：预览图 + 可选 `.gedi_tiles` 瓦片金字塔（已有 `meta.json` 不会重建）
- 变换：FFT、小波、Butterworth、Savitzky–Golay、归一化、基线去除等
- 指标计算 + 多文件批处理（CSV）
- HDF5 查看器、波形计算器、按 Shot 号定位、导出 CSV / 地图 PNG
- 界面 **中文 / English**

## 快速开始

### Python 版

```bash
cd python
pip install -r requirements.txt
python main.py
```

Windows：`python\run.bat`（可用环境变量 `GEDI_PYTHON` 指定解释器）。

详见 [python/README.md](python/README.md)。

### C++ / Qt6 版

依赖：**Qt 6**（Widgets / Network）、**CMake ≥ 3.16**、C++17 编译器（MSVC 或 MinGW）；运行时可选 `hdf5.dll`（动态加载 H5）。

```bash
cd cpp
cmake -S . -B build -DQT_ROOT="C:/Qt/6.x.x/mingw_64"
cmake --build build -j
```

Windows 一键脚本（可改 `tools\build.bat` 或预设 `QT_DIR` 等环境变量）：

```bat
cd cpp
tools\build.bat
```

可执行文件：`cpp\build\GEDIProcessDesktopCpp.exe`。

详见 [cpp/README.md](cpp/README.md)。

## 环境变量（可选）

| 变量 | 作用 |
|------|------|
| `GEDI_PYTHON` | Python 解释器路径（GeoTIFF 预览 / 瓦片金字塔脚本） |
| `GEDI_TOOLS_DIR` | `tools/*.py` 目录（默认相对可执行文件查找） |
| `GEDI_SAMPLE_DIR` | 示例数据目录（文件对话框默认路径） |
| `GEDI_HDF5_DLL` / `GEDI_HDF5_BIN` | HDF5 库 / `h5dump` 位置 |

GeoTIFF 预览与本地瓦片依赖 Python 包：`rasterio`、`numpy`、`Pillow`。

## 仓库结构

```
gedi-process-desktop/
├── README.md
├── LICENSE
├── packaging/         Inno Setup 安装包脚本
├── python/            PySide6 实现
│   ├── main.py
│   ├── app/
│   └── tests_*.py
└── cpp/               Qt6 C++ 实现
    ├── CMakeLists.txt
    ├── src/
    │   ├── core/      i18n、变换注册
    │   ├── data/      H5 懒加载、栅格工具
    │   ├── map/       地图、在线瓦片、本地金字塔
    │   ├── processing/
    │   └── ui/
    └── tools/         tif_to_png.py、build_tile_pyramid.py、build.bat
```

## 安装包（Inno Setup）

```bat
:: 先构建 C++ 版（含 windeployqt）
cd cpp
tools\build.bat

:: 再打包（本机已装 Inno Setup 6/7）
cd ..\packaging
pack_cpp.bat
:: 或指定构建目录：
:: set BUILD_DIR=F:\GEDI_process\gedi_desktop_cpp\build
:: pack_cpp.bat
```

产物：`packaging\installer_output\GEDIProcessDesktopCpp-1.0.0-Setup.exe`。

## 许可

见 [LICENSE](LICENSE)（MIT）。

---

# GEDI Process Desktop (English)

Desktop tool for browsing and processing **GEDI L1B / L2A** footprints and waveforms from spaceborne lidar.

This repository ships two implementations:

| Path | Stack | Notes |
|------|--------|--------|
| [`python/`](python/) | Python 3.10+ / PySide6 | Full features, easy to extend (plugin registry) |
| [`cpp/`](cpp/) | C++17 / Qt 6 | High-performance rewrite: async basemap + local tile pyramid |

Same UI workflow in both: layer tree → footprint map → waveform (Height ↑ / Intensity →).

## Features

- Load GEDI **L1B / L2A** HDF5 and show footprint locations
- Click a footprint for **return waveform** (L1B `rxwaveform`) or **relative height** (L2A `rh`)
- Map: pan, wheel zoom (cursor-anchored), pick footprints; display **EPSG:4326** (plate carrée, like QGIS)
- **Online basemaps** (toggleable): Esri World Imagery / Shaded Relief, NASA Blue Marble, OpenStreetMap (async XYZ tiles)
- **Local raster / GeoTIFF**: preview + optional `.gedi_tiles` pyramid (`meta.json` present → never rebuild)
- Transforms: FFT, wavelets, Butterworth, Savitzky–Golay, normalize, baseline removal, and more
- Metrics + multi-file batch (CSV)
- HDF5 viewer, waveform calculator, go-to shot, export CSV / map PNG
- UI in **Chinese / English**

## Quick start

### Python

```bash
cd python
pip install -r requirements.txt
python main.py
```

Windows: `python\run.bat` (set `GEDI_PYTHON` to pick an interpreter).

Details: [python/README.md](python/README.md)

### C++ / Qt 6

Requirements: **Qt 6** (Widgets / Network), **CMake ≥ 3.16**, C++17 (MSVC or MinGW). Optional `hdf5.dll` at runtime (HDF5 loaded dynamically).

```bash
cd cpp
cmake -S . -B build -DQT_ROOT="C:/Qt/6.x.x/mingw_64"
cmake --build build -j
```

Windows one-shot (edit `tools\build.bat` or preset `QT_DIR` etc.):

```bat
cd cpp
tools\build.bat
```

Binary: `cpp\build\GEDIProcessDesktopCpp.exe`.

Details: [cpp/README.md](cpp/README.md)

## Environment variables (optional)

| Variable | Purpose |
|----------|---------|
| `GEDI_PYTHON` | Python interpreter (GeoTIFF preview / tile pyramid) |
| `GEDI_TOOLS_DIR` | Directory of `tools/*.py` (default: relative to the executable) |
| `GEDI_SAMPLE_DIR` | Sample data folder (file dialog default) |
| `GEDI_HDF5_DLL` / `GEDI_HDF5_BIN` | HDF5 library / `h5dump` location |

GeoTIFF preview and local tiles need Python packages: `rasterio`, `numpy`, `Pillow`.

## Repository layout

```
gedi-process-desktop/
├── README.md
├── LICENSE
├── packaging/         Inno Setup scripts (Windows installer)
├── python/            PySide6 app
│   ├── main.py
│   ├── app/
│   └── tests_*.py
└── cpp/               Qt 6 C++ app
    ├── CMakeLists.txt
    ├── src/
    │   ├── core/      i18n, transform registry
    │   ├── data/      lazy HDF5, raster tools
    │   ├── map/       map, online tiles, local pyramid
    │   ├── processing/
    │   └── ui/
    └── tools/         tif_to_png.py, build_tile_pyramid.py, build.bat
```

## Windows installer (Inno Setup)

```bat
:: Build C++ first (includes windeployqt)
cd cpp
tools\build.bat

:: Then package (Inno Setup 6/7 required)
cd ..\packaging
pack_cpp.bat
:: Or point at another build tree:
:: set BUILD_DIR=F:\GEDI_process\gedi_desktop_cpp\build
:: pack_cpp.bat
```

Output: `packaging\installer_output\GEDIProcessDesktopCpp-1.0.0-Setup.exe`.

## License

See [LICENSE](LICENSE) (MIT).
