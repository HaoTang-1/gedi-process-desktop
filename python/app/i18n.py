"""Lightweight i18n for GEDI Process Desktop. Default: zh-CN."""

from __future__ import annotations

from typing import Dict, List, Optional

LANG = "zh"  # "zh" | "en"

_STRINGS: Dict[str, Dict[str, str]] = {
    "zh": {
        "app_title": "GEDI 处理桌面",
        "menu_file": "文件(&F)",
        "menu_view": "视图(&V)",
        "menu_transform": "变换(&T)",
        "menu_metric": "计算(&C)",
        "menu_batch": "批处理(&B)",
        "menu_help": "帮助(&H)",
        "menu_language": "语言 / Language",
        "lang_zh": "中文（默认）",
        "lang_en": "English",
        "act_open": "打开数据文件(&O)…",
        "act_open_dir": "打开文件夹(&D)…",
        "act_sample": "加载示例数据(&S)",
        "act_clear": "清空已加载文件",
        "act_quit": "退出(&Q)",
        "act_reset_zoom": "重置光斑视图",
        "act_interaction": "交互：拖动平移 · 滚轮缩放 · 右键删除",
        "act_show_wave": "显示/隐藏波形面板",
        "act_show_right": "显示/隐藏属性面板",
        "act_batch": "批处理多文件…",
        "act_about": "关于 / 扩展方式",
        "act_recompute": "重新计算",
        "act_all": "全选",
        "act_none": "全不选",
        "act_apply_default": "综合指标（当前光斑）",
        "tb_map": "光斑位置（无底图）",
        "tb_map_sel": "已选光斑",
        "panel_db": "数据库 / 文件",
        "panel_info": "信息 / 计算",
        "panel_display": "显示属性",
        "panel_wave": "波形演示",
        "hint_check": "勾选 = 在地图显示光斑；取消 = 隐藏",
        "hint_right_delete": "右键文件可删除数据",
        "ctx_delete": "删除数据",
        "ctx_check_all": "全部显示",
        "ctx_uncheck_all": "全部隐藏",
        "hint_wave": "点击地图光斑后，在右侧查看波形（高度↑ / 强度→）\n变换请从菜单「变换」调用",
        "hint_info": "导入数据后显示文件信息\n点击光斑后查看波形计算",
        "hint_map_empty": "暂无光斑\n请导入数据并在左侧勾选文件",
        "hint_no_wave": "请选择光斑",
        "tab_file_info": "文件信息",
        "tab_metrics": "波形计算",
        "col_name": "名称",
        "col_type": "类型",
        "col_shots": "光斑数",
        "col_metric": "指标",
        "col_value": "值",
        "no_file": "未选择文件",
        "no_file_info": "未选择文件\n\n在左侧树中选择文件查看详情",
        "product": "产品类型",
        "n_shots": "光斑总数",
        "path": "路径",
        "beams": "波束",
        "color_by": "光斑着色",
        "scalar_field": "标量字段",
        "display": "显示",
        "point_size": "点大小",
        "show_circle": "显示 ~25 m 光斑圆",
        "beam_filter": "波束筛选",
        "all_beams": "全部波束",
        "modules": "扩展模块",
        "modules_hint": "变换/计算模块由注册中心动态加载\n菜单「变换」「计算」中调用",
        "scalar_beam": "波束 (分类色)",
        "scalar_file": "文件 (分类色)",
        "scalar_sens": "Sensitivity",
        "scalar_quality": "质量标志",
        "scalar_dh": "高程差 (冠层代理)",
        "scalar_elev": "最高高程",
        "axis_lon": "经度 (°E)",
        "axis_lat": "纬度 (°N)",
        "axis_height": "高度 (m)",
        "axis_intensity": "强度 (DN)",
        "axis_height_rel": "相对高度 (m)",
        "axis_percentile": "能量百分位 (%)",
        "axis_sample": "波形采样点",
        "wave_original": "原始",
        "wave_current": "当前",
        "empty_transforms": "（无已注册变换）",
        "metric_module": "模块: 综合指标",
        "about_title": "关于",
        "batch_title": "批处理 — 多文件波形/光斑指标",
        "status_ready": "就绪",
        "msg_pick_footprint": "请先在地图上点击一个光斑。",
        "msg_pick_file": "请先导入 GEDI 数据文件。",
        "msg_no_series": "当前光斑没有可处理的波形/RH。",
        "msg_lang_changed": "界面语言已切换为英文。",
        "msg_lang_changed_en": "Interface language switched to Chinese.",
        "map_empty_suffix": "可见文件",
        "map_points": "点",
        "map_color": "着色",
        "transform_group": "变换",
        "metric_group": "计算",
        "section_filter": "滤波",
        "section_transform": "变换",
        "section_norm": "归一化",
        "language_changed_status": "语言: 中文",
    },
    "en": {
        "app_title": "GEDI Process Desktop",
        "menu_file": "&File",
        "menu_view": "&View",
        "menu_transform": "&Transform",
        "menu_metric": "&Compute",
        "menu_batch": "&Batch",
        "menu_help": "&Help",
        "menu_language": "Language / 语言",
        "lang_zh": "中文（默认）",
        "lang_en": "English",
        "act_open": "Open Data Files(&O)…",
        "act_open_dir": "Open Folder(&D)…",
        "act_sample": "Load Sample Data(&S)",
        "act_clear": "Clear Loaded Files",
        "act_quit": "E&xit",
        "act_reset_zoom": "Reset Map View",
        "act_interaction": "Interaction: drag to pan · wheel zoom · right-click delete",
        "act_show_wave": "Show/Hide Waveform Panel",
        "act_show_right": "Show/Hide Properties",
        "act_batch": "Batch Process Files…",
        "act_about": "About / Extensibility",
        "act_recompute": "Recompute",
        "act_all": "Select All",
        "act_none": "Select None",
        "act_apply_default": "Comprehensive Metrics (Current Shot)",
        "tb_map": "Footprint Map (no basemap)",
        "tb_map_sel": "Selected Footprint",
        "panel_db": "Database / Files",
        "panel_info": "Info / Compute",
        "panel_display": "Properties",
        "panel_wave": "Waveform",
        "hint_check": "Checked = show footprints; unchecked = hide",
        "hint_right_delete": "Right-click a file to delete data",
        "ctx_delete": "Remove Data",
        "ctx_check_all": "Show All",
        "ctx_uncheck_all": "Hide All",
        "hint_wave": "Click a footprint on the map to view the waveform on the right\n(Height ↑ / Intensity →). Use Transform menu for FFT/wavelet.",
        "hint_info": "File info appears after import\nWaveform metrics after selecting a footprint",
        "hint_map_empty": "No footprints\nImport data and check files on the left",
        "hint_no_wave": "Select a footprint first",
        "tab_file_info": "File Info",
        "tab_metrics": "Waveform Metrics",
        "col_name": "Name",
        "col_type": "Type",
        "col_shots": "Shots",
        "col_metric": "Metric",
        "col_value": "Value",
        "no_file": "No file selected",
        "no_file_info": "No file selected\n\nSelect a file in the left tree",
        "product": "Product",
        "n_shots": "Total footprints",
        "path": "Path",
        "beams": "Beams",
        "color_by": "Footprint Color",
        "scalar_field": "Scalar Field",
        "display": "Display",
        "point_size": "Point Size",
        "show_circle": "Show ~25 m footprint circle",
        "beam_filter": "Beam Filter",
        "all_beams": "All beams",
        "modules": "Plugin Modules",
        "modules_hint": "Transforms/metrics load from the registry\nInvoke via Transform / Compute menus",
        "scalar_beam": "Beam (category)",
        "scalar_file": "File (category)",
        "scalar_sens": "Sensitivity",
        "scalar_quality": "Quality Flag",
        "scalar_dh": "Height Diff (canopy proxy)",
        "scalar_elev": "Elev Top",
        "axis_lon": "Longitude (°E)",
        "axis_lat": "Latitude (°N)",
        "axis_height": "Height (m)",
        "axis_intensity": "Intensity (DN)",
        "axis_height_rel": "Relative Height (m)",
        "axis_percentile": "Energy Percentile (%)",
        "axis_sample": "Waveform Sample",
        "wave_original": "Original",
        "wave_current": "Current",
        "empty_transforms": "(No registered transforms)",
        "metric_module": "Module: Comprehensive",
        "about_title": "About",
        "batch_title": "Batch — Multi-file Waveform/Footprint Metrics",
        "status_ready": "Ready",
        "msg_pick_footprint": "Click a footprint on the map first.",
        "msg_pick_file": "Import GEDI data files first.",
        "msg_no_series": "No waveform/RH for this footprint.",
        "msg_lang_changed": "Interface language switched to English.",
        "msg_lang_changed_en": "界面语言已切换为中文。",
        "map_empty_suffix": "visible files",
        "map_points": "pts",
        "map_color": "color",
        "transform_group": "Transform",
        "metric_group": "Compute",
        "section_filter": "Filters",
        "section_transform": "Transforms",
        "section_norm": "Normalization",
        "language_changed_status": "Language: English",
    },
}

_listeners: List = []


def t(key: str, **kwargs) -> str:
    lang = LANG if LANG in _STRINGS else "zh"
    text = _STRINGS[lang].get(key)
    if text is None:
        text = _STRINGS["zh"].get(key, key)
    if kwargs:
        try:
            return text.format(**kwargs)
        except Exception:
            return text
    return text


def get_language() -> str:
    return LANG


def set_language(lang: str) -> str:
    global LANG
    if lang not in _STRINGS:
        lang = "zh"
    LANG = lang
    for fn in list(_listeners):
        try:
            fn(lang)
        except Exception:
            pass
    return LANG


def add_listener(fn) -> None:
    if fn not in _listeners:
        _listeners.append(fn)


def remove_listener(fn) -> None:
    if fn in _listeners:
        _listeners.remove(fn)


def lang_label() -> str:
    return "中文" if LANG == "zh" else "English"
