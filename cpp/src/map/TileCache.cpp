#include "map/TileCache.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace map {

TileCache::TileCache(QObject* parent) : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    // IMPORTANT: use the reply argument — sender() here would be the NAM, not the reply
    connect(m_nam, &QNetworkAccessManager::finished, this, &TileCache::onReplyFinished);
}

QString TileCache::makeUrl(const QString& urlTemplate, int z, int x, int y)
{
    QString u = urlTemplate;
    u.replace(QLatin1String("{z}"), QString::number(z));
    u.replace(QLatin1String("{x}"), QString::number(x));
    u.replace(QLatin1String("{y}"), QString::number(y));
    return u;
}

void TileCache::setUrlTemplate(const QString& urlTemplate)
{
    m_url = urlTemplate;
}

void TileCache::clear()
{
    m_cache.clear();
    m_pending.clear();
}

bool TileCache::has(const QString& url) const
{
    return m_cache.contains(url);
}

QImage TileCache::image(const QString& url) const
{
    return m_cache.value(url);
}

void TileCache::requestTemplate(int z, int x, int y)
{
    if (m_url.isEmpty())
        return;
    request(makeUrl(m_url, z, x, y));
}

void TileCache::request(const QString& url)
{
    if (url.isEmpty() || m_cache.contains(url) || m_pending.contains(url))
        return;
    m_pending.insert(url);
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent",
                     "Mozilla/5.0 (compatible; GEDIProcessDesktopCpp/1.0)");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(15000);
    QNetworkReply* reply = m_nam->get(req);
    reply->setProperty("tileUrl", url);
}

void TileCache::onReplyFinished(QNetworkReply* reply)
{
    if (!reply)
        return;
    reply->deleteLater();
    const QString url = reply->property("tileUrl").toString();
    m_pending.remove(url);
    if (url.isEmpty())
        return;
    if (reply->error() != QNetworkReply::NoError) {
        emit batchUpdated();
        return;
    }
    QImage img;
    const QByteArray bytes = reply->readAll();
    if (!img.loadFromData(bytes) || img.isNull()) {
        emit batchUpdated();
        return;
    }
    m_cache.insert(url, img);
    while (m_cache.size() > m_maxCache)
        m_cache.erase(m_cache.begin());
    emit tileReady(url);
    emit batchUpdated();
}

} // namespace map
