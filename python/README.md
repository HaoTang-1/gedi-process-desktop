# GEDI Process Desktop — Python / PySide6

PySide6 桌面端：GEDI L1B/L2A 光斑浏览、波形处理、变换与批处理。

## 环境

- Python 3.10+
- PySide6、h5py、numpy、scipy、matplotlib、pandas、PyWavelets

```bash
pip install -r requirements.txt
```

或：

```bash
conda create -n gedi_desktop python=3.11 -y
conda activate gedi_desktop
pip install -r requirements.txt
```

## 运行

```bash
python main.py
python main.py /path/to/GEDI01_B_xxx.h5 /path/to/GEDI02_A_xxx.h5
```

Windows：`run.bat`（`GEDI_PYTHON` 可指定解释器）。

## 使用

1. **文件 → 打开数据文件 / 打开文件夹**
2. 左侧勾选控制光斑显隐，右键删除图层
3. 地图拖动、滚轮缩放，点击光斑
4. 右侧查看波形 / RH（高度 ↑ / 强度 →）
5. **变换**：滤波 / FFT / 小波等
6. **计算** / **批处理**：指标与 CSV 导出
7. **帮助 → 语言**：中文 / English

## 扩展

在 `app/processing/transforms.py` 注册变换，在 `app/processing/builtin_metrics.py` 注册指标，菜单会自动出现。

## 测试

```bash
python tests_smoke.py
python tests_smoke_ui.py
python tests_smoke_layout.py
python tests_smoke_interact.py
```
