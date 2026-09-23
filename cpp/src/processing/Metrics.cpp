#include "processing/Metrics.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace proc {

static double nan() { return std::numeric_limits<double>::quiet_NaN(); }

static void seriesStats(const std::vector<double>& y, QMap<QString, double>& m)
{
    std::vector<double> f;
    for (double v : y)
        if (std::isfinite(v))
            f.push_back(v);
    if (f.empty())
        return;
    double mean = std::accumulate(f.begin(), f.end(), 0.0) / f.size();
    double var = 0;
    for (double v : f)
        var += (v - mean) * (v - mean);
    var /= double(f.size());
    auto mm = std::minmax_element(f.begin(), f.end());
    m["wf_mean"] = mean;
    m["wf_std"] = std::sqrt(var);
    m["wf_min"] = *mm.first;
    m["wf_max"] = *mm.second;
    double lo = *mm.first;
    double e = 0;
    for (double v : y)
        e += std::max(0.0, v - lo);
    m["wf_energy"] = e;
    double wsum = 0, csum = 0;
    for (size_t i = 0; i < y.size(); ++i) {
        double w = std::max(0.0, y[i] - lo);
        wsum += w;
        csum += w * double(i);
    }
    m["wf_centroid"] = wsum > 0 ? csum / wsum : nan();
    // FWHM
    double peak = *mm.second;
    double half = lo + 0.5 * (peak - lo);
    int first = -1, last = -1;
    for (size_t i = 0; i < y.size(); ++i) {
        if (y[i] >= half) {
            if (first < 0)
                first = (int)i;
            last = (int)i;
        }
    }
    m["wf_fwhm"] = first >= 0 ? double(last - first + 1) : nan();
    // peaks ~ simple local maxima
    int peaks = 0;
    double span = peak - lo;
    for (size_t i = 1; i + 1 < y.size(); ++i) {
        if (y[i] > y[i - 1] && y[i] >= y[i + 1] && y[i] > lo + 0.15 * span)
            ++peaks;
    }
    m["wf_peak_count"] = peaks;
    // GCR
    size_t n = y.size();
    size_t ng = std::max<size_t>(1, n / 4);
    double ge = 0, ce = 0;
    for (size_t i = 0; i < n; ++i) {
        double w = std::max(0.0, y[i] - lo);
        if (i >= n - ng)
            ge += w;
        else
            ce += w;
    }
    m["ground_energy"] = ge;
    m["canopy_energy"] = ce;
    m["gcr"] = ce > 0 ? ge / ce : nan();
}

QMap<QString, double> computeMetrics(const data::WaveformResult& r)
{
    QMap<QString, double> m;
    m["shot_number"] = double(r.shotNumber);
    m["lon"] = r.lon;
    m["lat"] = r.lat;
    m["elev_top"] = r.elevTop;
    m["elev_bot"] = r.elevBot;
    m["sensitivity"] = r.sensitivity;
    m["quality"] = r.quality;
    m["delta_time"] = r.deltaTime;
    if (std::isfinite(r.elevTop) && std::isfinite(r.elevBot))
        m["canopy_height"] = r.elevTop - r.elevBot;
    if (r.hasWaveform()) {
        m["n_samples"] = double(r.waveform.size());
        seriesStats(r.waveform, m);
    } else if (r.hasRh()) {
        m["n_samples"] = double(r.rh.size());
        seriesStats(r.rh, m);
        auto rhAt = [&](int p) {
            if (r.rh.empty())
                return nan();
            p = std::clamp(p, 0, (int)r.rh.size() - 1);
            return r.rh[p];
        };
        m["rh25"] = rhAt(25);
        m["rh50"] = rhAt(50);
        m["rh75"] = rhAt(75);
        m["rh98"] = rhAt(98);
        m["rh100"] = rhAt(100);
    }
    if (std::isfinite(r.energyTotal))
        m["energy_total"] = r.energyTotal;
    return m;
}

} // namespace proc
