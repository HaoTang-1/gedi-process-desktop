#pragma once
#include <cmath>
#include <vector>

namespace proc {

struct ProcessParams {
    bool lowpass = false;
    bool highpass = false;
    bool savgol = false;
    bool movingAvg = false;
    bool normalize = false;
    bool baseline = false;
    bool fft = false;
    double cutoffLow = 0.15;
    double cutoffHigh = 0.05;
    int order = 4;
    int savgolWindow = 11;
    int savgolPoly = 3;
    int avgWindow = 5;
};

void normalize01(std::vector<double>& y);
void baselineRemove(std::vector<double>& y, double quantile = 5.0);
void movingAverage(std::vector<double>& y, int window);
void savgolSmooth(std::vector<double>& y, int window, int poly);
void butterLowpass(std::vector<double>& y, double cutoff, int order);
void butterHighpass(std::vector<double>& y, double cutoff, int order);
void fftMagnitude(const std::vector<double>& y, std::vector<double>& freq, std::vector<double>& mag);

} // namespace proc
