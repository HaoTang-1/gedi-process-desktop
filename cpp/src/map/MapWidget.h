#pragma once
#include "data/GediLoader.h"
#include "map/BasemapManager.h"
#include "map/ImageTilePyramid.h"

#include <QColor>
#include <QPointF>
#include <QPixmap>
#include <QRectF>
#include <QTimer>
#include <QWidget>
#include <QVector>

namespace map {

// Display space = EPSG:4326 plate carrée (like QGIS): lon/lat linear, world is 2:1.
// Online XYZ tiles (Web Mercator) are warped per lat-strip so they align with GEDI lon/lat.
class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(QWidget* parent = nullptr);

    void setPoints(const QVector<data::ShotMeta>& pts);
    void setScalarMode(const QString& mode);
    void setPointSize(int s);
    void setShowCircles(bool v);
    void setShowGrid(bool v);
    bool showGrid() const { return m_showGrid; }

    void setOnlineBasemap(const QString& providerId);
    QString onlineBasemap() const { return m_base.providerId(); }
    void refreshBasemap();
    void scheduleBasemap();

    void resetView();
    void setTitle(const QString& t) { m_title = t; update(); }
    void setLocalRaster(const QImage& img, double w, double e, double s, double n);
    bool loadLocalTiles(const QString& dir);
    void clearLocalRaster();
    void zoomToExtent(double w, double e, double s, double n);
    void zoomToLocalRaster();
    map::BasemapManager& basemapRef() { return m_base; }

    bool saveMapImage(const QString& path);
    QVector<data::ShotMeta> points() const { return m_pts; }
    void setSelectedIndex(int idx);

signals:
    void footprintSelected(data::ShotMeta sm);
    void cursorLonLat(double lon, double lat);

protected:
    void paintEvent(QPaintEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    // unified geo↔pixel in EPSG:4326. ALL drawing and picking use these.
    QPointF geoToPix(double lon, double lat) const;
    bool pixToGeo(const QPointF& p, double& lon, double& lat) const;

    void clampView();
    void fitExtent(double lon0, double lon1, double lat0, double lat1);
    QColor colorFor(const data::ShotMeta& sm) const;
    void drawScaleBar(QPainter& p);
    void drawLegend(QPainter& p);
    void drawGrid(QPainter& p);
    void drawBackground(QPainter& p);
    void grabBackground();
    void scheduleRefine();

    QPoint m_pressPos;
    bool m_dragMoved = false;

    QVector<data::ShotMeta> m_pts;
    map::BasemapManager m_base;
    map::ImageTilePyramid m_localTiles;
    QString m_scalar = QStringLiteral("beam");
    int m_pointSize = 5;
    bool m_circles = true;
    bool m_showGrid = true;
    QString m_title;

    // view + data extent in degrees (EPSG:4326)
    double m_x0 = -180, m_x1 = 180, m_y0 = -90, m_y1 = 90;
    double m_dx0 = -180, m_dx1 = 180, m_dy0 = -90, m_dy1 = 90;

    bool m_dragging = false;
    bool m_interacting = false;
    QPoint m_lastPos;

    int m_selIdx = -1;
    QPixmap m_bgGrab;
    QRectF m_bgGrabGeo; // lon/lat bounds of grab (x0,y0,x1,y1)
    bool m_bgGrabValid = false;
    QTimer m_baseTimer;
    QTimer m_refineTimer;
};

} // namespace map
