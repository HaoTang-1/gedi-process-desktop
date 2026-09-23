#include "data/GediLoader.h"
#include "data/H5Dyn.h"

#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace data {

QString productName(Product p)
{
    switch (p) {
    case Product::L1B:
        return QStringLiteral("L1B");
    case Product::L2A:
        return QStringLiteral("L2A");
    default:
        return QStringLiteral("UNKNOWN");
    }
}

QString GediFile::displayName() const
{
    return QStringLiteral("[%1] %2").arg(productName(product), QFileInfo(path).fileName());
}

Product GediLoader::detectProduct(const QString& path)
{
    const QString n = QFileInfo(path).fileName().toUpper();
    if (n.contains("GEDI01_B") || n.contains("L1B"))
        return Product::L1B;
    if (n.contains("GEDI02_A") || n.contains("L2A"))
        return Product::L2A;
    h5::hid_t f = 0;
    if (h5::fileOpen(path, f)) {
        QString sn;
        h5::readAttrString(f, QStringLiteral("/"), QStringLiteral("short_name"), sn);
        h5::fileClose(f);
        if (sn.contains("L1B"))
            return Product::L1B;
        if (sn.contains("L2A"))
            return Product::L2A;
    }
    return Product::Unknown;
}

GediLoader::~GediLoader() { closeAll(); }

long long GediLoader::handle(int fileId) const
{
    return m_handles.value(fileId, -1);
}

GediFile* GediLoader::openFile(const QString& path)
{
    Product prod = detectProduct(path);
    if (prod == Product::Unknown)
        prod = detectProduct(path);
    if (prod == Product::Unknown) {
        // try open and guess
        if (!path.endsWith(".h5", Qt::CaseInsensitive) && !path.endsWith(".hdf5", Qt::CaseInsensitive))
            return nullptr;
        prod = Product::L2A;
    }

    h5::hid_t f0 = 0;
    if (!h5::fileOpen(path, f0))
        return nullptr;

    auto gf = std::make_shared<GediFile>();
    gf->fileId = m_nextId++;
    gf->path = path;
    gf->product = prod;
    QString sn;
    h5::readAttrString(f0, QStringLiteral("/"), QStringLiteral("short_name"), sn);
    gf->shortName = sn.isEmpty() ? productName(prod) : sn;
    if (sn.contains("L1B"))
        gf->product = Product::L1B;
    else if (sn.contains("L2A"))
        gf->product = Product::L2A;

    m_handles[gf->fileId] = (long long)f0;

    const QStringList beamIds = {
        "BEAM0000", "BEAM0001", "BEAM0010", "BEAM0011",
        "BEAM0101", "BEAM0110", "BEAM1000", "BEAM1011"};

    for (const QString& b : beamIds) {
        // existence check
        QVector<qint64> dims;
        int cls = 0, sz = 0;
        if (!h5::datasetInfo(f0, b + QStringLiteral("/shot_number"), dims, cls, sz))
            continue;
        BeamMeta bm;
        auto load = [&](const QString& p, std::vector<double>& dst) {
            QVector<qint64> d;
            h5::readAsDouble(f0, b + QLatin1Char('/') + p, dst, d);
        };
        load(QStringLiteral("shot_number"), bm.shotNumber);
        load(QStringLiteral("lon_lowestmode"), bm.lon);
        if (bm.lon.empty())
            load(QStringLiteral("geolocation/longitude_bin0"), bm.lon);
        load(QStringLiteral("lat_lowestmode"), bm.lat);
        if (bm.lat.empty())
            load(QStringLiteral("geolocation/latitude_bin0"), bm.lat);
        load(QStringLiteral("elev_highestreturn"), bm.elevTop);
        if (bm.elevTop.empty())
            load(QStringLiteral("geolocation/elevation_bin0"), bm.elevTop);
        load(QStringLiteral("elev_lowestmode"), bm.elevBot);
        if (bm.elevBot.empty())
            load(QStringLiteral("geolocation/elevation_lastbin"), bm.elevBot);
        load(QStringLiteral("sensitivity"), bm.sensitivity);
        load(QStringLiteral("degrade_flag"), bm.degrade);
        if (bm.degrade.empty())
            load(QStringLiteral("geolocation/degrade"), bm.degrade);
        load(QStringLiteral("delta_time"), bm.deltaTime);
        load(QStringLiteral("rx_sample_start_index"), bm.wfStart);
        load(QStringLiteral("rx_sample_count"), bm.wfCount);
        load(QStringLiteral("energy_total"), bm.energyTotal);
        load(QStringLiteral("selected_algorithm"), bm.selectedAlgorithm);
        if (gf->product == Product::L2A) {
            QVector<qint64> dimsRh;
            int c, s;
            if (h5::datasetInfo(f0, b + QStringLiteral("/rh"), dimsRh, c, s))
                bm.hasRh = true;
        }
        // quality
        load(QStringLiteral("l2a_quality_flag_rel3"), bm.quality);
        if (bm.quality.empty())
            load(QStringLiteral("l2a_quality_flag_rel2"), bm.quality);
        if (bm.quality.empty()) {
            // L1B invert clip flag
            std::vector<double> clip;
            load(QStringLiteral("rx_clipflag"), clip);
            bm.quality.resize(clip.size());
            for (size_t i = 0; i < clip.size(); ++i)
                bm.quality[i] = clip[i] != 0 ? 0 : 1;
        }

        // normalize wfStart to 0-based
        if (!bm.wfStart.empty()) {
            double mn = *std::min_element(bm.wfStart.begin(), bm.wfStart.end());
            if (mn >= 1.0) {
                for (auto& v : bm.wfStart)
                    v -= 1.0;
            }
        }

        size_t n = bm.shotNumber.size();
        gf->nShots += (qint64)n;
        gf->beams.insert(b, std::move(bm));
    }

    m_files.push_back(gf);
    return gf.get();
}

void GediLoader::closeFile(int fileId)
{
    if (m_handles.contains(fileId)) {
        h5::fileClose((h5::hid_t)m_handles[fileId]);
        m_handles.remove(fileId);
    }
    m_files.erase(std::remove_if(m_files.begin(), m_files.end(),
                                 [&](const std::shared_ptr<GediFile>& f) { return f->fileId == fileId; }),
                  m_files.end());
}

void GediLoader::closeAll()
{
    for (auto it = m_handles.begin(); it != m_handles.end(); ++it)
        h5::fileClose((h5::hid_t)it.value());
    m_handles.clear();
    m_files.clear();
}

QList<GediFile*> GediLoader::files() const
{
    QList<GediFile*> out;
    for (auto& f : m_files)
        out.push_back(f.get());
    return out;
}

GediFile* GediLoader::file(int id) const
{
    for (auto& f : m_files)
        if (f->fileId == id)
            return f.get();
    return nullptr;
}

static void sampleIndices(size_t n, const std::vector<double>& lon, const std::vector<double>& lat,
                          const std::vector<double>& quality, bool qualityOnly, int budget,
                          std::vector<int>& idx)
{
    idx.clear();
    std::vector<int> valid;
    valid.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (i < lon.size() && i < lat.size() && std::isfinite(lon[i]) && std::isfinite(lat[i])) {
            if (qualityOnly && i < quality.size() && quality[i] != 1)
                continue;
            valid.push_back((int)i);
        }
    }
    if (valid.empty())
        return;
    budget = std::max(budget, 1);
    if ((int)valid.size() <= budget) {
        idx = valid;
        return;
    }
    idx.reserve(budget);
    double step = double(valid.size()) / double(budget);
    for (int k = 0; k < budget; ++k) {
        int j = int(k * step);
        if (j >= (int)valid.size())
            j = (int)valid.size() - 1;
        idx.push_back(valid[j]);
    }
    std::sort(idx.begin(), idx.end());
    idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
}

QVector<ShotMeta> GediLoader::collectMapPoints(const QList<int>& fileIds, const QString& beam,
                                               int maxPoints, bool qualityOnly) const
{
    QVector<ShotMeta> out;
    struct Job {
        int fid;
        QString beam;
        const BeamMeta* bm;
        const GediFile* gf;
    };
    QList<Job> jobs;
    for (int fid : fileIds) {
        auto* gf = file(fid);
        if (!gf)
            continue;
        for (auto it = gf->beams.constBegin(); it != gf->beams.constEnd(); ++it) {
            if (!beam.isEmpty() && beam != QLatin1String("ALL") && it.key() != beam)
                continue;
            jobs.push_back({fid, it.key(), &it.value(), gf});
        }
    }
    if (jobs.isEmpty())
        return out;
    int budget = std::max(80, maxPoints / (int)jobs.size());
    out.reserve(jobs.size() * budget);
    for (const auto& job : jobs) {
        const BeamMeta& b = *job.bm;
        std::vector<int> idx;
        sampleIndices(b.shotNumber.size(), b.lon, b.lat, b.quality, qualityOnly, budget, idx);
        for (int i : idx) {
            ShotMeta sm;
            sm.fileId = job.fid;
            sm.beam = job.beam;
            sm.index = i;
            sm.shotNumber = (long long)b.shotNumber[i];
            sm.lon = b.lon[i];
            sm.lat = b.lat[i];
            if (i < (int)b.elevTop.size())
                sm.elevTop = b.elevTop[i];
            if (i < (int)b.elevBot.size())
                sm.elevBot = b.elevBot[i];
            if (i < (int)b.sensitivity.size())
                sm.sensitivity = b.sensitivity[i];
            if (i < (int)b.quality.size())
                sm.quality = (int)b.quality[i];
            if (i < (int)b.degrade.size())
                sm.degrade = (int)b.degrade[i];
            if (i < (int)b.deltaTime.size())
                sm.deltaTime = b.deltaTime[i];
            if (i < (int)b.wfCount.size())
                sm.nSamples = (int)b.wfCount[i];
            out.push_back(sm);
        }
    }
    return out;
}

WaveformResult* GediLoader::loadWaveform(int fileId, const QString& beam, int index) const
{
    auto* gf = file(fileId);
    if (!gf || !gf->beams.contains(beam))
        return nullptr;
    const BeamMeta& b = gf->beams[beam];
    if (index < 0 || index >= (int)b.shotNumber.size())
        return nullptr;
    auto* res = new WaveformResult();
    res->product = gf->product;
    res->beam = beam;
    res->fileId = fileId;
    res->shotNumber = (long long)b.shotNumber[index];
    res->lon = b.lon[index];
    res->lat = b.lat[index];
    if (index < (int)b.elevTop.size())
        res->elevTop = b.elevTop[index];
    if (index < (int)b.elevBot.size())
        res->elevBot = b.elevBot[index];
    if (index < (int)b.sensitivity.size())
        res->sensitivity = b.sensitivity[index];
    if (index < (int)b.quality.size())
        res->quality = (int)b.quality[index];
    if (index < (int)b.degrade.size())
        res->degrade = (int)b.degrade[index];
    if (index < (int)b.deltaTime.size())
        res->deltaTime = b.deltaTime[index];
    if (index < (int)b.energyTotal.size())
        res->energyTotal = b.energyTotal[index];
    if (index < (int)b.selectedAlgorithm.size())
        res->selectedAlgorithm = (int)b.selectedAlgorithm[index];

    h5::hid_t f = (h5::hid_t)handle(fileId);
    if (f <= 0)
        return res;

        if (gf->product == Product::L1B) {
        if (index < (int)b.wfStart.size() && index < (int)b.wfCount.size()) {
            long long start = (long long)b.wfStart[index];
            long long count = (long long)b.wfCount[index];
            if (count > 0) {
                QString p = beam + QStringLiteral("/rxwaveform");
                // hyperslab — do NOT read the full flattened waveform
                if (h5::readSlice1D(f, p, start, count, res->waveform) && !res->waveform.empty()) {
                    res->heights.resize(count);
                    if (std::isfinite(res->elevTop) && std::isfinite(res->elevBot)) {
                        for (long long i = 0; i < count; ++i) {
                            double u = count == 1 ? 0.0 : double(i) / double(count - 1);
                            res->heights[i] = res->elevTop + (res->elevBot - res->elevTop) * u;
                        }
                    } else {
                        for (long long i = 0; i < count; ++i)
                            res->heights[i] = double(i);
                    }
                }
            }
        }
    } else {
        // L2A rh: one row only (N x 101)
        if (b.hasRh) {
            long long ncols = 101;
            long long row0 = (long long)index * ncols;
            if (h5::readSlice1D(f, beam + QStringLiteral("/rh"), row0, ncols, res->rh) && !res->rh.empty()) {
                res->heights.resize(res->rh.size());
                for (size_t i = 0; i < res->rh.size(); ++i)
                    res->heights[i] = double(i);
            }
        }
        if (index < (int)b.energyTotal.size())
            res->energyTotal = b.energyTotal[index];
        if (index < (int)b.selectedAlgorithm.size())
            res->selectedAlgorithm = (int)b.selectedAlgorithm[index];
    }
    return res;
}

} // namespace data
