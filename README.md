# GEDI Process Desktop

星载激光雷达 **GEDI L1B / L2A** 光斑浏览与波形处理桌面工具。

本仓库同时提供两套实现：

| 目录 | 技术栈 | 说明 |
|------|--------|------|
| [`python/`](python/) | Python 3.10+ / PySide6 | 完整功能，易扩展（注册中心） |
| [`cpp/`](cpp/) | C++17 / Qt 6 | 高性能重写，在线底图 + 本地瓦片金字塔 |

两套界面工作流一致：左侧图层树 → 中间光斑地图 → 右侧波形（高度 ↑ / 强度 →）。

---

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

---

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

---

## 环境变量（可选）

| 变量 | 作用 |
|------|------|
| `GEDI_PYTHON` | Python 解释器路径（GeoTIFF 预览 / 瓦片金字塔脚本） |
| `GEDI_TOOLS_DIR` | `tools/*.py` 目录（默认相对可执行文件查找） |
| `GEDI_SAMPLE_DIR` | 示例数据目录（文件对话框默认路径） |
| `GEDI_HDF5_DLL` / `GEDI_HDF5_BIN` | HDF5 库 / `h5dump` 位置 |

GeoTIFF 预览与本地瓦片依赖 Python 包：`rasterio`、`numpy`、`Pillow`。

---

## 仓库结构

```
gedi-process-desktop/
├── README.md          ← 本文件
├── LICENSE
├── python/            ← PySide6 实现
│   ├── main.py
│   ├── app/
│   ├── tools 可选
│   └── tests_*.py
└── cpp/               ← Qt6 C++ 实现
    ├── CMakeLists.txt
    ├── src/
    │   ├── core/      i18n、变换注册
    │   ├── data/      H5 懒加载、栅格工具
    │   ├── map/       地图、在线瓦片、本地金字塔
    │   ├── processing/
    │   └── ui/
    └── tools/         tif_to_png.py、build_tile_pyramid.py、build.bat
```

---

## 许可

见 [LICENSE](LICENSE)（MIT）。
