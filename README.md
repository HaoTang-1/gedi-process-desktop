# GEDI Process Desktop

基于 **PySide6** 的星载激光雷达 **GEDI L1B / L2A** 光斑浏览与波形处理桌面工具。

界面采用模块化、菜单驱动的工作流：文件树复选框控制显隐、光斑地图、波形面板，变换与指标可通过注册中心扩展。

---

## 功能

- 导入 GEDI **L1B** / **L2A** HDF5，显示光斑位置（底图暂未接入，后续可挂载地图服务）
- 点击光斑查看形状相关数据：
  - **L1B**：回波波形（`rxwaveform`）
  - **L2A**：相对高度剖面（`rh`）
- 波形面板（位于地图右侧）：**高度 ↑ / 强度 →**；可拖动平移、滚轮缩放
- 地图：拖动平移、滚轮缩放（自动限制合理范围）；文件复选框控制光斑显隐；右键删除数据
- 变换菜单（非常驻）：FFT、小波去噪 DWT、CWT 时频图、STFT、Butterworth 低/高通、Savitzky–Golay、滑动平均、归一化、基线去除
- 指标计算 + **多文件批处理**（导出 CSV）
- 界面 **中文 / English**（默认中文）：*帮助 → 语言*
- **变换 / 计算插件注册中心**：新算法注册后自动出现在菜单中

## 环境要求

- Python 3.10+
- PySide6、h5py、numpy、scipy、matplotlib、pandas、PyWavelets

```bash
pip install -r requirements.txt
```

或使用 conda：

```bash
conda create -n gedi_desktop python=3.11 -y
conda activate gedi_desktop
pip install -r requirements.txt
```

## 安装与运行

```bash
cd gedi_desktop
python main.py
```

启动时可直接传入 HDF5 路径：

```bash
python main.py /path/to/GEDI01_B_xxx.h5 /path/to/GEDI02_A_xxx.h5
```

Windows 可使用 `run.bat`（请按本机环境修改其中的 Python 路径）。

## 使用说明

1. **文件 → 打开数据文件**（或 **打开文件夹**）
2. 左侧文件树勾选/取消，控制地图上光斑显隐
3. 滚轮缩放地图，拖动平移，点击光斑
4. 右侧查看波形 / RH（高度 vs 强度）
5. 菜单 **变换**：滤波 / FFT / 小波等（含参数对话框）
6. 菜单 **计算**：已注册的指标模块
7. **批处理**：多文件指标导出 CSV
8. **帮助 → 语言**：中文 / English

## 扩展开发

在 `app/processing/transforms.py` 中添加变换：

```python
from app.core.registry import ParamSpec, register_transform
import numpy as np

@register_transform(
    id="my_filter",
    name="我的滤波",
    category="滤波",
    result_kind="signal",  # 或 spectrum
    params=[ParamSpec("k", "增益 k", "float", 1.0, 0.0, 10.0, 0.1)],
)
def tr_my_filter(y, k=1.0, **_):
    return {"y": np.asarray(y, float) * k, "label": f"我的滤波 k={k}", "kind": "signal"}
```

在 `app/processing/builtin_metrics.py` 中用 `@register_metric` 添加计算模块。  
界面文案：`app/i18n.py`。  
数据产品与字段：`app/data/gedi_loader.py`。

注册新模块后重启软件即可在菜单中看到。

## 数据说明

| 产品 | 坐标字段 | 剖面 / 波形 |
|------|----------|-------------|
| L1B | `geolocation/latitude_bin0`、`longitude_bin0` | `rxwaveform`（按需懒加载） |
| L2A | `lat_lowestmode`、`lon_lowestmode` | `rh`（N × 101） |

- 大文件仅缓存元数据；选中光斑时再读取波形 / RH
- 地图点位默认降采样显示
- **请勿将 NASA 大体积 HDF5 提交进公开仓库**；README 中说明获取方式即可

GEDI 产品可从 [NASA LP DAAC](https://lpdaac.usgs.gov/) 获取（可能需要注册）。

## 项目结构

```
gedi_desktop/
  main.py
  requirements.txt
  LICENSE
  app/
    i18n.py
    config.py
    core/registry.py      # 变换与计算插件注册
    core/session.py
    data/gedi_loader.py   # L1B/L2A HDF5
    processing/
      transforms.py
      builtin_metrics.py
      metrics.py
      waveform_ops.py
      batch.py
    ui/
      main_window.py
      file_tree.py
      info_panel.py
      footprint_map.py
      waveform_view.py
      property_panel.py
      pan_zoom.py
      batch_dialog.py
```

## 许可证

MIT，详见 [LICENSE](LICENSE)。

第三方库遵循各自许可证（PySide6/Qt 为 LGPL；科学计算栈多为 BSD/MIT 类）。

## 引用

使用 GEDI 数据产品时，请引用 GEDI 任务及相关 LP DAAC 产品 DOI（如 `GEDI01_B`、`GEDI02_A`）。

## 致谢

- NASA GEDI 任务与 LP DAAC 的 L1B/L2A 产品  
- 开源组件：PySide6、h5py、NumPy、SciPy、Matplotlib、pandas、PyWavelets  

---

# GEDI Process Desktop (English)

Desktop tool for browsing **GEDI L1B / L2A** footprints and processing waveforms, built with **PySide6**.

Modular, menu-driven UI (CloudCompare-style workflow): file tree with visibility checkboxes, footprint map, waveform panel, plugin registry for transforms/metrics.

## Features

- Load GEDI **L1B** and **L2A** HDF5 granules; show footprint locations (no basemap yet — map services can be added later)
- Click a footprint to inspect shape data:
  - **L1B**: received waveform (`rxwaveform`)
  - **L2A**: relative height profile (`rh`)
- Waveform panel (right of map): **height ↑ / intensity →**; drag to pan, wheel to zoom
- Map: drag to pan, wheel zoom with adaptive bounds; checkbox per file to show/hide footprints; right-click to remove a file
- Transform menu (not always-on): FFT, DWT denoise, CWT scalogram, STFT, Butterworth low/high-pass, Savitzky–Golay, moving average, normalize, baseline removal
- Metrics + **batch processing** over multiple files (CSV export)
- **中文 / English** UI (default: Chinese) via *Help → Language*
- Extensible **transform / metric registry** — new algorithms appear in menus automatically

## Requirements

- Python 3.10+
- PySide6, h5py, numpy, scipy, matplotlib, pandas, PyWavelets

```bash
pip install -r requirements.txt
```

Or with conda:

```bash
conda create -n gedi_desktop python=3.11 -y
conda activate gedi_desktop
pip install -r requirements.txt
```

## Install & Run

```bash
cd gedi_desktop
python main.py
```

Optional: pass HDF5 paths on the command line to load at startup:

```bash
python main.py /path/to/GEDI01_B_xxx.h5 /path/to/GEDI02_A_xxx.h5
```

On Windows you can use `run.bat` after adjusting the Python path inside it to your environment.

## Usage

1. **File → Open Data Files** (or **Open Folder**)
2. Check/uncheck files in the left tree to control map visibility
3. Scroll to zoom the map, drag to pan, click a footprint
4. Inspect the waveform/RH profile on the right (height vs intensity)
5. **Transform** menu: filters / FFT / wavelets (parameters dialog)
6. **Compute** menu: registered metric modules
7. **Batch**: multi-file metrics → CSV
8. **Help → Language**: 中文 / English

## Extending

Add a transform in `app/processing/transforms.py`:

```python
from app.core.registry import ParamSpec, register_transform
import numpy as np

@register_transform(
    id="my_filter",
    name="My Filter",
    category="Filters",
    result_kind="signal",  # or "spectrum"
    params=[ParamSpec("k", "Gain", "float", 1.0, 0.0, 10.0, 0.1)],
)
def tr_my_filter(y, k=1.0, **_):
    return {"y": np.asarray(y, float) * k, "label": f"My Filter k={k}", "kind": "signal"}
```

Add a metric in `app/processing/builtin_metrics.py` with `@register_metric`.  
UI strings: `app/i18n.py`.  
Data products/fields: `app/data/gedi_loader.py`.

Restart the app after registering new modules.

## Data

| Product | Location | Profile / waveform |
|---------|----------|--------------------|
| L1B | `geolocation/latitude_bin0`, `longitude_bin0` | `rxwaveform` (lazy-loaded) |
| L2A | `lat_lowestmode`, `lon_lowestmode` | `rh` (N × 101) |

- Large granules: only metadata is cached; waveforms/RH are read when a shot is selected.
- Map points are downsampled for display.
- **Do not commit large NASA HDF5 files to the repository.** Provide download instructions instead.

Obtain GEDI products from [NASA LP DAAC](https://lpdaac.usgs.gov/) (registration may be required).

## Project layout

```
gedi_desktop/
  main.py
  requirements.txt
  LICENSE
  app/
    i18n.py
    config.py
    core/registry.py      # transform & metric plugins
    core/session.py
    data/gedi_loader.py   # L1B/L2A HDF5
    processing/
      transforms.py
      builtin_metrics.py
      metrics.py
      waveform_ops.py
      batch.py
    ui/
      main_window.py
      file_tree.py
      info_panel.py
      footprint_map.py
      waveform_view.py
      property_panel.py
      pan_zoom.py
      batch_dialog.py
```

## License

MIT — see [LICENSE](LICENSE).

Third-party libraries remain under their own licenses (PySide6/Qt is LGPL; scientific stack is typically BSD/MIT-style).

## Citation

If you use GEDI data products, please cite the mission and the relevant LP DAAC product DOIs (GEDI L1B / L2A). Example product series: `GEDI01_B`, `GEDI02_A`.

## Acknowledgments

- NASA GEDI mission and LP DAAC for L1B/L2A products  
- Open-source stack: PySide6, h5py, NumPy, SciPy, Matplotlib, pandas, PyWavelets  
