#pragma once
#include "map/TileCache.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <vector>

namespace map {

struct Provider {
    QString id, nameZh, nameEn, url;
};

struct ProviderInfo {
    QString id, nameZh, nameEn, source, url, attribution, crs;
};

// Online XYZ sources — QGIS-style list
inline const std::vector<ProviderInfo>& defaultBasemaps()
{
    static const std::vector<ProviderInfo> v = {
        {"esri_world_imagery", "Esri 世界影像", "Esri World Imagery", "tiles",
         "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
         "Esri", "epsg3857"},
        {"esri_shaded_relief", "Esri 山体阴影", "Esri Shaded Relief", "tiles",
         "https://server.arcgisonline.com/ArcGIS/rest/services/World_Shaded_Relief/MapServer/tile/{z}/{y}/{x}",
         "Esri", "epsg3857"},
        {"gibs_blue_marble", "NASA Blue Marble", "NASA Blue Marble", "tiles",
         "https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/BlueMarble_ShadedRelief_Bathymetry/default/500m/{z}/{y}/{x}.jpeg",
         "NASA", "epsg3857"},
        {"osm", "OpenStreetMap", "OpenStreetMap", "tiles",
         "https://tile.openstreetmap.org/{z}/{x}/{y}.png", "OSM", "epsg3857"},
    };
    return v;
}

Provider providerForId(const QString& id);
Provider esriProvider();

// Online basemap drawn into EPSG:4326 (plate carrée) view with per-tile lat warp.
class BasemapManager : public QObject
{
    Q_OBJECT
public:
    explicit BasemapManager(QObject* parent = nullptr);

    void setOnlineBasemap(const QString& providerId);
    bool isEnabled() const { return !m_providerId.isEmpty(); }
    QString providerId() const { return m_providerId; }
    void setOpacity(double o) { m_opacity = o; }
    double opacity() const { return m_opacity; }
    int pendingTiles() const { return m_cache.pendingCount(); }

    // lon0/lon1/lat0/lat1 = view in degrees (EPSG:4326)
    void drawTiles(class QPainter& p, double lon0, double lat0, double lon1, double lat1,
                   int widgetW, int widgetH) const;

    bool hasLocal = false;
    bool localVisible = true;
    QImage localImage;
    double west = 0, east = 0, south = 0, north = 0;

signals:
    void tilesUpdated();

private:
    int chooseZoom(double lon0, double lat0, double lon1, double lat1) const;
    void drawTileWarped(class QPainter& p, const QImage& im, int z, int tx, int ty,
                        double lon0, double lat0, double lon1, double lat1,
                        int widgetW, int widgetH) const;

    mutable TileCache m_cache;
    QString m_providerId;
    QString m_url;
    double m_opacity = 0.95;
};

} // namespace map
