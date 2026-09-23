#include "processing/Calculator.h"
#include "processing/Metrics.h"
#include "processing/WaveformOps.h"

#include <cmath>

namespace proc {

QString ProcessChain::summary() const
{
    if (steps.isEmpty())
        return QStringLiteral("(empty)");
    QStringList parts;
    for (const auto& s : steps) {
        auto* sp = core::transforms().get(s.id);
        parts << (sp ? sp->name : s.id);
    }
    return parts.join(QStringLiteral(" → "));
}

ChainOut runChain(const std::vector<double>& y, const ProcessChain& chain)
{
    ChainOut out;
    out.y = y;
    QStringList labels;
    for (const auto& st : chain.steps) {
        auto* sp = core::transforms().get(st.id);
        if (!sp || !sp->fn)
            continue;
        auto r = sp->fn(out.y, st.params);
        out.y = r.y;
        labels << r.label;
        if (r.kind == core::ResultKind::Spectrum) {
            out.hasSpectrum = true;
            out.auxX = r.auxX;
            out.auxY = r.auxY;
            out.auxLabel = r.auxLabel;
        }
    }
    out.label = labels.isEmpty() ? chain.summary() : labels.join(QStringLiteral(" → "));
    return out;
}

BatchResult runBatch(const data::GediLoader& loader, const QList<int>& fileIds,
                     const ProcessChain& chain, bool qualityOnly, int stride, int maxPerBeam,
                     const std::function<void(int, int, QString)>& progress)
{
    BatchResult res;
    struct Job {
        int fid;
        QString fname, beam;
        int idx;
    };
    QVector<Job> jobs;
    for (int fid : fileIds) {
        auto* gf = loader.file(fid);
        if (!gf)
            continue;
        for (auto it = gf->beams.constBegin(); it != gf->beams.constEnd(); ++it) {
            const auto& b = it.value();
            std::vector<int> idx;
            for (size_t i = 0; i < b.shotNumber.size(); ++i) {
                if (qualityOnly && i < b.quality.size() && b.quality[i] != 1)
                    continue;
                idx.push_back((int)i);
            }
            if (stride > 1) {
                std::vector<int> s;
                for (size_t k = 0; k < idx.size(); k += (size_t)stride)
                    s.push_back(idx[k]);
                idx.swap(s);
            }
            if (maxPerBeam > 0 && (int)idx.size() > maxPerBeam)
                idx.resize(maxPerBeam);
            for (int i : idx)
                jobs.push_back({fid, gf->path.section('/', -1), it.key(), i});
        }
    }
    int total = jobs.size();
    int k = 0;
    for (const auto& job : jobs) {
        std::unique_ptr<data::WaveformResult> wr(loader.loadWaveform(job.fid, job.beam, job.idx));
        if (!wr) {
            ++res.nSkip;
            continue;
        }
        const std::vector<double>* src = wr->hasWaveform() ? &wr->waveform
                                                           : (wr->hasRh() ? &wr->rh : nullptr);
        if (!src) {
            ++res.nSkip;
            continue;
        }
        auto out = runChain(*src, chain);
        BatchRow row;
        row.file = job.fname;
        row.beam = job.beam;
        row.product = data::productName(wr->product);
        row.shot = double(wr->shotNumber);
        row.lon = wr->lon;
        row.lat = wr->lat;
        if (!out.y.empty()) {
            auto mm = std::minmax_element(out.y.begin(), out.y.end());
            row.outMin = *mm.first;
            row.outMax = *mm.second;
            double mean = 0;
            for (double v : out.y)
                mean += v;
            mean /= out.y.size();
            row.outMean = mean;
            double var = 0, e0 = *mm.first, e = 0;
            for (double v : out.y) {
                var += (v - mean) * (v - mean);
                e += std::max(0.0, v - e0);
            }
            row.outStd = std::sqrt(var / out.y.size());
            row.outEnergy = e;
        }
        row.metrics = computeMetrics(*wr);
        res.rows.push_back(row);
        ++res.nOk;
        ++k;
        if (progress && (k % 10 == 0 || k == total))
            progress(k, total, QStringLiteral("%1 %2 #%3").arg(job.fname, job.beam).arg(job.idx));
    }
    return res;
}

} // namespace proc
