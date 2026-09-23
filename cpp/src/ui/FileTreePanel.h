#pragma once
#include "data/GediLoader.h"
#include <QMap>
#include <QSet>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

namespace ui {

struct RasterLayer {
    int id = 0;
    QString name, path, providerId;
    bool visible = true;
    bool isOnline = false;
    double west = 0, east = 0, south = 0, north = 0;
};

class FileTreePanel : public QWidget
{
    Q_OBJECT
public:
    explicit FileTreePanel(QWidget* parent = nullptr);
    void retranslate();
    void addGedi(const data::GediFile& f);
    void removeGedi(int fileId);
    void clearAll();
    void addRaster(const RasterLayer& layer);
    void removeRaster(int id);
    QSet<int> checkedGediIds() const;
    QList<int> checkedRasterIds() const;
    void setAllChecked(bool on);

signals:
    void visibilityChanged();
    void gediSelected(int fileId);
    void rasterSelected(int id);
    void removeGediRequested(int fileId);
    void removeRasterRequested(int id);
    void hdf5ViewRequested(int fileId);
    void hdfviewRequested(int fileId);
    void h5dumpRequested(int fileId);
    void rasterPropsRequested(int id);
    void rasterZoomRequested(int id);
    void rasterVisibilityChanged(int id, bool visible);

private:
    void onContextMenu(const QPoint& pos);
    QTreeWidgetItem* catL1b = nullptr;
    QTreeWidgetItem* catL2a = nullptr;
    QTreeWidgetItem* catRaster = nullptr;
    QTreeWidget* m_tree = nullptr;
    QLabel* m_hint = nullptr;
    QMap<int, data::GediFile> m_files;
    QMap<int, RasterLayer> m_rasters;
};

} // namespace ui
