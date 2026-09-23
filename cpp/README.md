<p align="center">
  <img src="assets/app.png" alt="GEDI Process Desktop" width="96" />
</p>

# GEDI Process Desktop — C++ / Qt 6

Qt 6 C++ 重写版：异步在线底图、本地瓦片金字塔、**原生 GeoTIFF 解码**（无需 Python）。

显示坐标系 **EPSG:4326**（plate carrée，与 QGIS 一致）。

**中文** / **English** 见下。

---

## 中文

### 依赖

| 组件 | 说明 |
|------|------|
| Qt 6 | Widgets、Core、Gui、Concurrent、Network |
| CMake ≥ 3.16 | Ninja 或 MinGW |
| C++17 | MinGW 或 MSVC |
| hdf5.dll（可选） | 动态加载 HDF5 |
| Python 3（可选） | 仅冷门 GeoTIFF 压缩时回退 |

### 构建

```bat
cmake -S . -B build -DQT_ROOT=C:\Qt\6.10.1\mingw_64
cmake --build build -j
```

或：

```bat
tools\build.bat
```

产物：`build\GEDIProcessDesktopCpp.exe`。

### 安装包

```bat
cd ..\packaging
pack_cpp.bat
```

默认路径：`C:\Program Files (x86)\GEDIProcessDesktopCpp`。

### 环境变量

| 变量 | 作用 |
|------|------|
| `GEDI_PYTHON` | Python 解释器 |
| `GEDI_TOOLS_DIR` | `tools` 目录 |
| `GEDI_SAMPLE_DIR` | 示例数据目录 |
| `GEDI_HDF5_DLL` / `GEDI_HDF5_BIN` | HDF5 / `h5dump` |

### GeoTIFF 与金字塔

- 原生支持：未压缩 / PackBits / LZW / Deflate，8/16 位，读 `ModelPixelScale` + `ModelTiepoint`
- 金字塔：`<影像目录>\.gedi_tiles\`（`meta.json` 存在则不重建）
- 命令行：`--tif-preview` / `--tif-pyramid`

### 波形处理

归一化 / 基线 / 去趋势；SG、滑动平均、中值、高斯；Butterworth 低/高/带通/带阻；
导数、积分、包络、滑动 RMS、阈值、反转、重采样；FFT 幅度/相位/PSD、**IFFT**、频谱去噪。

### Logo

替换 `assets/app.ico` + `assets/app.png` 后重新编译打包即可。

### 联系方式

- Email：[tangh@std.uestc.edu.cn](mailto:tangh@std.uestc.edu.cn)

---

## English

Native Qt 6 rewrite: async basemap, tile pyramid, **native GeoTIFF** (no Python required). Display CRS **EPSG:4326** (plate carrée).

Build with `tools\build.bat` or CMake. Package with `../packaging/pack_cpp.bat` (default install: `C:\Program Files (x86)\GEDIProcessDesktopCpp`).

Signal processing: filters (SG/median/Gaussian/Butterworth LP/HP/BP/BS), time-domain ops (deriv/integral/envelope/RMS/…), FFT mag/phase/PSD, IFFT.

Replace `assets/app.ico` + `app.png` to change branding.

**Contact:** [tangh@std.uestc.edu.cn](mailto:tangh@std.uestc.edu.cn)
