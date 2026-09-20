"""Qt stylesheet — scientific instrument panel (light)."""

from app.config import CYAN, GRID, INK, MUTED, NASA, PAPER, PANEL, WARN

APP_STYLESHEET = f"""
* {{
    font-family: "Segoe UI", "Microsoft YaHei", "PingFang SC", sans-serif;
    font-size: 12px;
    color: {INK};
}}
QMainWindow, QDialog {{
    background: {PAPER};
}}
QToolBar {{
    background: {PANEL};
    border-bottom: 1px solid {GRID};
    spacing: 6px;
    padding: 4px 8px;
}}
QToolButton, QPushButton {{
    background: {PANEL};
    border: 1px solid {GRID};
    border-radius: 4px;
    padding: 5px 12px;
    min-height: 24px;
}}
QPushButton:hover {{
    border-color: {NASA};
    background: #F3F7FB;
}}
QPushButton:pressed {{
    background: #E6EEF7;
}}
QPushButton:disabled {{
    color: {MUTED};
    background: #F5F7FA;
}}
QPushButton#primary {{
    background: {NASA};
    color: white;
    border: 1px solid {NASA};
    font-weight: 600;
}}
QPushButton#primary:hover {{
    background: #0A347D;
}}
QPushButton#accent {{
    background: {CYAN};
    color: white;
    border: 1px solid {CYAN};
    font-weight: 600;
}}
QGroupBox {{
    background: {PANEL};
    border: 1px solid {GRID};
    border-radius: 6px;
    margin-top: 10px;
    padding: 8px 10px 10px 10px;
    font-weight: 600;
}}
QGroupBox::title {{
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 4px;
    color: {NASA};
}}
QListWidget, QTableWidget, QTreeWidget, QPlainTextEdit, QTextEdit, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {{
    background: {PANEL};
    border: 1px solid {GRID};
    border-radius: 4px;
    padding: 3px 4px;
    selection-background-color: #D6E6F5;
    selection-color: {INK};
}}
QHeaderView::section {{
    background: #E8EEF4;
    border: 1px solid {GRID};
    padding: 4px 6px;
    font-weight: 600;
}}
QStatusBar {{
    background: {PANEL};
    border-top: 1px solid {GRID};
    color: {MUTED};
}}
QSplitter::handle {{
    background: {GRID};
    width: 3px;
}}
QTabWidget::pane {{
    border: 1px solid {GRID};
    background: {PANEL};
    border-radius: 4px;
}}
QTabBar::tab {{
    background: #E8EEF4;
    border: 1px solid {GRID};
    border-bottom: none;
    padding: 6px 14px;
    margin-right: 2px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
}}
QTabBar::tab:selected {{
    background: {PANEL};
    color: {NASA};
    font-weight: 600;
}}
QCheckBox, QRadioButton, QLabel {{
    background: transparent;
    border: none;
}}
QProgressBar {{
    border: 1px solid {GRID};
    border-radius: 4px;
    text-align: center;
    background: {PANEL};
    min-height: 16px;
}}
QProgressBar::chunk {{
    background: {NASA};
    border-radius: 3px;
}}
QScrollBar:vertical {{
    background: {PAPER};
    width: 12px;
    margin: 0;
}}
QScrollBar::handle:vertical {{
    background: #C5D0DB;
    border-radius: 4px;
    min-height: 24px;
}}
QMenuBar {{
    background: {PANEL};
    border-bottom: 1px solid {GRID};
}}
QMenuBar::item:selected {{
    background: #E8EEF4;
}}
QMenu {{
    background: {PANEL};
    border: 1px solid {GRID};
}}
QMenu::item:selected {{
    background: #D6E6F5;
}}
QToolTip {{
    background: {INK};
    color: white;
    border: none;
    padding: 4px 6px;
}}
"""
