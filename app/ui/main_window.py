"""Main window — CloudCompare-like modular workstation.

Layout:
  left-top     文件树（复选框控制光斑显隐）
  left-bottom  文件信息 / 波形计算
  center       光斑位置图（滚轮缩放）
  map-right    波形演示（高度竖直 / 强度横轴）
  far-right    显示属性
Menus: 文件 | 视图 | 变换 | 计算 | 批处理 | 帮助(语言 中文/English)
"""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List, Optional

from PySide6.QtCore import Qt
from PySide6.QtGui import QAction, QActionGroup, QKeySequence
from PySide6.QtWidgets import (
    QDialog,
    QDialogButtonBox,
    QDoubleSpinBox,
    QFormLayout,
    QComboBox,
    QFileDialog,
    QLabel,
    QMainWindow,
    QMessageBox,
    QSpinBox,
    QSplitter,
    QStatusBar,
    QVBoxLayout,
    QWidget,
    QHBoxLayout,
)

from app.config import APP_NAME, APP_VERSION, MAP_MAX_POINTS, sample_data_dir
from app.core.registry import METRICS, TRANSFORMS, ParamSpec
from app.core.session import SessionState
from app.data.gedi_loader import GediLoader, ShotMeta, WaveformResult
from app.i18n import get_language, set_language, t
from app.processing import transforms as transforms_mod  # noqa: F401
from app.processing import builtin_metrics  # noqa: F401
from app.processing.metrics import compute_waveform_metrics
from app.processing.transforms import apply_transform_by_id
from app.ui.batch_dialog import BatchDialog
from app.ui.file_tree import FileTreePanel
from app.ui.footprint_map import FootprintMapPanel
from app.ui.info_panel import InfoPanel
from app.ui.property_panel import DisplayPanel
from app.ui.styles import APP_STYLESHEET
from app.ui.waveform_view import WaveformPanel


class ParamDialog(QDialog):
    def __init__(self, title: str, description: str, params: List[ParamSpec], parent=None):
        super().__init__(parent)
        self.setWindowTitle(title)
        self._params = params
        self._widgets: Dict[str, QWidget] = {}
        lay = QVBoxLayout(self)
        if description:
            lab = QLabel(description)
            lab.setWordWrap(True)
            lay.addWidget(lab)
        form = QFormLayout()
        for p in params:
            if p.kind == "int":
                w = QSpinBox()
                w.setMinimum(int(p.minimum) if p.minimum is not None else 0)
                w.setMaximum(int(p.maximum) if p.maximum is not None else 999999)
                w.setSingleStep(int(p.step) if p.step else 1)
                w.setValue(int(p.default))
            elif p.kind == "choice":
                w = QComboBox()
                for c in p.choices:
                    w.addItem(c)
                w.setCurrentIndex(int(p.default) if p.default is not None else 0)
            else:
                w = QDoubleSpinBox()
                w.setMinimum(float(p.minimum) if p.minimum is not None else -1e9)
                w.setMaximum(float(p.maximum) if p.maximum is not None else 1e9)
                w.setDecimals(4)
                w.setSingleStep(float(p.step) if p.step else 0.01)
                w.setValue(float(p.default))
            self._widgets[p.name] = w
            form.addRow(p.label, w)
        lay.addLayout(form)
        btns = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)
        lay.addWidget(btns)

    def values(self) -> Dict[str, Any]:
        out = {}
        for p in self._params:
            w = self._widgets[p.name]
            if p.kind == "choice":
                out[p.name] = w.currentIndex()
            elif p.kind == "int":
                out[p.name] = int(w.value())
            else:
                out[p.name] = float(w.value())
        return out


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.resize(1560, 940)
        self.setStyleSheet(APP_STYLESHEET)

        self.loader = GediLoader()
        self.session = SessionState()
        self._map_points: List[ShotMeta] = []

        self._build_menu()
        self._build_toolbar()
        self._build_central()
        self._build_status()
        self._connect_signals()
        self._rebuild_transform_menu()
        self._rebuild_metric_menu()
        self.apply_language()
        self.waveform_panel.clear_message(t("hint_wave"))
        self.info_panel.show_message(t("hint_info"))
        self._update_status()

    # ------------------------------------------------------------------ menu
    def _build_menu(self):
        mb = self.menuBar()

        self.m_file = mb.addMenu(t("menu_file"))
        self.act_open = QAction(t("act_open"), self)
        self.act_open.setShortcut(QKeySequence.Open)
        self.act_open.triggered.connect(self.open_files_dialog)
        self.act_open_dir = QAction(t("act_open_dir"), self)
        self.act_open_dir.triggered.connect(self.open_folder_dialog)
        self.act_sample = QAction(t("act_sample"), self)
        self.act_sample.triggered.connect(self.load_sample_data)
        self.act_clear = QAction(t("act_clear"), self)
        self.act_clear.triggered.connect(self.clear_files)
        self.act_quit = QAction(t("act_quit"), self)
        self.act_quit.setShortcut(QKeySequence.Quit)
        self.act_quit.triggered.connect(self.close)
        for a in (self.act_open, self.act_open_dir, self.act_sample):
            self.m_file.addAction(a)
        self.m_file.addSeparator()
        self.m_file.addAction(self.act_clear)
        self.m_file.addSeparator()
        self.m_file.addAction(self.act_quit)

        self.m_view = mb.addMenu(t("menu_view"))
        self.act_reset_zoom = QAction(t("act_reset_zoom"), self)
        self.act_reset_zoom.triggered.connect(self.map_panel_reset)
        self.act_show_wave = QAction(t("act_show_wave"), self)
        self.act_show_wave.setCheckable(True)
        self.act_show_wave.setChecked(True)
        self.act_show_wave.toggled.connect(self._toggle_waveform_panel)
        self.act_show_right = QAction(t("act_show_right"), self)
        self.act_show_right.setCheckable(True)
        self.act_show_right.setChecked(True)
        self.act_show_right.toggled.connect(self._toggle_right_panel)
        self.act_help_zoom = QAction(t("act_interaction"), self)
        self.act_help_zoom.triggered.connect(self.show_about)
        self.m_view.addAction(self.act_reset_zoom)
        self.m_view.addAction(self.act_help_zoom)
        self.m_view.addSeparator()
        self.m_view.addAction(self.act_show_wave)
        self.m_view.addAction(self.act_show_right)

        self.m_transform = mb.addMenu(t("menu_transform"))
        self.m_metric = mb.addMenu(t("menu_metric"))

        self.m_batch = mb.addMenu(t("menu_batch"))
        self.act_batch = QAction(t("act_batch"), self)
        self.act_batch.triggered.connect(self.open_batch)
        self.m_batch.addAction(self.act_batch)

        self.m_help = mb.addMenu(t("menu_help"))
        self.m_lang = self.m_help.addMenu(t("menu_language"))
        self.lang_group = QActionGroup(self)
        self.lang_group.setExclusive(True)
        self.act_lang_zh = QAction(t("lang_zh"), self, checkable=True)
        self.act_lang_en = QAction(t("lang_en"), self, checkable=True)
        self.lang_group.addAction(self.act_lang_zh)
        self.lang_group.addAction(self.act_lang_en)
        if get_language() == "zh":
            self.act_lang_zh.setChecked(True)
        else:
            self.act_lang_en.setChecked(True)
        self.act_lang_zh.triggered.connect(lambda: self.switch_language("zh"))
        self.act_lang_en.triggered.connect(lambda: self.switch_language("en"))
        self.m_lang.addAction(self.act_lang_zh)
        self.m_lang.addAction(self.act_lang_en)
        self.m_help.addSeparator()
        self.act_about = QAction(t("act_about"), self)
        self.act_about.triggered.connect(self.show_about)
        self.m_help.addAction(self.act_about)

    def _rebuild_transform_menu(self):
        self.m_transform.clear()
        by_cat = TRANSFORMS.by_category()
        if not by_cat:
            self.m_transform.addAction(QAction(t("empty_transforms"), self))
            return
        # stable category order
        cat_order = ["归一化", "滤波", "变换"]
        if get_language() == "en":
            cat_map = {"归一化": "Normalization", "滤波": "Filters", "变换": "Transforms"}
        else:
            cat_map = {}
        cats = list(by_cat.keys())
        for c in cat_order:
            if c in cats:
                cats.remove(c)
                cats.insert(0, c)
        cats = list(reversed([c for c in cat_order if c in by_cat])) + [c for c in by_cat if c not in cat_order]
        # simpler: use original order from registry
        for cat, specs in TRANSFORMS.by_category().items():
            label = cat_map.get(cat, cat) if get_language() == "en" else cat
            self.m_transform.addSection(label)
            for spec in specs:
                act = QAction(spec.name, self)
                act.setData(spec.id)
                if spec.description:
                    act.setToolTip(spec.description)
                act.triggered.connect(lambda checked=False, sid=spec.id: self.run_transform(sid))
                self.m_transform.addAction(act)

    def _rebuild_metric_menu(self):
        self.m_metric.clear()
        act_all = QAction(t("act_apply_default"), self)
        act_all.triggered.connect(self.compute_current_metrics)
        self.m_metric.addAction(act_all)
        self.m_metric.addSeparator()
        for cat, specs in METRICS.by_category().items():
            self.m_metric.addSection(cat)
            for spec in specs:
                act = QAction(spec.name, self)
                act.setToolTip(spec.description)
                act.triggered.connect(lambda checked=False, sid=spec.id: self.run_metric(sid))
                self.m_metric.addAction(act)
        self.m_metric.addSeparator()
        act_reg = QAction(t("act_about"), self)
        act_reg.triggered.connect(self.show_about)
        self.m_metric.addAction(act_reg)

    def _build_toolbar(self):
        tb = self.addToolBar("Main")
        tb.setObjectName("MainToolbar")
        tb.setMovable(False)
        self._toolbar = tb
        self._rebuild_toolbar()

    def _rebuild_toolbar(self):
        tb = self._toolbar
        tb.clear()
        tb.addAction(self.act_open)
        tb.addAction(self.act_sample)
        tb.addSeparator()
        tb.addAction(self.act_reset_zoom)
        tb.addSeparator()
        for sid in ("lowpass", "highpass", "savgol", "fft", "dwt_denoise"):
            spec = TRANSFORMS.get(sid)
            if spec is None:
                continue
            act = QAction(spec.name, self)
            act.triggered.connect(lambda checked=False, x=sid: self.run_transform(x))
            tb.addAction(act)

    def _build_central(self):
        central = QWidget()
        self.setCentralWidget(central)
        outer = QHBoxLayout(central)
        outer.setContentsMargins(6, 6, 6, 6)

        self.splitter = QSplitter(Qt.Horizontal)

        # LEFT: file tree + info
        left_split = QSplitter(Qt.Vertical)
        self.file_tree = FileTreePanel()
        self.info_panel = InfoPanel()
        left_split.addWidget(self.file_tree)
        left_split.addWidget(self.info_panel)
        left_split.setStretchFactor(0, 3)
        left_split.setStretchFactor(1, 2)

        # CENTER-RIGHT group: MAP | WAVEFORM (waveform on the right of map)
        mid = QSplitter(Qt.Horizontal)
        self.map_panel = FootprintMapPanel()
        self.waveform_panel = WaveformPanel()
        mid.addWidget(self.map_panel)
        mid.addWidget(self.waveform_panel)
        mid.setStretchFactor(0, 58)   # map
        mid.setStretchFactor(1, 42)   # waveform right of map
        mid.setCollapsible(0, False)
        mid.setCollapsible(1, True)
        self._mid_split = mid

        # FAR RIGHT: display properties
        self.display_panel = DisplayPanel()
        self._right_panel = self.display_panel

        self.splitter.addWidget(left_split)
        self.splitter.addWidget(mid)
        self.splitter.addWidget(self.display_panel)
        self.splitter.setStretchFactor(0, 20)
        self.splitter.setStretchFactor(1, 62)
        self.splitter.setStretchFactor(2, 16)
        outer.addWidget(self.splitter)

        self.info_panel.set_recompute_handler(self.compute_current_metrics)

    def _build_status(self):
        sb = QStatusBar()
        self.setStatusBar(sb)
        self.lbl_status = QLabel("…")
        sb.addWidget(self.lbl_status, 1)

    def _connect_signals(self):
        self.file_tree.visibility_changed.connect(self._refresh_map)
        self.file_tree.selection_changed.connect(self._on_file_selected)
        self.file_tree.file_remove_requested.connect(self.remove_file_by_id)
        self.map_panel.footprint_selected.connect(self.on_footprint_selected)
        self.display_panel.scalar_changed.connect(self._on_scalar_changed)
        self.display_panel.point_size_changed.connect(self._on_display_changed)
        self.display_panel.show_circles_changed.connect(self._on_display_changed)
        self.display_panel.beam_filter_changed.connect(lambda _: self._refresh_map())

    def remove_file_by_id(self, file_id: int):
        """Right-click delete on file tree: unload HDF5 and refresh UI."""
        gf = self.loader.get_file(file_id)
        name = gf.path.name if gf else str(file_id)
        self.loader.close_file(file_id)
        self.session.visible_file_ids.discard(file_id)
        self.file_tree.remove_file(file_id)
        if self.session.selected_file_id == file_id:
            self.session.selected_file_id = None
            self.info_panel.show_message(t("hint_info"))
        # clear waveform if it came from removed file
        res = self.session.selected_result
        if res is not None and self.session.selected_shot is not None:
            if self.session.selected_shot.file_id == file_id:
                self.session.selected_result = None
                self.session.selected_shot = None
                self.waveform_panel.clear_message(t("hint_wave"))
        self._refresh_map()
        self._update_status(f"removed {name}")
        self.log(f"removed {name}")

    # ------------------------------------------------------------- i18n
    def switch_language(self, lang: str):
        set_language(lang)
        self.apply_language()
        if lang == "en":
            QMessageBox.information(self, "Language", t("msg_lang_changed"))
        else:
            QMessageBox.information(self, "语言", t("msg_lang_changed_en"))

    def apply_language(self):
        self.setWindowTitle(f"{t('app_title')} v{APP_VERSION}")
        # rebuild menus (labels depend on language)
        mb = self.menuBar()
        mb.clear()
        self._build_menu()
        self._rebuild_transform_menu()
        self._rebuild_metric_menu()
        self._rebuild_toolbar()

        self.file_tree.retranslate()
        self.info_panel.retranslate()
        self.display_panel.retranslate()
        self.waveform_panel.retranslate()
        self.map_panel.canvas._style_axes()
        # refresh map title/axes without losing points
        if self._map_points:
            self._refresh_map()
        else:
            self.map_panel.set_points([], "")

        if self.session.selected_result is not None:
            self.waveform_panel.show_base(self.session.selected_result)
        else:
            self.waveform_panel.clear_message(t("hint_wave"))

        if self.session.selected_file_id is not None:
            gf = self.loader.get_file(self.session.selected_file_id)
            if gf:
                self.info_panel.show_file(gf)
        else:
            self.info_panel.show_message(t("hint_info"))

        self._update_status(t("language_changed_status"))

    # ------------------------------------------------------------- status
    def _update_status(self, extra: str = ""):
        n_files = len(self.loader.files)
        n_vis = len(self.session.visible_file_ids)
        n_shots = sum(f.n_shots for f in self.loader.files)
        lang = "中文" if get_language() == "zh" else "EN"
        msg = (
            f"{t('app_title')} · {t('map_empty_suffix')} {n_vis}/{n_files} · "
            f"{n_shots:,} · {len(TRANSFORMS.all())} transforms / {len(METRICS.all())} metrics · {lang}"
        )
        if extra:
            msg += " · " + extra
        self.lbl_status.setText(msg)

    def log(self, msg: str):
        self.session.log(msg)
        self._update_status(msg)

    # ------------------------------------------------------------- files
    def open_files_dialog(self):
        paths, _ = QFileDialog.getOpenFileNames(
            self,
            t("act_open"),
            str(sample_data_dir()),
            "GEDI HDF5 (*.h5 *.hdf5);;All Files (*)",
        )
        if paths:
            self.load_paths([Path(p) for p in paths])

    def open_folder_dialog(self):
        folder = QFileDialog.getExistingDirectory(self, t("act_open_dir"), str(sample_data_dir()))
        if not folder:
            return
        root = Path(folder)
        paths = sorted(root.rglob("*.h5")) + sorted(root.rglob("*.hdf5"))
        if not paths:
            QMessageBox.information(self, t("act_open_dir"), str(root))
            return
        self.load_paths(paths)

    def load_sample_data(self):
        root = sample_data_dir()
        if not root.exists():
            QMessageBox.warning(self, t("act_sample"), str(root))
            return
        paths = sorted(root.rglob("*.h5")) + sorted(root.rglob("*.hdf5"))
        if not paths:
            QMessageBox.warning(self, t("act_sample"), str(root))
            return
        self.load_paths(paths)

    def clear_files(self):
        self.loader.close_all()
        self.file_tree.clear_all()
        self.session.visible_file_ids.clear()
        self.session.selected_result = None
        self.session.selected_shot = None
        self.session.transform_chain.clear()
        self._map_points = []
        self.map_panel.set_points([], "")
        self.waveform_panel.clear_message(t("hint_wave"))
        self.info_panel.show_message(t("hint_info"))
        self._update_status()

    def load_paths(self, paths: List[Path]):
        errors = []
        loaded = []
        for p in paths:
            try:
                gf = self.loader.open_file(p)
                if gf is None:
                    continue
                loaded.append(gf)
                self.session.visible_file_ids.add(gf.file_id)
                self.file_tree.add_file(gf, checked=True)
            except Exception as e:
                errors.append(f"{p.name}: {e}")
        if loaded:
            self.session.selected_file_id = loaded[0].file_id
            self.file_tree.select_file_id(loaded[0].file_id)
            self.info_panel.show_file(loaded[0])
            self._refresh_map()
        self._update_status()
        if errors:
            QMessageBox.warning(self, "Load", "\n".join(errors[:12]))
        elif loaded:
            self.log(f"loaded {len(loaded)}")

    def _on_file_selected(self, gf):
        if gf is None:
            return
        self.session.selected_file_id = gf.file_id
        self.info_panel.show_file(gf)
        self._update_status(gf.path.name)

    # --------------------------------------------------------------- map
    def _refresh_map(self):
        vis = self.file_tree.checked_file_ids()
        self.session.visible_file_ids = set(vis)
        beam = self.display_panel.current_beam()
        scalar = self.display_panel.current_scalar()
        self.map_panel.set_display_options(
            scalar=scalar,
            point_size=self.display_panel.point_size(),
            show_circles=self.display_panel.chk_circle.isChecked(),
        )
        self._map_points = self.loader.collect_map_points(
            file_ids=sorted(vis) if vis else [],
            beam=beam,
            max_points=MAP_MAX_POINTS,
            quality_only=False,
        )
        if get_language() == "zh":
            suffix = f"{t('map_empty_suffix')} {len(vis)} · {len(self._map_points)} {t('map_points')}"
        else:
            suffix = f"{len(vis)} {t('map_empty_suffix')} · {len(self._map_points)} {t('map_points')}"
        if beam:
            suffix += f" · {beam}"
        suffix += f" · {t('map_color')}:{scalar}"
        self.map_panel.set_points(self._map_points, suffix)
        self._update_status()

    def _on_scalar_changed(self, *_):
        self._refresh_map()

    def _on_display_changed(self, *_):
        self._refresh_map()

    def map_panel_reset(self):
        self.map_panel.reset_view()

    def _toggle_waveform_panel(self, show: bool):
        self.session.waveform_visible = show
        self.waveform_panel.setVisible(show)

    def _toggle_right_panel(self, show: bool):
        self.display_panel.setVisible(show)

    # ---------------------------------------------------------- selection
    def on_footprint_selected(self, sm: ShotMeta):
        self.session.selected_shot = sm
        try:
            result = self.loader.load_waveform_result(sm.file_id, sm.beam, sm.index)
        except Exception as e:
            QMessageBox.critical(self, "Error", str(e))
            return
        if result is None:
            QMessageBox.warning(self, "Footprint", t("hint_no_wave"))
            return
        self.session.selected_result = result
        self.session.transform_chain.clear()
        if not self.waveform_panel.isVisible():
            self.act_show_wave.setChecked(True)
        self.waveform_panel.show_base(result)
        gf = self.loader.get_file(sm.file_id)
        if gf:
            self.info_panel.show_file(gf)
        try:
            metrics = compute_waveform_metrics(result, None)
            self.session.last_metrics = metrics
            self.info_panel.show_metrics(result, metrics)
        except Exception:
            self.info_panel.show_metrics(result)
        self.log(f"{result.product} {result.beam} {result.shot_number}")

    # -------------------------------------------------------- transforms
    def run_transform(self, spec_id: str):
        spec = TRANSFORMS.get(spec_id)
        if spec is None:
            QMessageBox.warning(self, t("menu_transform"), spec_id)
            return
        result = self.session.selected_result
        if result is None:
            QMessageBox.information(self, t("menu_transform"), t("msg_pick_footprint"))
            return
        if result.has_waveform():
            y = result.waveform
        elif result.has_rh():
            y = result.rh
        else:
            QMessageBox.information(self, t("menu_transform"), t("msg_no_series"))
            return

        params = {}
        if spec.params:
            dlg = ParamDialog(spec.name, spec.description, spec.params, self)
            if dlg.exec() != QDialog.Accepted:
                return
            params = dlg.values()

        try:
            out = apply_transform_by_id(y, spec.id, params)
        except Exception as e:
            QMessageBox.critical(self, "Error", f"{spec.name}\n{e}")
            return

        self.session.transform_params[spec.id] = params
        if spec.result_kind == "signal" and spec_id not in self.session.transform_chain:
            self.session.transform_chain.append(spec_id)
        self.waveform_panel.apply_transform_result(result, out)

        try:
            metrics = compute_waveform_metrics(result, None)
            if out.get("y") is not None and spec.result_kind == "signal":
                import numpy as np

                yy = np.asarray(out["y"], dtype=float)
                yy = yy[np.isfinite(yy)]
                if yy.size:
                    metrics = dict(metrics)
                    metrics["wf_mean"] = float(np.mean(yy))
                    metrics["wf_std"] = float(np.std(yy))
                    metrics["wf_min"] = float(np.min(yy))
                    metrics["wf_max"] = float(np.max(yy))
                    metrics["wf_energy"] = float(np.nansum(np.clip(yy - np.min(yy), 0, None)))
            self.session.last_metrics = metrics
            self.info_panel.show_metrics(result, metrics)
        except Exception:
            pass
        self.log(out.get("label", spec.name))

    def run_metric(self, spec_id: str):
        spec = METRICS.get(spec_id)
        if spec is None:
            return
        result = self.session.selected_result
        if result is None:
            QMessageBox.information(self, t("menu_metric"), t("msg_pick_footprint"))
            return
        try:
            metrics = spec.func(result)
        except Exception as e:
            QMessageBox.critical(self, "Error", f"{spec.name}\n{e}")
            return
        self.session.last_metrics = metrics
        self.info_panel.show_metrics(result, metrics)
        self.log(spec.name)

    def compute_current_metrics(self):
        result = self.session.selected_result
        if result is None:
            QMessageBox.information(self, t("menu_metric"), t("msg_pick_footprint"))
            return
        metrics = compute_waveform_metrics(result, None)
        self.session.last_metrics = metrics
        self.info_panel.show_metrics(result, metrics)
        self.log(t("act_apply_default"))

    def open_batch(self):
        if not self.loader.files:
            QMessageBox.information(self, t("menu_batch"), t("msg_pick_file"))
            return
        dlg = BatchDialog(self.loader, None, self, visible_ids=self.session.visible_file_ids)
        dlg.exec()

    def show_about(self):
        zh = get_language() == "zh"
        tnames = "\n".join(f"· {s.category} / {s.name}" for s in TRANSFORMS.all())
        mnames = "\n".join(f"· {s.category} / {s.name}" for s in METRICS.all())
        if zh:
            body = (
                f"<b>{t('app_title')} v{APP_VERSION}</b><br><br>"
                "布局：左侧文件树（复选框控制光斑显隐，右键删除数据）+ 信息/计算；"
                "中央光斑地图（<b>拖动平移</b>、<b>滚轮缩放</b>，缩放限制在合理经纬度范围）；"
                "<b>地图右侧波形</b>（高度↑ / 强度→，同样支持拖动与缩放）；右侧显示属性。<br>"
                "帮助 → 语言：中文 / English（默认中文）。<br><br>"
                "<b>扩展</b><br>"
                "变换：<code>app/processing/transforms.py</code> → <code>@register_transform</code><br>"
                "计算：<code>app/processing/builtin_metrics.py</code> → <code>@register_metric</code><br><br>"
                f"<b>变换模块</b><br>{tnames}<br><br>"
                f"<b>计算模块</b><br>{mnames}"
            )
        else:
            body = (
                f"<b>{t('app_title')} v{APP_VERSION}</b><br><br>"
                "Layout: left file tree (checkbox visibility, right-click to remove data) + info/metrics; "
                "center footprint map (<b>drag to pan</b>, <b>wheel zoom</b> with adaptive min/max bounds); "
                "<b>waveform on the right of the map</b> (Height ↑ / Intensity →, also pannable/zoomable); "
                "properties on the far right.<br>"
                "Help → Language: 中文 / English (default Chinese).<br><br>"
                "<b>Extensibility</b><br>"
                "Transforms: <code>app/processing/transforms.py</code> → <code>@register_transform</code><br>"
                "Metrics: <code>app/processing/builtin_metrics.py</code> → <code>@register_metric</code><br><br>"
                f"<b>Transforms</b><br>{tnames}<br><br>"
                f"<b>Metrics</b><br>{mnames}"
            )
        QMessageBox.about(self, t("about_title"), body)

    def closeEvent(self, event):
        try:
            self.loader.close_all()
        except Exception:
            pass
        super().closeEvent(event)
