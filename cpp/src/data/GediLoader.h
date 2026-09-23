#pragma once
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace data {

enum class Product { L1B, L2A, Unknown };

QString productName(Product p);

struct ShotMeta {
    int fileId = -1;
    QString beam;
    int index = 0;
    long long shotNumber = 0;
    double lon = 0, lat = 0;
    double elevTop = std::numeric_limits<double>::quiet_NaN();
    double elevBot = std::numeric_limits<double>::quiet_NaN();
    double sensitivity = std::numeric_limits<double>::quiet_NaN();
    int quality = -1;
    int degrade = -1;
    double deltaTime = std::numeric_limits<double>::quiet_NaN();
    int nSamples = 0;
};

struct WaveformResult {
    Product product = Product::Unknown;
    QString beam;
    long long shotNumber = 0;
    double lon = 0, lat = 0;
    double elevTop = std::numeric_limits<double>::quiet_NaN();
    double elevBot = std::numeric_limits<double>::quiet_NaN();
    double sensitivity = std::numeric_limits<double>::quiet_NaN();
    int quality = -1;
    int degrade = -1;
    double deltaTime = std::numeric_limits<double>::quiet_NaN();
    int fileId = -1;
    std::vector<double> waveform;
    std::vector<double> heights;
    std::vector<double> rh;
    double energyTotal = std::numeric_limits<double>::quiet_NaN();
    int selectedAlgorithm = -1;
    bool hasWaveform() const { return !waveform.empty(); }
    bool hasRh() const { return !rh.empty(); }
};

struct BeamMeta {
    std::vector<double> shotNumber;
    std::vector<double> lon, lat;
    std::vector<double> elevTop, elevBot;
    std::vector<double> sensitivity, deltaTime;
    std::vector<double> quality, degrade;
    std::vector<double> wfStart, wfCount;
    std::vector<double> energyTotal, selectedAlgorithm;
    bool hasRh = false;
};

struct GediFile {
    int fileId = -1;
    QString path;
    Product product = Product::Unknown;
    QString shortName;
    QMap<QString, BeamMeta> beams;
    qint64 nShots = 0;
    QString displayName() const;
};

class GediLoader
{
public:
    ~GediLoader();

    GediFile* openFile(const QString& path);
    void closeFile(int fileId);
    void closeAll();
    QList<GediFile*> files() const;
    GediFile* file(int id) const;

    QVector<ShotMeta> collectMapPoints(const QList<int>& fileIds, const QString& beam,
                                       int maxPoints, bool qualityOnly) const;
    WaveformResult* loadWaveform(int fileId, const QString& beam, int index) const; // caller deletes

    static Product detectProduct(const QString& path);

private:
    QList<std::shared_ptr<GediFile>> m_files;
    int m_nextId = 0;
    mutable QMap<int, long long> m_handles; // hid_t
    long long handle(int fileId) const;
};

} // namespace data
