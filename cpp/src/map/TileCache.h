#pragma once
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace map {

// Async XYZ tile cache. UI never blocks on network.
class TileCache : public QObject
{
    Q_OBJECT
public:
    explicit TileCache(QObject* parent = nullptr);

    void setUrlTemplate(const QString& urlTemplate);
    QString urlTemplate() const { return m_url; }
    void clear();
    bool has(const QString& url) const;
    QImage image(const QString& url) const;
    void request(const QString& url);
    void requestTemplate(int z, int x, int y);
    int pendingCount() const { return m_pending.size(); }

    static QString makeUrl(const QString& urlTemplate, int z, int x, int y);

signals:
    void tileReady(const QString& url);
    void batchUpdated();

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_nam = nullptr;
    QString m_url;
    QHash<QString, QImage> m_cache;
    QSet<QString> m_pending;
    int m_maxCache = 700;
};

} // namespace map
