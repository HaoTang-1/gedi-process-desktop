#pragma once
#include <QHash>
#include <QImage>
#include <QString>
#include <QStringList>

namespace map {

// Disk-backed tile pyramid (tools/build_tile_pyramid.py: z/x/y.png + meta.json)
// Source raster is EPSG:4326 plate carrée. Drawn with lon/lat linear transform.
// Never rebuild if meta.json exists.
class ImageTilePyramid
{
public:
    void setImage(const QImage& img, double west, double east, double south, double north);
    bool setTileDirectory(const QString& dir);
    void clear();
    void clearTiles();
    bool hasDiskTiles() const { return !m_tileDir.isEmpty(); }
    bool isValid() const { return !m_tileDir.isEmpty() || !m_full.isNull(); }

    // View is EPSG:4326 degrees
    void drawInView(class QPainter& p, double lon0, double lat0, double lon1, double lat1,
                    int widgetW, int widgetH) const;

    double west() const { return m_west; }
    double east() const { return m_east; }
    double south() const { return m_south; }
    double north() const { return m_north; }
    int zmax() const { return m_zmax; }

private:
    QImage loadDiskTile(int z, int tx, int ty) const;
    QImage tile(int z, int tx, int ty) const;
    int chooseZ(double lon0, double lat0, double lon1, double lat1) const;

    QImage m_full;
    QString m_tileDir;
    int m_zmax = 0;
    int m_imgW = 0, m_imgH = 0;
    double m_west = 0, m_east = 0, m_south = 0, m_north = 0;
    mutable QHash<QString, QImage> m_cache;
    mutable QStringList m_order;
    static constexpr int kTile = 256;
    static constexpr int kCacheMax = 64;
};

} // namespace map
