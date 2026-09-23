#include "map/BasemapManager.h"
#include "map/Mercator.h"

#include <QPainter>
#include <QtMath>
#include <cmath>

namespace map {

Provider esriProvider()
{
    Provider p;
    p.id = QStringLiteral("esri_world_imagery");
    p.nameZh = QStringLiteral("Esri 世界影像");
    p.nameEn = QStringLiteral("Esri World Imagery");
    p.url = QStringLiteral(
        "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}");
    return p;
}

Provider providerForId(const QString& id)
{
    for (const auto& p : defaultBasemaps()) {
        if (p.id == id) {
            Provider r;
            r.id = p.id;
            r.nameZh = p.nameZh;
            r.nameEn = p.nameEn;
            r.url = p.url;
            return r;
        }
    }
    return {};
}

BasemapManager::BasemapManager(QObject* parent) : QObject(parent)
{
    connect(&m_cache, &TileCache::tileReady, this, &BasemapManager::tilesUpdated);
    connect(&m_cache, &TileCache::batchUpdated, this, &BasemapManager::tilesUpdated);
}

void BasemapManager::setOnlineBasemap(const QString& providerId)
{
    m_providerId = providerId;
    m_url.clear();
    if (!providerId.isEmpty()) {
        Provider p = providerForId(providerId);
        m_url = p.url;
        if (m_url.isEmpty())
            m_url = esriProvider().url;
        m_cache.setUrlTemplate(m_url);
    }
    emit tilesUpdated();
}

static void tileLonLat(int z, int x, int y, double& w, double& s, double& e, double& n)
{
    double mx0, my0, mx1, my1;
    tileMercBounds(z, x, y, mx0, my0, mx1, my1);
    w = mercXToLon(mx0);
    e = mercXToLon(mx1);
    s = mercYToLat(my0);
    n = mercYToLat(my1);
}

int BasemapManager::chooseZoom(double lon0, double lat0, double lon1, double lat1) const
{
    const double spanLon = std::max(1e-6, std::abs(lon1 - lon0));
    const double spanLat = std::max(1e-6, std::abs(lat1 - lat0));
    // pick z so view ~2–4 tiles on the larger dimension (degrees)
    const double span = std::max(spanLon, spanLat * 2.0); // lat tiles are taller in deg near poles
    int z = int(std::floor(std::log2(360.0 / span * 2.0)));
    z = qBound(0, z, 19);

    auto tileCount = [&](int zz) {
        const int tx0 = lonToTileX(lon0, zz);
        const int tx1 = lonToTileX(lon1, zz);
        const int ty0 = latToTileY(lat1, zz); // north → smaller tile y
        const int ty1 = latToTileY(lat0, zz);
        return (std::abs(tx1 - tx0) + 1) * (std::abs(ty1 - ty0) + 1);
    };
    for (int guard = 0; guard < 16; ++guard) {
        if (tileCount(z) <= 36 || z <= 0)
            break;
        --z;
    }
    return z;
}

// Draw one Web-Mercator tile into plate-carrée view by splitting into lat strips
// (tile rows are linear in merc Y, not in lat). This keeps GEDI lon/lat aligned.
void BasemapManager::drawTileWarped(QPainter& p, const QImage& im, int z, int tx, int ty,
                                    double lon0, double lat0, double lon1, double lat1,
                                    int widgetW, int widgetH) const
{
    double w, s, e, n;
    tileLonLat(z, tx, ty, w, s, e, n);
    const double sx = widgetW / std::max(1e-12, lon1 - lon0);
    const double sy = widgetH / std::max(1e-12, lat1 - lat0);
    const double dstX = (w - lon0) * sx;
    const double dstW = (e - w) * sx;
    if (dstW < 0.4)
        return;

    const int strips = 12;
    const int th = im.height();
    const int tw = im.width();
    for (int i = 0; i < strips; ++i) {
        const double v0 = double(i) / strips;
        const double v1 = double(i + 1) / strips;
        // merc-linear vertical fraction → lat
        double mx0, my0, mx1, my1;
        tileMercBounds(z, tx, ty, mx0, my0, mx1, my1);
        const double myTop = my1 + (my0 - my1) * v0;
        const double myBot = my1 + (my0 - my1) * v1;
        const double latTop = mercYToLat(myTop);
        const double latBot = mercYToLat(myBot);
        const double dstY = (lat1 - latTop) * sy;
        const double dstH = (latTop - latBot) * sy;
        if (dstH < 0.3)
            continue;
        const QRectF src(0, th * v0, tw, th * (v1 - v0));
        const QRectF dst(dstX, dstY, dstW, dstH);
        p.drawImage(dst, im, src);
    }
}

void BasemapManager::drawTiles(QPainter& p, double lon0, double lat0, double lon1, double lat1,
                               int widgetW, int widgetH) const
{
    if (m_providerId.isEmpty() || m_url.isEmpty() || widgetW <= 0 || widgetH <= 0)
        return;
    if (lon1 < lon0)
        std::swap(lon0, lon1);
    if (lat1 < lat0)
        std::swap(lat0, lat1);

    const int z = chooseZoom(lon0, lat0, lon1, lat1);
    const int maxIdx = (1 << z) - 1;
    int tx0 = lonToTileX(lon0, z);
    int tx1 = lonToTileX(lon1, z);
    int ty0 = latToTileY(lat1, z);
    int ty1 = latToTileY(lat0, z);
    if (tx0 > tx1)
        std::swap(tx0, tx1);
    if (ty0 > ty1)
        std::swap(ty0, ty1);
    tx0 = qBound(0, tx0, maxIdx);
    tx1 = qBound(0, tx1, maxIdx);
    ty0 = qBound(0, ty0, maxIdx);
    ty1 = qBound(0, ty1, maxIdx);
    if ((tx1 - tx0 + 1) * (ty1 - ty0 + 1) > 64)
        return;

    p.save();
    p.setOpacity(m_opacity);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            const QString url = TileCache::makeUrl(m_url, z, tx, ty);
            QImage im = m_cache.image(url);
            if (im.isNull()) {
                m_cache.request(url);
                continue;
            }
            drawTileWarped(p, im, z, tx, ty, lon0, lat0, lon1, lat1, widgetW, widgetH);
        }
    }
    p.restore();
}

} // namespace map
