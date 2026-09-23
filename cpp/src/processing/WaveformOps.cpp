#include "processing/WaveformOps.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>

namespace proc {

static int oddAtLeast(int n, int m)
{
    n = std::max(n, m);
    return n % 2 ? n : n + 1;
}

static size_t nextPow2(size_t n)
{
    size_t n2 = 1;
    while (n2 < n)
        n2 <<= 1;
    return n2;
}

static void fftRadix2(std::vector<std::complex<double>>& a, bool inverse)
{
    const size_t n = a.size();
    if (n < 2)
        return;
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2 * M_PI / double(len) * (inverse ? 1 : -1);
        const std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; ++j) {
                const auto u = a[i + j];
                const auto v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse) {
        for (auto& v : a)
            v /= double(n);
    }
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
    const double q = std::clamp(quantile / 100.0, 0.0, 1.0);
    const size_t k = size_t(q * (tmp.size() - 1));
    const double floorV = tmp[k];
    for (auto& v : y)
        v -= floorV;
}

void movingAverage(std::vector<double>& y, int window)
{
    if (window <= 1 || y.size() < (size_t)window)
        return;
    std::vector<double> out(y.size());
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

void savgolSmooth(std::vector<double>& y, int window, int poly)
{
    window = oddAtLeast(window, 5);
    if ((int)y.size() < window)
        return;
    // local polynomial fit (order poly) → smoother center value
    const int half = window / 2;
    std::vector<double> out(y.size(), 0.0);
    // precompute X^T X for equally spaced samples -1..half
    std::vector<double> xs(window);
    for (int i = 0; i < window; ++i)
        xs[i] = i - half;
    const int p = std::min(poly, 3);
    // design matrix A (window x p+1); normal equations (p+1)x(p+1)
    std::vector<std::vector<double>> AtA(p + 1, std::vector<double>(p + 1, 0.0));
    for (int i = 0; i < window; ++i) {
        std::vector<double> row(p + 1, 1.0);
        for (int j = 1; j <= p; ++j)
            row[j] = row[j - 1] * xs[i];
        for (int a = 0; a <= p; ++a)
            for (int b = 0; b <= p; ++b)
                AtA[a][b] += row[a] * row[b];
    }
    // Gauss invert AtA
    std::vector<std::vector<double>> M = AtA;
    std::vector<std::vector<double>> Inv(p + 1, std::vector<double>(p + 1, 0.0));
    for (int i = 0; i <= p; ++i)
        Inv[i][i] = 1.0;
    for (int c = 0; c <= p; ++c) {
        int piv = c;
        for (int r = c + 1; r <= p; ++r)
            if (std::abs(M[r][c]) > std::abs(M[piv][c]))
                piv = r;
        std::swap(M[c], M[piv]);
        std::swap(Inv[c], Inv[piv]);
        const double d = M[c][c] == 0 ? 1e-12 : M[c][c];
        for (int j = 0; j <= p; ++j) {
            M[c][j] /= d;
            Inv[c][j] /= d;
        }
        for (int r = 0; r <= p; ++r) {
            if (r == c)
                continue;
            const double f = M[r][c];
            for (int j = 0; j <= p; ++j) {
                M[r][j] -= f * M[c][j];
                Inv[r][j] -= f * Inv[c][j];
            }
        }
    }
    // filter coeff for center: e0^T * Inv * A^T
    std::vector<double> w(window, 0.0);
    for (int i = 0; i < window; ++i) {
        std::vector<double> row(p + 1, 1.0);
        for (int j = 1; j <= p; ++j)
            row[j] = row[j - 1] * xs[i];
        double acc = 0;
        for (int a = 0; a <= p; ++a)
            acc += Inv[0][a] * row[a];
        w[i] = acc;
    }
    for (size_t i = 0; i < y.size(); ++i) {
        double s = 0;
        for (int k = -half; k <= half; ++k) {
            const int j = std::clamp(int(i) + k, 0, int(y.size()) - 1);
            s += w[k + half] * y[j];
        }
        out[i] = s;
    }
    y.swap(out);
}

static void biquadLowpass(std::vector<double>& y, double fc, int order)
{
    if (y.size() < 8)
        return;
    fc = std::clamp(fc, 1e-4, 0.49);
    auto once = [&](std::vector<double>& x) {
        const double a = std::exp(-2.0 * M_PI * fc);
        const double b = 1.0 - a;
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

void butterBandpass(std::vector<double>& y, double lo, double hi, int order)
{
    if (lo > hi)
        std::swap(lo, hi);
    butterLowpass(y, hi, order);
    butterHighpass(y, lo, order);
}

void butterBandstop(std::vector<double>& y, double lo, double hi, int order)
{
    if (y.size() < 8)
        return;
    if (lo > hi)
        std::swap(lo, hi);
    std::vector<double> band = y;
    butterBandpass(band, lo, hi, order);
    for (size_t i = 0; i < y.size(); ++i)
        y[i] = y[i] - band[i];
}

void medianFilter(std::vector<double>& y, int window)
{
    window = oddAtLeast(window, 3);
    if (window < 3 || y.size() < (size_t)window)
        return;
    const int half = window / 2;
    std::vector<double> out(y.size());
    std::vector<double> buf(window);
    for (size_t i = 0; i < y.size(); ++i) {
        for (int k = -half; k <= half; ++k)
            buf[k + half] = y[std::clamp(int(i) + k, 0, int(y.size()) - 1)];
        std::nth_element(buf.begin(), buf.begin() + half, buf.end());
        out[i] = buf[half];
    }
    y.swap(out);
}

void gaussianSmooth(std::vector<double>& y, double sigma)
{
    if (sigma <= 0 || y.size() < 3)
        return;
    const int radius = std::max(1, int(std::ceil(sigma * 3)));
    std::vector<double> ker(2 * radius + 1);
    double sum = 0;
    for (int i = -radius; i <= radius; ++i) {
        ker[i + radius] = std::exp(-0.5 * (i * i) / (sigma * sigma));
        sum += ker[i + radius];
    }
    for (auto& k : ker)
        k /= sum;
    std::vector<double> out(y.size());
    for (size_t i = 0; i < y.size(); ++i) {
        double s = 0;
        for (int k = -radius; k <= radius; ++k) {
            const int j = std::clamp(int(i) + k, 0, int(y.size()) - 1);
            s += y[j] * ker[k + radius];
        }
        out[i] = s;
    }
    y.swap(out);
}

void movingRms(std::vector<double>& y, int window)
{
    window = std::max(1, window);
    std::vector<double> out(y.size());
    for (size_t i = 0; i < y.size(); ++i) {
        int a = int(i) - window / 2;
        int b = a + window;
        a = std::max(a, 0);
        b = std::min(b, (int)y.size());
        double s = 0;
        for (int j = a; j < b; ++j)
            s += y[j] * y[j];
        out[i] = std::sqrt(s / std::max(1, b - a));
    }
    y.swap(out);
}

void derivative(std::vector<double>& y)
{
    if (y.size() < 2)
        return;
    std::vector<double> out(y.size());
    for (size_t i = 0; i < y.size(); ++i) {
        const size_t a = i == 0 ? 0 : i - 1;
        const size_t b = std::min(y.size() - 1, i + 1);
        out[i] = (y[b] - y[a]) / double(std::max<ptrdiff_t>(1, ptrdiff_t(b) - ptrdiff_t(a)));
    }
    y.swap(out);
}

void integrate(std::vector<double>& y)
{
    double acc = 0;
    for (auto& v : y) {
        acc += v;
        v = acc;
    }
}

void detrendLinear(std::vector<double>& y)
{
    const size_t n = y.size();
    if (n < 3)
        return;
    // least squares y = a + b x
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (size_t i = 0; i < n; ++i) {
        sx += double(i);
        sy += y[i];
        sxx += double(i) * double(i);
        sxy += double(i) * y[i];
    }
    const double den = double(n) * sxx - sx * sx;
    if (std::abs(den) < 1e-12)
        return;
    const double b = (double(n) * sxy - sx * sy) / den;
    const double a = (sy - b * sx) / double(n);
    for (size_t i = 0; i < n; ++i)
        y[i] -= a + b * double(i);
}

void absSignal(std::vector<double>& y)
{
    for (auto& v : y)
        v = std::abs(v);
}

void squareSignal(std::vector<double>& y)
{
    for (auto& v : y)
        v = v * v;
}

void flipSignal(std::vector<double>& y)
{
    std::reverse(y.begin(), y.end());
}

void thresholdHard(std::vector<double>& y, double level)
{
    for (auto& v : y)
        v = (v >= level) ? v : 0.0;
}

void resampleLinear(std::vector<double>& y, int outN)
{
    if (outN < 2 || y.size() < 2)
        return;
    std::vector<double> out(outN);
    const double step = double(y.size() - 1) / double(outN - 1);
    for (int i = 0; i < outN; ++i) {
        const double t = i * step;
        const int i0 = std::min(int(y.size()) - 2, int(t));
        const double f = t - i0;
        out[i] = y[i0] * (1 - f) + y[i0 + 1] * f;
    }
    y.swap(out);
}

void spectralDenoise(std::vector<double>& y, double keepRatio)
{
    const size_t n = y.size();
    if (n < 8)
        return;
    keepRatio = std::clamp(keepRatio, 0.01, 1.0);
    const size_t n2 = nextPow2(n);
    std::vector<std::complex<double>> a(n2, 0.0);
    for (size_t i = 0; i < n; ++i)
        a[i] = y[i];
    fftRadix2(a, false);
    // soft threshold by magnitude rank
    std::vector<double> mags(n2);
    for (size_t i = 0; i < n2; ++i)
        mags[i] = std::abs(a[i]);
    std::vector<double> sorted = mags;
    std::sort(sorted.begin(), sorted.end());
    const double thr = sorted[size_t((1.0 - keepRatio) * (sorted.size() - 1))];
    for (size_t i = 0; i < n2; ++i) {
        const double m = std::abs(a[i]);
        if (m < thr && i > 1 && i < n2 - 1)
            a[i] = 0.0;
    }
    fftRadix2(a, true);
    y.resize(n);
    for (size_t i = 0; i < n; ++i)
        y[i] = a[i].real();
}

void envelopeHilbert(std::vector<double>& y)
{
    const size_t n = y.size();
    if (n < 8)
        return;
    const size_t n2 = nextPow2(n);
    std::vector<std::complex<double>> a(n2, 0.0);
    for (size_t i = 0; i < n; ++i)
        a[i] = y[i];
    fftRadix2(a, false);
    // analytic: keep positive freqs, double them (except DC/Nyquist)
    for (size_t i = 1; i < n2 / 2; ++i)
        a[i] *= 2.0;
    for (size_t i = n2 / 2 + 1; i < n2; ++i)
        a[i] = 0.0;
    fftRadix2(a, true);
    for (size_t i = 0; i < n; ++i)
        y[i] = std::abs(a[i]);
}

static void spectrumOf(const std::vector<double>& y, std::vector<std::complex<double>>& a,
                       size_t& n2)
{
    const size_t n = y.size();
    n2 = nextPow2(std::max<size_t>(n, 2));
    a.assign(n2, 0.0);
    double mean = 0;
    for (double v : y)
        mean += v;
    mean /= double(std::max<size_t>(n, 1));
    for (size_t i = 0; i < n; ++i) {
        const double w = (n > 1) ? 0.5 * (1 - std::cos(2 * M_PI * i / (n - 1))) : 1.0;
        a[i] = std::complex<double>((y[i] - mean) * w, 0.0);
    }
    fftRadix2(a, false);
}

void fftMagnitude(const std::vector<double>& y, std::vector<double>& freq, std::vector<double>& mag)
{
    const size_t n = y.size();
    if (n < 2) {
        freq = {0};
        mag = {0};
        return;
    }
    std::vector<std::complex<double>> a;
    size_t n2 = 0;
    spectrumOf(y, a, n2);
    const size_t half = n2 / 2 + 1;
    freq.resize(half);
    mag.resize(half);
    for (size_t i = 0; i < half; ++i) {
        freq[i] = double(i) / double(n);
        mag[i] = 2.0 / n * std::abs(a[i]);
    }
}

void fftPhase(const std::vector<double>& y, std::vector<double>& freq, std::vector<double>& phase)
{
    const size_t n = y.size();
    if (n < 2) {
        freq = {0};
        phase = {0};
        return;
    }
    std::vector<std::complex<double>> a;
    size_t n2 = 0;
    spectrumOf(y, a, n2);
    const size_t half = n2 / 2 + 1;
    freq.resize(half);
    phase.resize(half);
    for (size_t i = 0; i < half; ++i) {
        freq[i] = double(i) / double(n);
        phase[i] = std::arg(a[i]);
    }
}

void fftPower(const std::vector<double>& y, std::vector<double>& freq, std::vector<double>& psd)
{
    std::vector<double> mag;
    fftMagnitude(y, freq, mag);
    psd.resize(mag.size());
    for (size_t i = 0; i < mag.size(); ++i)
        psd[i] = 0.5 * mag[i] * mag[i];
}

void ifftFromMagnitude(const std::vector<double>& mag, std::vector<double>& yOut)
{
    // reconstruct zero-phase real signal from |FFT| (half spectrum assumed)
    const size_t half = mag.size();
    if (half < 2) {
        yOut = mag;
        return;
    }
    const size_t n2 = (half - 1) * 2;
    std::vector<std::complex<double>> a(n2, 0.0);
    for (size_t i = 0; i < half; ++i)
        a[i] = std::complex<double>(mag[i], 0.0);
    for (size_t i = 1; i < half - 1; ++i)
        a[n2 - i] = std::complex<double>(mag[i], 0.0);
    fftRadix2(a, true);
    yOut.resize(half);
    for (size_t i = 0; i < half; ++i)
        yOut[i] = a[i].real();
}

void ifftRoundtrip(const std::vector<double>& y, std::vector<double>& yOut)
{
    const size_t n = y.size();
    if (n < 2) {
        yOut = y;
        return;
    }
    const size_t n2 = nextPow2(n);
    std::vector<std::complex<double>> a(n2, 0.0);
    for (size_t i = 0; i < n; ++i)
        a[i] = y[i];
    fftRadix2(a, false);
    fftRadix2(a, true);
    yOut.resize(n);
    for (size_t i = 0; i < n; ++i)
        yOut[i] = a[i].real();
}

} // namespace proc
