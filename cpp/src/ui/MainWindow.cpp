#include "ui/MainWindow.h"
#include "core/I18n.h"
#include "core/TransformRegistry.h"
#include "data/RasterInfo.h"
#include "map/MapWidget.h"
#include "processing/Metrics.h"
#include "processing/WaveformOps.h"
#include "ui/BatchDialog.h"
#include "ui/FileTreePanel.h"
#include "ui/Hdf5ViewerDialog.h"
#include "ui/InfoPanel.h"
#include "ui/ParamDialog.h"
#include "ui/PropertyPanel.h"
#include "ui/RasterPropsDialog.h"
#include "ui/WaveformCalculatorDialog.h"
#include "ui/WaveformWidget.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressDialog>
#include <QRandomGenerator>
#include <QSplitter>
#include <QStatusBar>
#include <QStandardPaths>
#include <QTextStream>

namespace ui {

static QString sampleDir()
{
    return data::sampleDataDir();
}

MainWindow::MainWindow()
{
    core::registerBuiltinTransforms();
    setWindowTitle(i18n::t("app_title"));
    resize(1500, 920);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* outer = new QHBoxLayout(central);
    auto* split = new QSplitter(Qt::Horizontal, central);

    auto* left = new QSplitter(Qt::Vertical, split);
    m_tree = new FileTreePanel(left);
    m_info = new InfoPanel(left);
    left->addWidget(m_tree);
    left->addWidget(m_info);
    left->setStretchFactor(0, 3);
    left->setStretchFactor(1, 2);

    auto* mid = new QSplitter(Qt::Horizontal, split);
    m_map = new map::MapWidget(mid);
    m_wave = new WaveformWidget(mid);
    mid->addWidget(m_map);
    mid->addWidget(m_wave);
    mid->setStretchFactor(0, 3);   // map larger
    mid->setStretchFactor(1, 2);

    m_props = new PropertyPanel(split);
    split->addWidget(left);
    split->addWidget(mid);
    split->addWidget(m_props);
    // center map dominant; side panels narrower
    split->setStretchFactor(0, 16);
    split->setStretchFactor(1, 68);
    split->setStretchFactor(2, 12);
    split->setSizes({220, 980, 180});
    left->setSizes({320, 220});
    mid->setSizes({640, 400});
    outer->addWidget(split);

    buildMenus();
    statusBar()->showMessage(i18n::t("status_ready"));

    connect(m_tree, &FileTreePanel::visibilityChanged, this, &MainWindow::refreshMap);
    connect(m_tree, &FileTreePanel::removeGediRequested, this, [this](int id) {
        m_loader.closeFile(id);
        m_tree->removeGedi(id);
        refreshMap();
    });
    connect(m_tree, &FileTreePanel::removeRasterRequested, this, [this](int id) {
        m_tree->removeRaster(id);
        m_map->clearLocalRaster();
        refreshMap();
    });
    connect(m_tree, &FileTreePanel::hdf5ViewRequested, this, [this](int id) {
        auto* f = m_loader.file(id);
        if (f)
            Hdf5ViewerDialog(f->path, this).exec();
    });
    connect(m_tree, &FileTreePanel::hdfviewRequested, this, [this](int id) {
        auto* f = m_loader.file(id);
        if (!f)
            return;
        QStringList cands = {
            QStringLiteral("C:/Program Files/HDF_Group/HDFView/3.3.2/HDFView.exe"),
            QStringLiteral("C:/Program Files/HDF_Group/HDFView/3.3.0/HDFView.exe"),
        };
        QString exe;
        for (const QString& c : cands)
            if (QFileInfo::exists(c)) {
                exe = c;
                break;
            }
        if (exe.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("HDFView"),
                                     QStringLiteral("未找到 HDFView。请安装官方 HDFView 后重试。"));
            return;
        }
        QProcess::startDetached(exe, {f->path});
    });
    connect(m_tree, &FileTreePanel::h5dumpRequested, this, [this](int id) {
        auto* f = m_loader.file(id);
        if (!f)
            return;
        QString tool = data::findSystemTool(QStringLiteral("h5dump"));
        if (tool.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("h5dump"),
                                     QStringLiteral("未找到 h5dump，请安装 HDF5 工具。"));
            return;
        }
        QProcess pr;
        pr.start(tool, {QStringLiteral("-n"), f->path});
        pr.waitForFinished(30000);
        QString out = QString::fromUtf8(pr.readAllStandardOutput());
        if (out.isEmpty())
            out = QString::fromUtf8(pr.readAllStandardError());
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("h5dump"));
        dlg.resize(800, 600);
        auto* lay = new QVBoxLayout(&dlg);
        auto* te = new QPlainTextEdit(&dlg);
        te->setReadOnly(true);
        te->setPlainText(out.left(200000));
        lay->addWidget(te);
        dlg.exec();
    });
    connect(m_tree, &FileTreePanel::rasterVisibilityChanged, this, [this](int, bool vis) {
        auto& bm = m_map->basemapRef();
        if (vis) {
            bm.hasLocal = true;
            bm.localVisible = true;
            if (!bm.localImage.isNull())
                m_map->setLocalRaster(bm.localImage, bm.west, bm.east, bm.south, bm.north);
        } else {
            // hide without wiping pixels — only toggle visibility
            bm.localVisible = false;
        }
        m_map->update();
    });
    connect(m_tree, &FileTreePanel::rasterZoomRequested, this, [this](int) {
        m_map->zoomToLocalRaster();
    });
    connect(m_tree, &FileTreePanel::rasterPropsRequested, this, [this](int) {
        QMessageBox::information(
            this, QStringLiteral("Raster Properties / 栅格属性"),
            QStringLiteral("导入时会尝试 gdaladdo 建金字塔（.ovr）。\n"
                           "本地图层绘制在世界影像之上。\n"
                           "详细字段请用 GDAL: gdalinfo <file>"));
    });
    connect(m_map, &map::MapWidget::footprintSelected, this, &MainWindow::onFootprint);
    connect(m_map, &map::MapWidget::cursorLonLat, this, [this](double lon, double lat) {
        statusBar()->showMessage(
            QStringLiteral("坐标 %1°E, %2°N    |    EPSG:4326 (plate carrée)    |    %3")
                .arg(lon, 0, 'f', 6)
                .arg(lat, 0, 'f', 6)
                .arg(i18n::t("status_ready")));
    });
    connect(m_props, &PropertyPanel::displayChanged, this, [this] {
        m_map->setScalarMode(m_props->scalar());
        m_map->setPointSize(m_props->pointSize());
        m_map->setShowCircles(m_props->showCircles());
        refreshMap();
    });
    connect(m_info, &InfoPanel::recomputeRequested, this, &MainWindow::runMetric);
}

void MainWindow::addRasterLayers()
{
    auto paths = QFileDialog::getOpenFileNames(this, i18n::t("act_add_raster"), sampleDir(),
                                               QStringLiteral("Images (*.png *.jpg *.bmp *.tif *.tiff)"));
    if (paths.isEmpty())
        return;
    QProgressDialog pd(QStringLiteral("加载栅格…"), QStringLiteral("取消"), 0, paths.size() * 3, this);
    pd.setWindowModality(Qt::WindowModal);
    pd.setMinimumDuration(0);
    int step = 0;
    for (const auto& p : paths) {
        pd.setLabelText(QStringLiteral("读取 %1 …").arg(QFileInfo(p).fileName()));
        pd.setValue(step++);
        QApplication::processEvents();
        QImage img = data::loadRasterPreview(p, 2048);
        pd.setLabelText(QStringLiteral("属性 %1 …").arg(QFileInfo(p).fileName()));
        pd.setValue(step++);
        QApplication::processEvents();
        auto info = data::inspectRaster(p, true);
        if (img.isNull()) {
            QMessageBox::warning(this, i18n::t("act_add_raster"),
                                 QStringLiteral("%1\n\n%2").arg(p, info.error.isEmpty()
                                                                      ? QStringLiteral("转换/解码失败")
                                                                      : info.error));
            continue;
        }
        RasterLayer L;
        L.id = m_tree->children().size() + QRandomGenerator::global()->bounded(100000) + 1;
        L.name = QFileInfo(p).fileName();
        L.path = p;
        L.isOnline = false;
        L.west = info.west;
        L.east = info.east;
        L.south = info.south;
        L.north = info.north;
        if (!(L.west < L.east && L.south <= L.north)) {
            L.west = -180;
            L.east = 180;
            L.south = -90;
            L.north = 90;
        }
        m_tree->addRaster(L);
        m_map->setLocalRaster(img, L.west, L.east, L.south, L.north);
        {
            const QString tiles = QFileInfo(p).absolutePath() + QStringLiteral("/.gedi_tiles");
            const bool haveMeta = QFileInfo::exists(tiles + QStringLiteral("/meta.json"));
            bool ok = m_map->loadLocalTiles(tiles);
            // meta.json exists → never rebuild; missing → build once
            if (!ok && !haveMeta) {
                QElapsedTimer tick;
                tick.start();
                pd.setRange(0, 100);
                pd.setLabelText(QStringLiteral("构建瓦片金字塔…"));
                QApplication::processEvents();
                const bool built = data::buildTilePyramid(p, tiles, [&](int z, int zmax, QString msg) {
                    const double frac = zmax > 0 ? double(z + 1) / (zmax + 1) : 1.0;
                    const int elapsed = int(tick.elapsed() / 1000);
                    const int est = std::max(1, int(elapsed / std::max(0.05, frac)) - elapsed);
                    pd.setLabelText(QStringLiteral("%1  剩约 %2 秒").arg(msg).arg(est));
                    pd.setValue(std::min(95, int(frac * 95)));
                    QApplication::processEvents();
                    return !pd.wasCanceled();
                });
                Q_UNUSED(built);
                pd.setValue(100);
                ok = m_map->loadLocalTiles(tiles);
            } else if (haveMeta) {
                // already built — instant
                pd.setLabelText(QStringLiteral("使用已有金字塔 %1").arg(QFileInfo(p).fileName()));
                QApplication::processEvents();
            }
            if (!ok && haveMeta)
                qDebug("tiles exist but load failed — using preview fallback");
        }
        m_map->zoomToExtent(L.west, L.east, L.south, L.north);
        pd.setValue(step++);
        QApplication::processEvents();
    }
    pd.setValue(paths.size() * 3);
    refreshMap();
}

void MainWindow::updateBasemapActions(const QString& providerId)
{
    for (QAction* a : m_baseActions) {
        if (!a)
            continue;
        a->setChecked(a->data().toString() == providerId);
    }
}

void MainWindow::buildMenus()
{
    m_baseActions.clear();
    auto* mFile = menuBar()->addMenu(i18n::t("menu_file"));
    mFile->addAction(i18n::t("act_open"), this, &MainWindow::openFiles, QKeySequence::Open);
    mFile->addAction(i18n::t("act_open_dir"), this, &MainWindow::openFolder);
    mFile->addAction(i18n::t("act_sample"), this, &MainWindow::loadSample);
    mFile->addSeparator();
    mFile->addAction(i18n::t("act_clear"), this, &MainWindow::clearAll);
    mFile->addSeparator();
    mFile->addAction(i18n::t("act_quit"), qApp, &QApplication::quit);

    // 图层：本地栅格 + 在线底图（扁平，不再嵌套）
    auto* mLayer = menuBar()->addMenu(i18n::t("menu_layers"));
    mLayer->addAction(i18n::t("act_add_raster"), this, &MainWindow::addRasterLayers);
    mLayer->addSeparator();
    auto* actOff = mLayer->addAction(i18n::t("act_base_off"));
    actOff->setCheckable(true);
    actOff->setChecked(true);
    actOff->setData(QString());
    auto* grp = new QActionGroup(this);
    grp->setExclusive(true);
    grp->addAction(actOff);
    m_baseActions.append(actOff);
    connect(actOff, &QAction::triggered, this, [this] { setOnlineBasemap(QString()); });
    for (const auto& pb : map::defaultBasemaps()) {
        auto* a = mLayer->addAction(pb.nameZh);
        a->setCheckable(true);
        a->setData(pb.id);
        grp->addAction(a);
        m_baseActions.append(a);
        connect(a, &QAction::triggered, this, [this, id = pb.id] { setOnlineBasemap(id); });
    }

    auto* mView = menuBar()->addMenu(i18n::t("menu_view"));
    mView->addAction(i18n::t("act_reset_zoom"), this, [this] { m_map->resetView(); });
    auto* actGrid = mView->addAction(i18n::t("act_grid"));
    actGrid->setCheckable(true);
    actGrid->setChecked(m_map->showGrid());
    connect(actGrid, &QAction::toggled, this, [this](bool on) { m_map->setShowGrid(on); });

    auto* mTr = menuBar()->addMenu(i18n::t("menu_transform"));
    for (const auto& cat : core::transforms().byCategory().keys()) {
        mTr->addSection(cat);
        for (const auto& s : core::transforms().byCategory().value(cat)) {
            auto* a = mTr->addAction(s.name);
            connect(a, &QAction::triggered, this, [this, id = s.id] { applyTransform(id); });
        }
    }

    auto* mMet = menuBar()->addMenu(i18n::t("menu_metric"));
    mMet->addAction(i18n::t("act_apply_default"), this, &MainWindow::runMetric);

    auto* mCalc = menuBar()->addMenu(i18n::t("menu_calc"));
    mCalc->addAction(i18n::t("act_wave_calc"), this, &MainWindow::openCalc);

    auto* mTools = menuBar()->addMenu(i18n::t("menu_tools"));
    mTools->addAction(i18n::t("act_find_shot"), this, [this] {
        bool ok = false;
        QString s = QInputDialog::getText(this, i18n::t("act_find_shot"),
                                          QStringLiteral("shot_number:"), QLineEdit::Normal,
                                          QString(), &ok);
        if (!ok || s.isEmpty())
            return;
        long long want = s.toLongLong();
        for (int i = 0; i < m_points.size(); ++i) {
            if (m_points[i].shotNumber == want) {
                m_map->setSelectedIndex(i);
                onFootprint(m_points[i]);
                return;
            }
        }
        QMessageBox::information(this, i18n::t("act_find_shot"), QStringLiteral("Not in current map points"));
    });
    mTools->addAction(i18n::t("act_stats"), this, [this] {
        qint64 files = m_loader.files().size();
        qint64 shots = 0;
        for (auto* f : m_loader.files())
            shots += f->nShots;
        QMessageBox::information(this, i18n::t("act_stats"),
                                 QStringLiteral("Files: %1\nShots: %2\nMap points: %3")
                                     .arg(files)
                                     .arg(shots)
                                     .arg(m_points.size()));
    });
    mTools->addSeparator();
    mTools->addAction(i18n::t("act_copy_coord"), this, [this] {
        if (!m_current) {
            QMessageBox::information(this, i18n::t("act_copy_coord"), i18n::t("msg_pick_footprint"));
            return;
        }
        QApplication::clipboard()->setText(
            QStringLiteral("%1,%2,%3").arg(m_current->lon, 0, 'f', 8).arg(m_current->lat, 0, 'f', 8).arg(m_current->shotNumber));
    });
    mTools->addAction(i18n::t("act_export_wave"), this, [this] {
        if (!m_current || (m_current->waveform.empty() && m_current->rh.empty())) {
            QMessageBox::information(this, i18n::t("act_export_wave"), i18n::t("msg_pick_footprint"));
            return;
        }
        QString path = QFileDialog::getSaveFileName(this, i18n::t("act_export_wave"), "waveform.csv",
                                                    QStringLiteral("CSV (*.csv)"));
        if (path.isEmpty())
            return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return;
        QTextStream ts(&f);
        ts << "index,value\n";
        const std::vector<double>& y = m_current->hasWaveform() ? m_current->waveform : m_current->rh;
        for (size_t i = 0; i < y.size(); ++i)
            ts << int(i) << "," << y[i] << "\n";
    });
    mTools->addAction(i18n::t("act_export_metrics"), this, [this] {
        if (!m_current)
            return;
        QString path = QFileDialog::getSaveFileName(this, i18n::t("act_export_metrics"), "metrics.csv",
                                                    QStringLiteral("CSV (*.csv)"));
        if (path.isEmpty())
            return;
        auto met = proc::computeMetrics(*m_current);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return;
        QTextStream ts(&f);
        ts << "key,value\n";
        for (auto it = met.constBegin(); it != met.constEnd(); ++it)
            ts << it.key() << "," << it.value() << "\n";
    });
    mTools->addAction(i18n::t("act_save_map"), this, [this] {
        QString path = QFileDialog::getSaveFileName(this, i18n::t("act_save_map"), "map.png",
                                                    QStringLiteral("PNG (*.png)"));
        if (!path.isEmpty())
            m_map->saveMapImage(path);
    });

    auto* mBatch = menuBar()->addMenu(i18n::t("menu_batch"));
    mBatch->addAction(i18n::t("act_batch"), this, &MainWindow::openBatch);

    auto* mHelp = menuBar()->addMenu(i18n::t("menu_help"));
    auto* mLang = mHelp->addMenu(i18n::t("menu_language"));
    mLang->addAction(QStringLiteral("中文"), this, [this] { switchLang(QStringLiteral("zh")); });
    mLang->addAction(QStringLiteral("English"), this, [this] { switchLang(QStringLiteral("en")); });
    mHelp->addAction(i18n::t("act_about"), this, [this] {
        QMessageBox::about(
            this, i18n::t("act_about"),
            QStringLiteral(
                "GEDI Process Desktop C++ / Qt\n"
                "EPSG:4326 (plate carrée)\n"
                "\n"
                "联系 / Contact\n"
                "Email: tangh@std.uestc.edu.cn\n"
                "\n"
                "MIT License"));
    });
}

void MainWindow::switchLang(const QString& lang)
{
    i18n::setLangFromName(lang);
    retranslateUi();
}

void MainWindow::retranslateUi()
{
    setWindowTitle(i18n::t("app_title"));
    menuBar()->clear();
    buildMenus();
    m_tree->retranslate();
    m_info->retranslate();
    m_props->retranslate();
    m_wave->clearMessage(i18n::t("hint_wave"));
    refreshMap();
}

void MainWindow::openFiles()
{
    auto paths = QFileDialog::getOpenFileNames(this, i18n::t("act_open"), sampleDir(),
                                               QStringLiteral("GEDI (*.h5 *.hdf5);;All (*)"));
    QProgressDialog pd(i18n::t("act_open"), QString(), 0, paths.size(), this);
    pd.setWindowModality(Qt::WindowModal);
    int i = 0;
    for (const auto& p : paths) {
        pd.setLabelText(QFileInfo(p).fileName());
        pd.setValue(i++);
        QApplication::processEvents();
        auto* f = m_loader.openFile(p);
        if (f)
            m_tree->addGedi(*f);
    }
    pd.setValue(paths.size());
    refreshMap();
}

void MainWindow::openFolder()
{
    auto dir = QFileDialog::getExistingDirectory(this, i18n::t("act_open_dir"), sampleDir());
    if (dir.isEmpty())
        return;
    QStringList paths;
    for (const auto& fi : QDir(dir).entryInfoList({QStringLiteral("*.h5"), QStringLiteral("*.hdf5")}))
        paths << fi.absoluteFilePath();
    QProgressDialog pd(i18n::t("act_open_dir"), QString(), 0, paths.size(), this);
    int i = 0;
    for (const auto& p : paths) {
        pd.setValue(i++);
        QApplication::processEvents();
        auto* f = m_loader.openFile(p);
        if (f)
            m_tree->addGedi(*f);
    }
    pd.setValue(paths.size());
    refreshMap();
}

void MainWindow::loadSample() { openFolder(); /* user picks data_sample */ }

void MainWindow::clearAll()
{
    m_loader.closeAll();
    m_tree->clearAll();
    m_points.clear();
    m_map->setPoints({});
    m_wave->clearMessage(i18n::t("hint_wave"));
}

void MainWindow::refreshMap()
{
    QList<int> ids;
    for (int id : m_tree->checkedGediIds())
        ids << id;
    m_points = m_loader.collectMapPoints(ids, m_props->beamFilter(), 24000, m_props->qualityOnly());
    m_map->setScalarMode(m_props->scalar());
    m_map->setPointSize(m_props->pointSize());
    m_map->setPoints(m_points);
    statusBar()->showMessage(QStringLiteral("%1: %2 pts").arg(i18n::t("tb_map_base_off")).arg(m_points.size()));
}

void MainWindow::onFootprint(const data::ShotMeta& sm)
{
    QProgressDialog pd(QStringLiteral("Loading waveform…"), QString(), 0, 3, this);
    pd.setWindowModality(Qt::WindowModal);
    pd.setValue(0);
    QApplication::processEvents();
    m_current.reset(m_loader.loadWaveform(sm.fileId, sm.beam, sm.index));
    pd.setValue(1);
    if (!m_current)
        return;
    m_wave->showResult(*m_current);
    pd.setValue(2);
    auto met = proc::computeMetrics(*m_current);
    m_info->showMetrics(*m_current, met);
    pd.setValue(3);
    statusBar()->showMessage(QStringLiteral("%1 %2 %3")
                                 .arg(data::productName(m_current->product), m_current->beam)
                                 .arg(m_current->shotNumber));
}

void MainWindow::applyTransform(const QString& id)
{
    if (!m_current) {
        QMessageBox::information(this, QStringLiteral("Transform"), i18n::t("msg_pick_footprint"));
        return;
    }
    auto* sp = core::transforms().get(id);
    if (!sp)
        return;
    QVariantMap params;
    if (!sp->params.isEmpty()) {
        ParamDialog dlg(sp->name, sp->description, sp->params, this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        params = dlg.values();
    }
    const std::vector<double>& src = m_current->hasWaveform() ? m_current->waveform : m_current->rh;
    auto r = sp->fn(src, params);
    m_wave->showProcessed(*m_current, r.y, r.label);
    if (r.kind == core::ResultKind::Spectrum && !r.auxY.empty()) {
        m_wave->showSpectrum(r.auxX, r.auxY, r.auxLabel.isEmpty() ? QStringLiteral("FFT") : r.auxLabel);
        statusBar()->showMessage(r.auxLabel + QStringLiteral(" bins=%1").arg(r.auxY.size()));
    } else {
        m_wave->showSpectrum({}, {}, QString());
    }
}

void MainWindow::runMetric()
{
    if (!m_current)
        return;
    m_info->showMetrics(*m_current, proc::computeMetrics(*m_current));
}

void MainWindow::openCalc()
{
    WaveformCalculatorDialog dlg(&m_loader, m_current.get(), this);
    dlg.exec();
}

void MainWindow::openBatch()
{
    BatchDialog dlg(&m_loader, this);
    dlg.exec();
}

void MainWindow::setOnlineBasemap(const QString& providerId)
{
    m_map->setOnlineBasemap(providerId);
    updateBasemapActions(providerId);
    statusBar()->showMessage(providerId.isEmpty() ? i18n::t("act_base_off")
                                                  : i18n::t("tb_map_base_on"));
}

} // namespace ui
