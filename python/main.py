"""GEDI Process Desktop entry point."""

from __future__ import annotations

import os
import sys
from pathlib import Path

# Ensure project root is on sys.path when launched from elsewhere
ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

# Matplotlib Qt backend + Chinese fonts
os.environ.setdefault("QT_API", "pyside6")

import matplotlib

matplotlib.use("QtAgg")
matplotlib.rcParams["figure.dpi"] = 100
matplotlib.rcParams["savefig.dpi"] = 150

# Register CJK fonts after backend selection
from app.ui import mpl_fonts  # noqa: E402, F401


def main() -> int:
    from PySide6.QtWidgets import QApplication

    from app.config import APP_NAME, ORG_NAME
    from app.ui.main_window import MainWindow

    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    app.setOrganizationName(ORG_NAME)
    app.setStyle("Fusion")

    win = MainWindow()
    win.show()

    # Optional CLI: pass h5 paths to auto-load
    paths = [Path(a) for a in sys.argv[1:] if a.lower().endswith((".h5", ".hdf5"))]
    if paths:
        win.load_paths(paths)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
