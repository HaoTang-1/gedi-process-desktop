#pragma once
#include "data/GediLoader.h"
#include "map/MapWidget.h"
#include <QMainWindow>

namespace ui {

class FileTreePanel;
class InfoPanel;
class PropertyPanel;
class WaveformWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow();

private slots:
    void openFiles();
    void openFolder();
    void loadSample();
    void clearAll();
    void refreshMap();
    void onFootprint(const data::ShotMeta& sm);
    void applyTransform(const QString& id);
    void runMetric();
    void openCalc();
    void openBatch();
    void setOnlineBasemap(const QString& providerId);
    void switchLang(const QString& lang);
    void retranslateUi();

private:
    void buildMenus();
    void addRasterLayers();
    void updateBasemapActions(const QString& providerId);
    data::GediLoader m_loader;
    FileTreePanel* m_tree = nullptr;
    InfoPanel* m_info = nullptr;
    map::MapWidget* m_map = nullptr;
    WaveformWidget* m_wave = nullptr;
    PropertyPanel* m_props = nullptr;
    QVector<data::ShotMeta> m_points;
    QScopedPointer<data::WaveformResult> m_current;
    QList<QAction*> m_baseActions;
};

} // namespace ui
