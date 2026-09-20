"""Left-top DB tree: loaded files with visibility checkboxes (CloudCompare-like)."""

from __future__ import annotations

from typing import Dict, List, Optional, Set

from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QAction, QColor
from PySide6.QtWidgets import (
    QLabel,
    QMenu,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

from app.config import BEAM_COLORS, MUTED, NASA
from app.data.gedi_loader import GediFile
from app.i18n import t


class FileTreePanel(QWidget):
    """File database tree. Checkbox controls footprints; right-click deletes file."""

    visibility_changed = Signal()
    selection_changed = Signal(object)  # GediFile | None
    file_remove_requested = Signal(int)  # file_id

    def __init__(self, parent=None):
        super().__init__(parent)
        self._files: Dict[int, GediFile] = {}
        self._block = False
        self._build()

    def _build(self):
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(4)
        header = QLabel(t("panel_db"))
        self.header_label = header
        header.setStyleSheet(f"color:{NASA}; font-weight:600; padding:2px 4px;")
        lay.addWidget(header)
        self.tree = QTreeWidget()
        self.tree.setHeaderLabels([t("col_name"), t("col_type"), t("col_shots")])
        self.tree.setColumnWidth(0, 220)
        self.tree.setColumnWidth(1, 48)
        self.tree.setColumnWidth(2, 70)
        self.tree.setRootIsDecorated(True)
        self.tree.setUniformRowHeights(True)
        self.tree.itemChanged.connect(self._on_item_changed)
        self.tree.itemSelectionChanged.connect(self._on_selection_changed)
        self.tree.setContextMenuPolicy(Qt.CustomContextMenu)
        self.tree.customContextMenuRequested.connect(self._show_context_menu)
        lay.addWidget(self.tree)
        hint = QLabel(t("hint_check"))
        self.hint_label = hint
        hint.setStyleSheet(f"color:{MUTED}; font-size:11px; padding:2px 4px;")
        lay.addWidget(hint)

    def _show_context_menu(self, pos):
        item = self.tree.itemAt(pos)
        if item is None:
            return
        parent = item.parent()
        target = parent if parent is not None else item
        fid = target.data(0, Qt.UserRole)
        if fid is None:
            return
        fid = int(fid)
        menu = QMenu(self)
        act_del = QAction(t("ctx_delete"), self)
        act_del.triggered.connect(lambda: self.file_remove_requested.emit(fid))
        menu.addAction(act_del)
        menu.addSeparator()
        act_check = QAction(t("ctx_check_all"), self)
        act_check.triggered.connect(lambda: self.set_all_checked(True))
        act_uncheck = QAction(t("ctx_uncheck_all"), self)
        act_uncheck.triggered.connect(lambda: self.set_all_checked(False))
        menu.addAction(act_check)
        menu.addAction(act_uncheck)
        menu.exec(self.tree.viewport().mapToGlobal(pos))

    def retranslate(self):
        self.header_label.setText(t("panel_db"))
        self.tree.setHeaderLabels([t("col_name"), t("col_type"), t("col_shots")])
        self.hint_label.setText(t("hint_check") + " · " + t("hint_right_delete"))

    def set_files(self, files: List[GediFile], visible_ids: Optional[Set[int]] = None):
        self._block = True
        self._files = {f.file_id: f for f in files}
        self.tree.clear()
        vis = visible_ids if visible_ids is not None else {f.file_id for f in files}
        for gf in files:
            item = QTreeWidgetItem()
            item.setText(0, gf.path.name)
            item.setText(1, gf.product)
            item.setText(2, f"{gf.n_shots:,}")
            item.setData(0, Qt.UserRole, gf.file_id)
            item.setFlags(item.flags() | Qt.ItemIsUserCheckable)
            item.setCheckState(0, Qt.Checked if gf.file_id in vis else Qt.Unchecked)
            color = QColor(NASA) if gf.product == "L1B" else QColor("#1AA3C8")
            item.setForeground(1, color)
            # beam children (display only)
            for beam, meta in gf.beams.items():
                ch = QTreeWidgetItem()
                ch.setText(0, beam)
                ch.setText(1, "")
                ch.setText(2, f"{meta['shot_number'].shape[0]:,}")
                ch.setForeground(0, QColor(BEAM_COLORS.get(beam, MUTED)))
                ch.setFlags(ch.flags() & ~Qt.ItemIsUserCheckable)
                item.addChild(ch)
            self.tree.addTopLevelItem(item)
        self._block = False

    def add_file(self, gf: GediFile, checked: bool = True):
        self._block = True
        self._files[gf.file_id] = gf
        item = QTreeWidgetItem()
        item.setText(0, gf.path.name)
        item.setText(1, gf.product)
        item.setText(2, f"{gf.n_shots:,}")
        item.setData(0, Qt.UserRole, gf.file_id)
        item.setFlags(item.flags() | Qt.ItemIsUserCheckable)
        item.setCheckState(0, Qt.Checked if checked else Qt.Unchecked)
        color = QColor(NASA) if gf.product == "L1B" else QColor("#1AA3C8")
        item.setForeground(1, color)
        for beam, meta in gf.beams.items():
            ch = QTreeWidgetItem()
            ch.setText(0, beam)
            ch.setText(2, f"{meta['shot_number'].shape[0]:,}")
            ch.setForeground(0, QColor(BEAM_COLORS.get(beam, MUTED)))
            item.addChild(ch)
        self.tree.addTopLevelItem(item)
        self._block = False
        self.visibility_changed.emit()

    def remove_file(self, file_id: int):
        self._block = True
        self._files.pop(file_id, None)
        for i in range(self.tree.topLevelItemCount()):
            it = self.tree.topLevelItem(i)
            if it.data(0, Qt.UserRole) == file_id:
                self.tree.takeTopLevelItem(i)
                break
        self._block = False
        self.visibility_changed.emit()
        self.selection_changed.emit(self.current_file())

    def clear_all(self):
        self._block = True
        self._files.clear()
        self.tree.clear()
        self._block = False
        self.visibility_changed.emit()

    def checked_file_ids(self) -> Set[int]:
        ids: Set[int] = set()
        for i in range(self.tree.topLevelItemCount()):
            it = self.tree.topLevelItem(i)
            if it.checkState(0) == Qt.Checked:
                fid = it.data(0, Qt.UserRole)
                if fid is not None:
                    ids.add(int(fid))
        return ids

    def all_file_ids(self) -> Set[int]:
        return set(self._files.keys())

    def set_all_checked(self, checked: bool):
        self._block = True
        state = Qt.Checked if checked else Qt.Unchecked
        for i in range(self.tree.topLevelItemCount()):
            self.tree.topLevelItem(i).setCheckState(0, state)
        self._block = False
        self.visibility_changed.emit()

    def current_file(self) -> Optional[GediFile]:
        items = self.tree.selectedItems()
        if not items:
            return None
        it = items[0]
        # if beam child selected, use parent
        parent = it.parent()
        fid = None
        if parent is not None:
            fid = parent.data(0, Qt.UserRole)
        else:
            fid = it.data(0, Qt.UserRole)
        if fid is None:
            return None
        return self._files.get(int(fid))

    def _on_item_changed(self, item: QTreeWidgetItem, col: int):
        if self._block or col != 0:
            return
        if item.parent() is not None:
            return
        self.visibility_changed.emit()

    def _on_selection_changed(self):
        self.selection_changed.emit(self.current_file())

    def select_file_id(self, file_id: int):
        self._block = True
        for i in range(self.tree.topLevelItemCount()):
            it = self.tree.topLevelItem(i)
            if it.data(0, Qt.UserRole) == file_id:
                self.tree.setCurrentItem(it)
                break
        self._block = False
