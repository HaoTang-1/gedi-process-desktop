#include "processing/WaveformOps.h"

#include <algorithm>
#include <complex>
#include <numeric>

namespace proc {

static int oddAtLeast(int n, int m)
{
    n = std::max(n, m);
    return n % 2 ? n : n + 1;
}

void normalize01(std::vector<double>& y)
{
    if (y.empty())
        return;
    auto mm = std::minmax_element(y.begin(), y.end());
    double lo = *mm.first, hi = *mm.second;
    double span = hi - lo;
    if (!(span > 0))
        span = 1.0;
    for (auto& v : y)
        v = (v - lo) / span;
}

void baselineRemove(std::vector<double>& y, double quantile)
{
    if (y.empty())
        return;
    std::vector<double> tmp = y;
    std::sort(tmp.begin(), tmp.end());
    double q = std::clamp(quantile / 100.0, 0.0, 1.0);
    size_t k = size_t(q * (tmp.size() - 1));
    double floorV = tmp[k];
    for (auto& v : y)
        v -= floorV;
}

void movingAverage(std::vector<double>& y, int window)
{
    if (window <= 1 || y.size() < (size_t)window)
        return;
    std::vector<double> out(y.size());
    double inv = 1.0 / window;
    for (size_t i = 0; i < y.size(); ++i) {
        int a = int(i) - window / 2;
        int b = a + window;
        a = std::max(a, 0);
        b = std::min(b, (int)y.size());
        double s = 0;
        for (int j = a; j < b; ++j)
            s += y[j];
        out[i] = s / std::max(1, b - a);
    }
    y.swap(out);
}

// Lightweight Savitzky-Golay via convolution with fitted polynomial (simplified: quadratic MA fallback)
void savgolSmooth(std::vector<double>& y, int window, int poly)
{
    window = oddAtLeast(window, 5);
    if ((int)y.size() < window)
        return;
    (void)poly;
    movingAverage(y, window);
}

static void biquadLowpass(std::vector<double>& y, double fc, int order)
{
    // cascade of simple one-pole IIR approximating Butterworth (causal + reverse for zero-phase)
    if (y.size() < 8)
        return;
    fc = std::clamp(fc, 1e-4, 0.49);
    auto once = [&](std::vector<double>& x) {
        // bilinear 1-pole
        double a = std::exp(-2.0 * M_PI * fc);
        double b = 1.0 - a;
        double z = x.empty() ? 0.0 : x[0];
        for (size_t i = 0; i < x.size(); ++i) {
            z = b * x[i] + a * z;
            x[i] = z;
        }
    };
    for (int k = 0; k < std::max(1, order / 2); ++k) {
        once(y);
        std::reverse(y.begin(), y.end());
        once(y);
        std::reverse(y.begin(), y.end());
    }
}

void butterLowpass(std::vector<double>& y, double cutoff, int order)
{
    biquadLowpass(y, cutoff, order);
}

void butterHighpass(std::vector<double>& y, double cutoff, int order)
{
    if (y.size() < 8)
        return;
    std::vector<double> lo = y;
    biquadLowpass(lo, cutoff, order);
    for (size_t i = 0; i < y.size(); ++i)
        y[i] = y[i] - lo[i];
}

static void fftRadix2(std::vector<std::complex<double>>& a, bool inverse)
{
    size_t n = a.size();
    // bit reverse
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2 * M_PI / double(len) * (inverse ? 1 : -1);
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; ++j) {
                auto u = a[i + j];
                auto v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

void fftMagnitude(const std::vector<double>& y, std::vector<double>& freq, std::vector<double>& mag)
{
    size_t n = y.size();
    if (n < 2) {
        freq = {0};
        mag = {0};
        return;
    }
    // pad to pow2
    size_t n2 = 1;
    while (n2 < n)
        n2 <<= 1;
    std::vector<std::complex<double>> a(n2, 0.0);
    double mean = 0;
    for (double v : y)
        mean += v;
    mean /= double(n);
    for (size_t i = 0; i < n; ++i) {
        double w = 0.5 * (1 - std::cos(2 * M_PI * i / (n - 1))); // hann
        a[i] = std::complex<double>((y[i] - mean) * w, 0.0);
    }
    fftRadix2(a, false);
    size_t half = n2 / 2 + 1;
    freq.resize(half);
    mag.resize(half);
    for (size_t i = 0; i < half; ++i) {
        freq[i] = double(i) / double(n);
        mag[i] = 2.0 / n * std::abs(a[i]);
    }
}

} // namespace proc
