#include "map/MapWidget.h"
#include "core/I18n.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace map {

static const QMap<QString, QColor> kBeamColors = {
    {"BEAM0000", QColor("#0B3D91")}, {"BEAM0001", QColor("#1AA3C8")},
    {"BEAM0010", QColor("#2E7D32")}, {"BEAM0011", QColor("#C45C26")},
    {"BEAM0101", QColor("#6A1B9A")}, {"BEAM0110", QColor("#AD1457")},
    {"BEAM1000", QColor("#00838F")}, {"BEAM1011", QColor("#5D4037")},
};

// Single transform for ALL drawing and picking (EPSG:4326 plate carrée):
//   x = (lon - m_x0) / (m_x1 - m_x0) * width
//   y = (m_y1 - lat) / (m_y1 - m_y0) * height   (north = top)

MapWidget::MapWidget(QWidget* parent) : QWidget(parent), m_base(this)
{
    setMinimumSize(320, 240);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
    m_title = i18n::t("tb_map_base_off");

    m_baseTimer.setSingleShot(true);
    m_baseTimer.setInterval(80);
    connect(&m_baseTimer, &QTimer::timeout, this, [this] { update(); });

    m_refineTimer.setSingleShot(true);
    m_refineTimer.setInterval(120);
    connect(&m_refineTimer, &QTimer::timeout, this, [this] {
        m_interacting = false;
        grabBackground();
        update();
    });

    connect(&m_base, &BasemapManager::tilesUpdated, this, [this] {
        if (!m_interacting) {
            grabBackground();
            update();
        } else {
            scheduleRefine();
        }
    });
}

QPointF MapWidget::geoToPix(double lon, double lat) const
{
    const double u = (lon - m_x0) / std::max(1e-12, m_x1 - m_x0);
    const double v = (m_y1 - lat) / std::max(1e-12, m_y1 - m_y0);
    return QPointF(u * width(), v * height());
}

bool MapWidget::pixToGeo(const QPointF& p, double& lon, double& lat) const
{
    if (width() <= 0 || height() <= 0)
        return false;
    lon = m_x0 + (p.x() / width()) * (m_x1 - m_x0);
    lat = m_y1 - (p.y() / height()) * (m_y1 - m_y0);
    return true;
}

void MapWidget::setPoints(const QVector<data::ShotMeta>& pts)
{
    m_pts = pts;
    m_selIdx = -1;
    if (!pts.isEmpty()) {
        double x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30;
        for (const auto& p : pts) {
            x0 = std::min(x0, p.lon);
            x1 = std::max(x1, p.lon);
            y0 = std::min(y0, p.lat);
            y1 = std::max(y1, p.lat);
        }
        m_dx0 = x0;
        m_dx1 = x1;
        m_dy0 = y0;
        m_dy1 = y1;
        const double px = std::max(0.05, (x1 - x0) * 0.04);
        const double py = std::max(0.05, (y1 - y0) * 0.04);
        fitExtent(std::max(-180.0, x0 - px), std::min(180.0, x1 + px),
                  std::max(-90.0, y0 - py), std::min(90.0, y1 + py));
    } else {
        m_dx0 = -180;
        m_dx1 = 180;
        m_dy0 = -90;
        m_dy1 = 90;
        fitExtent(-180, 180, -90, 90);
    }
    grabBackground();
    update();
}

void MapWidget::setScalarMode(const QString& mode)
{
    m_scalar = mode;
    update();
}

void MapWidget::setPointSize(int s)
{
    m_pointSize = s;
    update();
}

void MapWidget::setShowCircles(bool v)
{
    m_circles = v;
    update();
}

void MapWidget::setShowGrid(bool v)
{
    m_showGrid = v;
    update();
}

void MapWidget::setOnlineBasemap(const QString& providerId)
{
    m_bgGrabValid = false;
    m_base.setOnlineBasemap(providerId);
    m_title = providerId.isEmpty() ? i18n::t("tb_map_base_off") : i18n::t("tb_map_base_on");
    grabBackground();
    update();
}

void MapWidget::setLocalRaster(const QImage& img, double w, double e, double s, double n)
{
    m_base.hasLocal = true;
    m_base.localVisible = true;
    m_base.localImage = img;
    m_base.west = w;
    m_base.east = e;
    m_base.south = s;
    m_base.north = n;
    m_localTiles.setImage(img, w, e, s, n);
    m_bgGrabValid = false;
    grabBackground();
    update();
}

bool MapWidget::loadLocalTiles(const QString& dir)
{
    if (m_localTiles.setTileDirectory(dir)) {
        m_base.hasLocal = true;
        m_base.localVisible = true;
        m_bgGrabValid = false;
        grabBackground();
        update();
        return true;
    }
    return false;
}

void MapWidget::clearLocalRaster()
{
    m_base.hasLocal = false;
    m_base.localVisible = false;
    m_base.localImage = QImage();
    m_localTiles.clear();
    m_bgGrabValid = false;
    grabBackground();
    update();
}

void MapWidget::zoomToExtent(double w, double e, double s, double n)
{
    if (!(w < e) || !(s <= n))
        return;
    fitExtent(w, e, s, n);
    grabBackground();
    update();
}

void MapWidget::zoomToLocalRaster()
{
    if (m_base.hasLocal)
        zoomToExtent(m_base.west, m_base.east, m_base.south, m_base.north);
}

void MapWidget::scheduleBasemap()
{
    m_baseTimer.start();
}

void MapWidget::scheduleRefine()
{
    m_interacting = true;
    m_refineTimer.start();
}

void MapWidget::refreshBasemap()
{
    m_bgGrabValid = false;
    grabBackground();
    update();
}

void MapWidget::clampView()
{
    const double minS = 0.002;
    if (m_x1 - m_x0 < minS) {
        const double cx = 0.5 * (m_x0 + m_x1);
        m_x0 = cx - minS / 2;
        m_x1 = cx + minS / 2;
    }
    if (m_y1 - m_y0 < minS) {
        const double cy = 0.5 * (m_y0 + m_y1);
        m_y0 = cy - minS / 2;
        m_y1 = cy + minS / 2;
    }
    const double padx = (m_x1 - m_x0) * 0.25;
    const double pady = (m_y1 - m_y0) * 0.25;
    if (m_x0 < -180 - padx) {
        const double d = -180 - padx - m_x0;
        m_x0 += d;
        m_x1 += d;
    }
    if (m_x1 > 180 + padx) {
        const double d = m_x1 - (180 + padx);
        m_x0 -= d;
        m_x1 -= d;
    }
    if (m_y0 < -90 - pady) {
        const double d = -90 - pady - m_y0;
        m_y0 += d;
        m_y1 += d;
    }
    if (m_y1 > 90 + pady) {
        const double d = m_y1 - (90 + pady);
        m_y0 -= d;
        m_y1 -= d;
    }
}

void MapWidget::fitExtent(double lon0, double lon1, double lat0, double lat1)
{
    double padLon = std::max(8.0, (lon1 - lon0) * 0.12);
    double padLat = std::max(4.0, (lat1 - lat0) * 0.08);
    lon0 -= padLon;
    lon1 += padLon;
    lat0 -= padLat;
    lat1 += padLat;
    if ((lon1 - lon0) > 120.0) {
        lon0 = std::min(lon0, -180.0);
        lon1 = std::max(lon1, 180.0);
    }
    if ((lat1 - lat0) > 80.0) {
        lat0 = std::max(-90.0, std::min(lat0, -90.0));
        lat1 = std::min(90.0, std::max(lat1, 90.0));
    }
    lon0 = std::max(lon0, -180.0);
    lon1 = std::min(lon1, 180.0);
    lat0 = std::max(lat0, -90.0);
    lat1 = std::min(lat1, 90.0);

    // QGIS-like plate carrée: uniform deg/px so world is 2:1 wide, never stretched
    double ex = std::max(lon1 - lon0, 0.02);
    double ey = std::max(lat1 - lat0, 0.02);
    const double W = std::max(1, width());
    const double H = std::max(1, height());
    const double s = std::max(ex / W, ey / H); // contain
    const double cx = 0.5 * (lon0 + lon1);
    const double cy = 0.5 * (lat0 + lat1);
    m_x0 = cx - s * W * 0.5;
    m_x1 = cx + s * W * 0.5;
    m_y0 = cy - s * H * 0.5;
    m_y1 = cy + s * H * 0.5;
    clampView();
}

void MapWidget::resetView()
{
    fitExtent(m_dx0, m_dx1, m_dy0, m_dy1);
    grabBackground();
    update();
}

void MapWidget::setSelectedIndex(int idx)
{
    m_selIdx = idx;
    update();
}

QColor MapWidget::colorFor(const data::ShotMeta& sm) const
{
    if (m_scalar == QLatin1String("file")) {
        static const QVector<QColor> pal = {
            QColor("#0B3D91"), QColor("#1AA3C8"), QColor("#2E7D32"), QColor("#C45C26"),
            QColor("#6A1B9A"), QColor("#AD1457"), QColor("#00838F"), QColor("#5D4037")};
        return pal.value(sm.fileId % pal.size(), pal[0]);
    }
    if (m_scalar == QLatin1String("sensitivity") && std::isfinite(sm.sensitivity)) {
        const double v = std::clamp(double(sm.sensitivity), 0.0, 1.0);
        return QColor::fromHsv(int(220 * (1.0 - v)), 170, 210);
    }
    if (m_scalar == QLatin1String("quality")) {
        const double v = (sm.quality == 1) ? 1.0 : 0.0;
        return QColor::fromHsv(int(220 * (1.0 - v)), 170, 210);
    }
    if (m_scalar == QLatin1String("canopy") && std::isfinite(sm.elevTop) && std::isfinite(sm.elevBot)) {
        const double v = std::clamp((sm.elevTop - sm.elevBot) / 40.0, 0.0, 1.0);
        return QColor::fromHsv(int(220 * (1.0 - v)), 170, 210);
    }
    return kBeamColors.value(sm.beam, QColor("#0B3D91"));
}

static QString niceDistanceLabel(double meters)
{
    if (meters >= 1000.0) {
        const double km = meters / 1000.0;
        if (km >= 100)
            return QStringLiteral("%1 km").arg(int(km + 0.5));
        if (km >= 10)
            return QStringLiteral("%1 km").arg(km, 0, 'f', 0);
        return QStringLiteral("%1 km").arg(km, 0, 'f', 1);
    }
    return QStringLiteral("%1 m").arg(int(meters + 0.5));
}

void MapWidget::drawScaleBar(QPainter& p)
{
    const double spanLon = m_x1 - m_x0;
    if (spanLon <= 0 || width() <= 0)
        return;
    const double midLat = 0.5 * (m_y0 + m_y1);
    const double mPerDeg = 111320.0 * std::max(0.01, std::cos(midLat * M_PI / 180.0));
    const double mPerPx = (spanLon / width()) * mPerDeg;
    const double targetM = (width() * 0.22) * mPerPx;
    if (!(targetM > 0) || !std::isfinite(targetM))
        return;
    const double pow10 = std::pow(10.0, std::floor(std::log10(targetM)));
    double nice = std::floor(targetM / pow10 + 0.5) * pow10;
    if (nice <= 0)
        nice = pow10;
    int barPx = int(nice / std::max(mPerPx, 1e-9));
    if (barPx < 30 || barPx > width() - 40)
        return;
    const int x = width() / 2 - barPx / 2;
    const int y = height() - 22;
    p.setPen(QPen(QColor("#1C2733"), 2));
    p.drawLine(x, y, x + barPx, y);
    p.drawLine(x, y - 4, x, y + 4);
    p.drawLine(x + barPx, y - 4, x + barPx, y + 4);
    QFont f = font();
    f.setPointSizeF(8);
    p.setFont(f);
    p.drawText(QRectF(x - 30, y + 6, barPx + 60, 16), Qt::AlignCenter, niceDistanceLabel(nice));
}

void MapWidget::drawGrid(QPainter& p)
{
    if (!m_showGrid)
        return;
    const double lon0 = m_x0, lon1 = m_x1;
    const double lat0 = m_y0, lat1 = m_y1;
    const double spanLon = lon1 - lon0;
    const double spanLat = lat1 - lat0;
    if (spanLon <= 0 || spanLat <= 0)
        return;
    auto niceStep = [](double span) {
        const double raw = span / 6.0;
        const double p = std::pow(10.0, std::floor(std::log10(std::max(raw, 1e-9))));
        const double c = raw / p;
        if (c < 2)
            return 1.0 * p;
        if (c < 5)
            return 2.0 * p;
        return 5.0 * p;
    };
    const double stepLon = niceStep(spanLon);
    const double stepLat = niceStep(spanLat);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    QFont f = font();
    f.setPointSizeF(7);
    p.setFont(f);
    QPen pen(QColor(28, 39, 51, 50));
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    for (double lon = std::ceil(lon0 / stepLon) * stepLon; lon <= lon1; lon += stepLon) {
        const QPointF a = geoToPix(lon, lat0);
        const QPointF b = geoToPix(lon, lat1);
        p.drawLine(a, b);
        p.setPen(QColor(28, 39, 51, 120));
        p.drawText(QPointF(a.x() + 3, 14),
                   QStringLiteral("%1°").arg(lon, 0, 'f', std::abs(lon) < 10 ? 1 : 0));
        p.setPen(pen);
    }
    for (double lat = std::ceil(lat0 / stepLat) * stepLat; lat <= lat1; lat += stepLat) {
        const QPointF a = geoToPix(lon0, lat);
        const QPointF b = geoToPix(lon1, lat);
        p.drawLine(a, b);
        p.setPen(QColor(28, 39, 51, 120));
        p.drawText(QRectF(2, a.y() - 8, 48, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1°").arg(lat, 0, 'f', std::abs(lat) < 10 ? 1 : 0));
        p.setPen(pen);
    }
    p.restore();
}

void MapWidget::drawBackground(QPainter& p)
{
    if (m_base.isEnabled()) {
        m_base.drawTiles(p, m_x0, m_y0, m_x1, m_y1, width(), height());
    }
    if (m_base.hasLocal && m_base.localVisible && m_localTiles.isValid()) {
        m_localTiles.drawInView(p, m_x0, m_y0, m_x1, m_y1, width(), height());
    }
}

void MapWidget::grabBackground()
{
    if (width() <= 0 || height() <= 0)
        return;
    QPixmap grab(width(), height());
    grab.fill(QColor("#D9E2EC"));
    {
        QPainter p(&grab);
        drawBackground(p);
    }
    m_bgGrab = grab;
    m_bgGrabGeo = QRectF(QPointF(m_x0, m_y0), QPointF(m_x1, m_y1));
    m_bgGrabValid = true;
}

void MapWidget::drawLegend(QPainter& p)
{
    if (m_pts.isEmpty())
        return;
    QFont lf = font();
    lf.setPointSizeF(8);
    p.setFont(lf);
    const QFontMetrics fm(lf);
    const bool isQuality = (m_scalar == QLatin1String("quality"));
    const bool isRamp = (m_scalar == QLatin1String("sensitivity")
                         || m_scalar == QLatin1String("canopy"));
    struct Row {
        QColor col;
        QString lab;
    };
    QVector<Row> rows;
    if (isQuality) {
        data::ShotMeta a, b;
        a.quality = 1;
        b.quality = 0;
        rows.append({colorFor(a), QStringLiteral("1 好 / good")});
        rows.append({colorFor(b), QStringLiteral("0 差 / bad")});
    } else if (isRamp) {
        for (int i = 0; i < 5; ++i) {
            const double v = 1.0 - i / 4.0;
            data::ShotMeta fake;
            fake.sensitivity = v;
            fake.elevTop = v * 40;
            fake.elevBot = 0;
            rows.append({colorFor(fake), QString::number(v, 'f', 2)});
        }
    } else {
        QSet<QString> seen;
        for (const auto& sm : m_pts) {
            const QString key = (m_scalar == QLatin1String("file"))
                                    ? QStringLiteral("file %1").arg(sm.fileId)
                                    : sm.beam;
            if (seen.contains(key))
                continue;
            seen.insert(key);
            rows.append({colorFor(sm), key});
            if (rows.size() >= 8)
                break;
        }
    }

    QString title = QStringLiteral("图例 / Legend · ");
    title += (m_scalar == QLatin1String("file")) ? QStringLiteral("File")
            : (m_scalar == QLatin1String("sensitivity")) ? QStringLiteral("Sensitivity")
            : (m_scalar == QLatin1String("quality")) ? QStringLiteral("Quality")
            : (m_scalar == QLatin1String("canopy")) ? QStringLiteral("ΔH")
                                                    : QStringLiteral("Beam");
    const QString cap = QStringLiteral("颜色 = 分类/属性着色");

    int textW = std::max({fm.horizontalAdvance(title), fm.horizontalAdvance(cap), 70});
    for (const auto& r : rows)
        textW = std::max(textW, fm.horizontalAdvance(r.lab) + 18);
    const int pad = 8;
    const int lineH = fm.height() + 4;
    const int swatch = 10;
    const int boxW = textW + pad * 2 + swatch;
    const int boxH = pad * 2 + lineH * 2 + 4 + rows.size() * lineH;
    int x0 = 12;
    int y0 = height() - 48 - boxH;
    if (y0 < 12)
        y0 = 12;

    p.fillRect(x0 - pad, y0 - pad, boxW, boxH, QColor(255, 255, 255, 165));

    QFont tf = lf;
    tf.setBold(true);
    p.setFont(tf);
    p.setPen(QColor("#0B3D91"));
    p.drawText(x0, y0 + fm.ascent(), title);
    p.setFont(lf);
    p.setPen(QColor("#5B6B7C"));
    p.drawText(x0, y0 + lineH + fm.ascent(), cap);
    int yi = y0 + lineH * 2 + 4;
    for (const auto& r : rows) {
        p.setPen(Qt::NoPen);
        p.setBrush(r.col);
        p.drawEllipse(x0, yi + (lineH - swatch) / 2, swatch - 2, swatch - 2);
        p.setPen(QColor("#1C2733"));
        p.drawText(x0 + swatch + 4, yi + fm.ascent(), r.lab);
        yi += lineH;
    }
}

void MapWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#D9E2EC"));

    const bool useGrab = m_interacting && m_bgGrabValid && !m_bgGrab.isNull();
    if (useGrab) {
        const QPointF a = geoToPix(m_bgGrabGeo.left(), m_bgGrabGeo.bottom()); // west,south
        const QPointF b = geoToPix(m_bgGrabGeo.right(), m_bgGrabGeo.top());  // east,north
        // bgGrabGeo is (x0,y0)-(x1,y1) = (west,south)-(east,north) in lon/lat
        const QRectF dst = QRectF(geoToPix(m_bgGrabGeo.left(), m_bgGrabGeo.top()),
                                  geoToPix(m_bgGrabGeo.right(), m_bgGrabGeo.bottom()))
                               .normalized();
        Q_UNUSED(a);
        Q_UNUSED(b);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPixmap(dst, m_bgGrab, QRectF(m_bgGrab.rect()));
    } else {
        drawBackground(p);
    }

    if (!m_pts.isEmpty()) {
        const double zoom = std::max(1e-12, m_dx1 - m_dx0) / std::max(1e-12, m_x1 - m_x0);
        const bool crisp = zoom > 2.0;
        p.setRenderHint(QPainter::Antialiasing, crisp);
        p.setPen(Qt::NoPen);
        const double r = crisp ? std::max(3.0, m_pointSize * 0.6) : std::max(1.5, m_pointSize * 0.4);
        for (const auto& sm : m_pts) {
            const QPointF pt = geoToPix(sm.lon, sm.lat);
            if (pt.x() < -8 || pt.y() < -8 || pt.x() > width() + 8 || pt.y() > height() + 8)
                continue;
            p.setBrush(colorFor(sm));
            p.drawEllipse(pt, r, r);
        }
    }

    if (m_selIdx >= 0 && m_selIdx < m_pts.size()) {
        const auto& sm = m_pts[m_selIdx];
        const QPointF pt = geoToPix(sm.lon, sm.lat);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#D97706"), 2));
        p.drawEllipse(pt, 10, 10);
    }

    drawGrid(p);
    drawLegend(p);
    drawScaleBar(p);
}

void MapWidget::wheelEvent(QWheelEvent* ev)
{
    double lon, lat;
    if (!pixToGeo(ev->position(), lon, lat))
        return;
    emit cursorLonLat(lon, lat);
    if (ev->angleDelta().y() == 0)
        return;

    // zoom anchored at cursor
    const double scale = ev->angleDelta().y() > 0 ? 0.85 : 1.18;
    const double u = ev->position().x() / std::max(1, width());
    const double v = ev->position().y() / std::max(1, height());
    double sx = std::max((m_x1 - m_x0) * scale, 0.002);
    double sy = std::max((m_y1 - m_y0) * scale, 0.002);
    m_x0 = lon - u * sx;
    m_x1 = m_x0 + sx;
    m_y1 = lat + v * sy;
    m_y0 = m_y1 - sy;
    clampView();
    scheduleRefine();
    scheduleBasemap();
    update();
}

void MapWidget::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton || ev->button() == Qt::MiddleButton) {
        m_dragging = true;
        m_dragMoved = false;
        m_lastPos = ev->pos();
        m_pressPos = ev->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent* ev)
{
    double lon, lat;
    pixToGeo(ev->pos(), lon, lat);
    emit cursorLonLat(lon, lat);
    if (!m_dragging)
        return;
    double lon0, lat0, lon1, lat1;
    pixToGeo(m_lastPos, lon0, lat0);
    pixToGeo(ev->pos(), lon1, lat1);
    m_x0 += lon0 - lon1;
    m_x1 += lon0 - lon1;
    m_y0 += lat0 - lat1;
    m_y1 += lat0 - lat1;
    m_lastPos = ev->pos();
    if ((ev->pos() - m_pressPos).manhattanLength() > 4)
        m_dragMoved = true;
    scheduleRefine();
    update();
}

void MapWidget::mouseReleaseEvent(QMouseEvent* ev)
{
    const bool wasDrag = m_dragMoved && m_dragging;
    if (m_dragging) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        clampView();
        scheduleRefine();
        scheduleBasemap();
        update();
    }
    if (ev->button() == Qt::LeftButton && !wasDrag && !m_pts.isEmpty()) {
        int best = -1;
        double bestD = 1e9;
        const QPointF clickPix = ev->position();
        for (int i = 0; i < m_pts.size(); ++i) {
            const QPointF pp = geoToPix(m_pts[i].lon, m_pts[i].lat);
            const double d = std::hypot(pp.x() - clickPix.x(), pp.y() - clickPix.y());
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
        if (best >= 0 && bestD < 12.0) {
            m_selIdx = best;
            update();
            emit footprintSelected(m_pts[best]);
        }
    }
    m_dragMoved = false;
}

void MapWidget::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    resetView();
}

bool MapWidget::saveMapImage(const QString& path)
{
    return grab().save(path);
}

} // namespace map
