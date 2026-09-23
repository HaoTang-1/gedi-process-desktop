#pragma once
#include <complex>
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
void butterBandpass(std::vector<double>& y, double lo, double hi, int order);
void butterBandstop(std::vector<double>& y, double lo, double hi, int order);
void medianFilter(std::vector<double>& y, int window);
void gaussianSmooth(std::vector<double>& y, double sigma);
void movingRms(std::vector<double>& y, int window);
void derivative(std::vector<double>& y);
void integrate(std::vector<double>& y);
void detrendLinear(std::vector<double>& y);
void absSignal(std::vector<double>& y);
void squareSignal(std::vector<double>& y);
void flipSignal(std::vector<double>& y);
void thresholdHard(std::vector<double>& y, double level);
void resampleLinear(std::vector<double>& y, int outN);
void spectralDenoise(std::vector<double>& y, double keepRatio);
void envelopeHilbert(std::vector<double>& y);
void fftPhase(const std::vector<double>& y, std::vector<double>& freq, std::vector<double>& phase);
void fftMagnitude(const std::vector<double>& y, std::vector<double>& freq, std::vector<double>& mag);
void fftPower(const std::vector<double>& y, std::vector<double>& freq, std::vector<double>& psd);
void ifftFromMagnitude(const std::vector<double>& mag, std::vector<double>& yOut);
void ifftRoundtrip(const std::vector<double>& y, std::vector<double>& yOut);

} // namespace proc
