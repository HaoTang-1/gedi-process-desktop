#include "map/ImageTilePyramid.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace map {

void ImageTilePyramid::setImage(const QImage& img, double west, double east,
                                double south, double north)
{
    m_full = img;
    m_west = west;
    m_east = east;
    m_south = south;
    m_north = north;
    m_imgW = img.width();
    m_imgH = img.height();
    if (m_zmax <= 0 && m_imgW > 0) {
        const int maxDim = std::max(m_imgW, m_imgH);
        m_zmax = std::clamp(int(std::ceil(std::log2(std::max(maxDim / 256.0, 1.0)))), 0, 6);
    }
    m_cache.clear();
    m_order.clear();
}

bool ImageTilePyramid::setTileDirectory(const QString& dir)
{
    const QString metaPath = dir + QStringLiteral("/meta.json");
    if (!QFileInfo::exists(metaPath))
        return false;
    QFile f(metaPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    m_west = o.value(QStringLiteral("west")).toDouble();
    m_east = o.value(QStringLiteral("east")).toDouble();
    m_south = o.value(QStringLiteral("south")).toDouble();
    m_north = o.value(QStringLiteral("north")).toDouble();
    m_zmax = o.value(QStringLiteral("zmax")).toInt(6);
    m_imgW = o.value(QStringLiteral("width")).toInt(21600);
    m_imgH = o.value(QStringLiteral("height")).toInt(10800);
    m_tileDir = dir;
    m_cache.clear();
    m_order.clear();
    return true;
}

void ImageTilePyramid::clearTiles()
{
    m_tileDir.clear();
    m_cache.clear();
    m_order.clear();
}

void ImageTilePyramid::clear()
{
    m_full = QImage();
    clearTiles();
    m_imgW = m_imgH = 0;
    m_zmax = 0;
}

QImage ImageTilePyramid::loadDiskTile(int z, int tx, int ty) const
{
    const QString key = QStringLiteral("%1/%2/%3").arg(z).arg(tx).arg(ty);
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it.value();
    const QString path = QStringLiteral("%1/%2/%3/%4.png").arg(m_tileDir).arg(z).arg(tx).arg(ty);
    QImage img;
    if (!img.load(path) || img.isNull())
        return QImage();
    m_cache.insert(key, img);
    m_order.append(key);
    while (m_cache.size() > kCacheMax && !m_order.isEmpty()) {
        const QString old = m_order.takeFirst();
        m_cache.remove(old);
    }
    return img;
}

QImage ImageTilePyramid::tile(int z, int tx, int ty) const
{
    if (!m_tileDir.isEmpty()) {
        const QImage t = loadDiskTile(z, tx, ty);
        if (!t.isNull())
            return t;
    }
    if (m_full.isNull() || m_imgW <= 0 || m_imgH <= 0)
        return QImage();
    const int decim = 1 << std::max(0, m_zmax - z);
    const int lw = std::max(1, m_imgW / decim);
    const int lh = std::max(1, m_imgH / decim);
    const int nx = (lw + kTile - 1) / kTile;
    const int ny = (lh + kTile - 1) / kTile;
    if (tx < 0 || ty < 0 || tx >= nx || ty >= ny)
        return QImage();
    QImage level = m_full.scaled(lw, lh, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    const int sx = tx * kTile;
    const int sy = ty * kTile;
    const int tw = std::min(kTile, lw - sx);
    const int th = std::min(kTile, lh - sy);
    QImage out(tw, th, QImage::Format_RGB32);
    QPainter p(&out);
    p.drawImage(0, 0, level, sx, sy, tw, th);
    return out;
}

int ImageTilePyramid::chooseZ(double lon0, double lat0, double lon1, double lat1) const
{
    const double iLon = std::max(1e-12, m_east - m_west);
    const double iLat = std::max(1e-12, m_north - m_south);
    const double visLon = std::max(1e-12, lon1 - lon0);
    const double visLat = std::max(1e-12, lat1 - lat0);

    int z = 0;
    for (int zz = 0; zz <= m_zmax; ++zz) {
        const int lw = std::max(1, m_imgW >> (m_zmax - zz));
        const int lh = std::max(1, m_imgH >> (m_zmax - zz));
        const double tLon = iLon * (kTile / std::max(1.0, double(lw)));
        const double tLat = iLat * (kTile / std::max(1.0, double(lh)));
        const int ntx = int(std::ceil(visLon / std::max(tLon, 1e-12))) + 1;
        const int nty = int(std::ceil(visLat / std::max(tLat, 1e-12))) + 1;
        if (ntx * nty > 36)
            break;
        z = zz;
    }
    {
        const int lw = std::max(1, m_imgW >> (m_zmax - z));
        const double tLon = iLon * (kTile / std::max(1.0, double(lw)));
        const double ntx = visLon / std::max(tLon, 1e-12);
        if (ntx <= 1.2)
            z = m_zmax;
        else if (ntx <= 3.0 && z < m_zmax)
            z = std::min(m_zmax, z + 2);
    }
    return z;
}

void ImageTilePyramid::drawInView(QPainter& p, double lon0, double lat0, double lon1, double lat1,
                                  int widgetW, int widgetH) const
{
    if (!isValid() || widgetW <= 0 || widgetH <= 0)
        return;
    if (lon1 < lon0)
        std::swap(lon0, lon1);
    if (lat1 < lat0)
        std::swap(lat0, lat1);

    const int z = chooseZ(lon0, lat0, lon1, lat1);
    const double iLon = std::max(1e-12, m_east - m_west);
    const double iLat = std::max(1e-12, m_north - m_south);
    const int lw = std::max(1, m_imgW >> (m_zmax - z));
    const int lh = std::max(1, m_imgH >> (m_zmax - z));
    const int nx = (lw + kTile - 1) / kTile;
    const int ny = (lh + kTile - 1) / kTile;
    const double tLon = iLon * (double(kTile) / double(lw));
    const double tLat = iLat * (double(kTile) / double(lh));

    int tx0 = int(std::floor((lon0 - m_west) / tLon));
    int tx1 = int(std::floor((lon1 - m_west) / tLon));
    int ty0 = int(std::floor((m_north - lat1) / tLat));
    int ty1 = int(std::floor((m_north - lat0) / tLat));
    tx0 = std::clamp(tx0 - 1, 0, nx - 1);
    tx1 = std::clamp(tx1 + 1, 0, nx - 1);
    ty0 = std::clamp(ty0 - 1, 0, ny - 1);
    ty1 = std::clamp(ty1 + 1, 0, ny - 1);

    const double sx = widgetW / std::max(1e-12, lon1 - lon0);
    const double sy = widgetH / std::max(1e-12, lat1 - lat0);

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            const QImage t = tile(z, tx, ty);
            if (t.isNull() || t.width() <= 0 || t.height() <= 0)
                continue;
            const double fracW = t.width() / double(kTile);
            const double fracH = t.height() / double(kTile);
            const double wLon = m_west + tx * tLon;
            const double nLat = m_north - ty * tLat;
            const double eLon = wLon + tLon * fracW;
            const double sLat = nLat - tLat * fracH;
            const QRectF dst((wLon - lon0) * sx, (lat1 - nLat) * sy,
                             (eLon - wLon) * sx, (nLat - sLat) * sy);
            if (dst.width() < 0.4 || dst.height() < 0.4)
                continue;
            p.drawImage(dst, t, QRectF(0, 0, t.width(), t.height()));
        }
    }
    p.restore();
}

} // namespace map
