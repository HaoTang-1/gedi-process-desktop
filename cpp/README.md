# GEDI Process Desktop — C++ / Qt 6

Qt 6 C++ 重写版：异步在线底图、Web 瓦片缓存、本地栅格金字塔，显示为 **EPSG:4326**（plate carrée，与 QGIS 一致）。

## 依赖

| 组件 | 说明 |
|------|------|
| Qt 6 | Widgets、Core、Gui、Concurrent、Network |
| CMake ≥ 3.16 | Ninja 或 MinGW Makefiles |
| C++17 | MinGW 或 MSVC |
| Python 3（可选） | GeoTIFF 预览、瓦片金字塔（`rasterio` / `numpy` / `Pillow`） |
| hdf5.dll（可选） | 动态加载 HDF5；放在 exe 旁或 PATH |

## 构建

```bat
cmake -S . -B build -DQT_ROOT=C:\Qt\6.10.1\mingw_64
cmake --build build -j
```

或 Windows 脚本（可预设 `QT_DIR`、`MINGW_DIR`、`CMAKE_EXE`、`NINJA_EXE`、`HDF5_DLL`）：

```bat
tools\build.bat
```

产物：`build\GEDIProcessDesktopCpp.exe`。脚本会尝试 `windeployqt` 并复制运行时 DLL。

## 环境变量

| 变量 | 作用 |
|------|------|
| `GEDI_PYTHON` | Python 解释器 |
| `GEDI_TOOLS_DIR` | `tools` 目录（默认：exe 旁或上一级 `tools`） |
| `GEDI_SAMPLE_DIR` | 文件对话框默认目录 |
| `GEDI_HDF5_DLL` / `GEDI_HDF5_BIN` | HDF5 库 / `h5dump` |

## 本地栅格金字塔

导入大图时可自动生成旁路目录 `<影像所在目录>\.gedi_tiles\`：

```
.meta.json    # 存在则绝不重建
z/x/y.png     # z0–z6，边缘瓦片不补黑边
```

手动构建：

```bash
python tools/build_tile_pyramid.py <src.tif> <tiles_dir>
python tools/tif_to_png.py <src.tif> <out.png>   # 预览 + bounds.json
```

## 地图说明

- 显示坐标系：**EPSG:4326**（等经纬，全球 2:1 宽幅）
- 在线 XYZ 瓦片（Web Mercator）按纬向条带变换后贴合，与光斑对齐
- 瓦片下载为异步队列，UI 不阻塞
- 底图源：Esri 影像 / 山体阴影、NASA Blue Marble、OSM

## 源码结构

```
src/
  core/         I18n、TransformRegistry
  data/         H5Dyn、GediLoader、RasterInfo、FieldDocs
  map/          MapWidget、TileCache、BasemapManager、ImageTilePyramid
  processing/   Metrics、Transforms、WaveformOps、Calculator
  ui/           MainWindow 及面板
tools/          构建与栅格脚本
```
