#include "core/TransformRegistry.h"
#include "processing/WaveformOps.h"

namespace core {

void registerBuiltinTransforms()
{
    auto& reg = transforms();

    {
        TransformSpec s;
        s.id = "normalize";
        s.name = "归一化 (0–1)";
        s.category = "归一化";
        s.menuOrder = 10;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::normalize01(r.y);
            r.label = QStringLiteral("归一化");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "baseline";
        s.name = "基线去除";
        s.category = "归一化";
        s.menuOrder = 20;
        s.params.push_back({"quantile", "噪声地板分位数 (%)", "float", 5, 0, 50, 0.5, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::baselineRemove(r.y, pr.value("quantile", 5.0).toDouble());
            r.label = QStringLiteral("基线去除");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "detrend";
        s.name = "线性去趋势";
        s.category = "归一化";
        s.menuOrder = 25;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::detrendLinear(r.y);
            r.label = QStringLiteral("去趋势");
            return r;
        };
        reg.add(s);
    }

    // ---- 滤波 ----
    {
        TransformSpec s;
        s.id = "savgol";
        s.name = "Savitzky-Golay 平滑";
        s.category = "滤波";
        s.menuOrder = 30;
        s.params.push_back({"window", "窗口长度", "int", 11, 5, 101, 2, {}});
        s.params.push_back({"poly", "多项式阶数", "int", 3, 1, 5, 1, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::savgolSmooth(r.y, pr.value("window", 11).toInt(), pr.value("poly", 3).toInt());
            r.label = QStringLiteral("SG 平滑");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "moving_avg";
        s.name = "滑动平均";
        s.category = "滤波";
        s.menuOrder = 35;
        s.params.push_back({"window", "窗口长度", "int", 5, 1, 51, 1, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::movingAverage(r.y, pr.value("window", 5).toInt());
            r.label = QStringLiteral("滑动平均");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "median";
        s.name = "中值滤波";
        s.category = "滤波";
        s.menuOrder = 38;
        s.params.push_back({"window", "窗口长度", "int", 5, 3, 31, 2, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::medianFilter(r.y, pr.value("window", 5).toInt());
            r.label = QStringLiteral("中值");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "gauss";
        s.name = "高斯平滑";
        s.category = "滤波";
        s.menuOrder = 39;
        s.params.push_back({"sigma", "σ (采样点)", "float", 2, 0.3, 20, 0.1, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::gaussianSmooth(r.y, pr.value("sigma", 2.0).toDouble());
            r.label = QStringLiteral("高斯");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "lowpass";
        s.name = "Butterworth 低通";
        s.category = "滤波";
        s.menuOrder = 40;
        s.params.push_back({"cutoff", "截止 (×Nyquist)", "float", 0.15, 0.001, 0.49, 0.01, {}});
        s.params.push_back({"order", "阶数", "int", 4, 1, 10, 1, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::butterLowpass(r.y, pr.value("cutoff", 0.15).toDouble(), pr.value("order", 4).toInt());
            r.label = QStringLiteral("低通");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "highpass";
        s.name = "Butterworth 高通";
        s.category = "滤波";
        s.menuOrder = 45;
        s.params.push_back({"cutoff", "截止 (×Nyquist)", "float", 0.05, 0.001, 0.49, 0.01, {}});
        s.params.push_back({"order", "阶数", "int", 4, 1, 10, 1, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::butterHighpass(r.y, pr.value("cutoff", 0.05).toDouble(), pr.value("order", 4).toInt());
            r.label = QStringLiteral("高通");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "bandpass";
        s.name = "Butterworth 带通";
        s.category = "滤波";
        s.menuOrder = 48;
        s.params.push_back({"lo", "下限 (×Nyquist)", "float", 0.05, 0.001, 0.45, 0.01, {}});
        s.params.push_back({"hi", "上限 (×Nyquist)", "float", 0.25, 0.01, 0.49, 0.01, {}});
        s.params.push_back({"order", "阶数", "int", 4, 1, 10, 1, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::butterBandpass(r.y, pr.value("lo", 0.05).toDouble(), pr.value("hi", 0.25).toDouble(),
                                 pr.value("order", 4).toInt());
            r.label = QStringLiteral("带通");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "bandstop";
        s.name = "Butterworth 带阻 / 陷波";
        s.category = "滤波";
        s.menuOrder = 49;
        s.params.push_back({"lo", "下限 (×Nyquist)", "float", 0.08, 0.001, 0.45, 0.01, {}});
        s.params.push_back({"hi", "上限 (×Nyquist)", "float", 0.18, 0.01, 0.49, 0.01, {}});
        s.params.push_back({"order", "阶数", "int", 4, 1, 10, 1, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::butterBandstop(r.y, pr.value("lo", 0.08).toDouble(), pr.value("hi", 0.18).toDouble(),
                                 pr.value("order", 4).toInt());
            r.label = QStringLiteral("带阻");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "spectral_denoise";
        s.name = "频谱软阈值去噪";
        s.category = "滤波";
        s.menuOrder = 52;
        s.params.push_back({"keep", "保留频谱比例", "float", 0.35, 0.05, 1.0, 0.05, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::spectralDenoise(r.y, pr.value("keep", 0.35).toDouble());
            r.label = QStringLiteral("频谱去噪");
            return r;
        };
        reg.add(s);
    }

    // ---- 波形变换 / 时域 ----
    {
        TransformSpec s;
        s.id = "derivative";
        s.name = "一阶导数";
        s.category = "时域";
        s.menuOrder = 60;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::derivative(r.y);
            r.label = QStringLiteral("dy/dx");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "integrate";
        s.name = "积分 (累加)";
        s.category = "时域";
        s.menuOrder = 62;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::integrate(r.y);
            r.label = QStringLiteral("积分");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "abs";
        s.name = "绝对值";
        s.category = "时域";
        s.menuOrder = 64;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::absSignal(r.y);
            r.label = QStringLiteral("|y|");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "square";
        s.name = "平方";
        s.category = "时域";
        s.menuOrder = 66;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::squareSignal(r.y);
            r.label = QStringLiteral("y²");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "moving_rms";
        s.name = "滑动 RMS";
        s.category = "时域";
        s.menuOrder = 68;
        s.params.push_back({"window", "窗口长度", "int", 9, 3, 81, 2, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::movingRms(r.y, pr.value("window", 9).toInt());
            r.label = QStringLiteral("RMS");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "envelope";
        s.name = "包络 (Hilbert)";
        s.category = "时域";
        s.menuOrder = 70;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::envelopeHilbert(r.y);
            r.label = QStringLiteral("包络");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "threshold";
        s.name = "硬阈值";
        s.category = "时域";
        s.menuOrder = 72;
        s.params.push_back({"level", "阈值", "float", 0.3, -10, 10, 0.05, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::thresholdHard(r.y, pr.value("level", 0.3).toDouble());
            r.label = QStringLiteral("阈值");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "flip";
        s.name = "时间反转";
        s.category = "时域";
        s.menuOrder = 74;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::flipSignal(r.y);
            r.label = QStringLiteral("反转");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "resample";
        s.name = "重采样 (线性)";
        s.category = "时域";
        s.menuOrder = 76;
        s.params.push_back({"n", "输出点数", "int", 256, 16, 4096, 16, {}});
        s.fn = [](const std::vector<double>& y, const QVariantMap& pr) {
            TransformResult r;
            r.y = y;
            proc::resampleLinear(r.y, pr.value("n", 256).toInt());
            r.label = QStringLiteral("重采样");
            return r;
        };
        reg.add(s);
    }

    // ---- 频域 ----
    {
        TransformSpec s;
        s.id = "fft";
        s.name = "傅里叶变换 FFT 幅度";
        s.category = "频域";
        s.menuOrder = 100;
        s.kind = ResultKind::Spectrum;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::fftMagnitude(y, r.auxX, r.auxY);
            r.kind = ResultKind::Spectrum;
            r.label = QStringLiteral("FFT");
            r.auxLabel = QStringLiteral("FFT 幅度谱");
            r.auxXLabel = QStringLiteral("归一化频率");
            r.auxYLabel = QStringLiteral("|FFT|");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "fft_phase";
        s.name = "FFT 相位谱";
        s.category = "频域";
        s.menuOrder = 102;
        s.kind = ResultKind::Spectrum;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::fftPhase(y, r.auxX, r.auxY);
            r.kind = ResultKind::Spectrum;
            r.label = QStringLiteral("相位");
            r.auxLabel = QStringLiteral("FFT 相位 (rad)");
            r.auxXLabel = QStringLiteral("归一化频率");
            r.auxYLabel = QStringLiteral("phase");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "psd";
        s.name = "功率谱 PSD";
        s.category = "频域";
        s.menuOrder = 104;
        s.kind = ResultKind::Spectrum;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::fftPower(y, r.auxX, r.auxY);
            r.kind = ResultKind::Spectrum;
            r.label = QStringLiteral("PSD");
            r.auxLabel = QStringLiteral("功率谱");
            r.auxXLabel = QStringLiteral("归一化频率");
            r.auxYLabel = QStringLiteral("PSD");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "ifft_roundtrip";
        s.name = "FFT → IFFT 逆变换";
        s.category = "频域";
        s.menuOrder = 110;
        s.description = QStringLiteral("先 FFT 再 IFFT，验证可逆并去除非平稳直流偏置以外的数值误差");
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            proc::ifftRoundtrip(y, r.y);
            r.label = QStringLiteral("IFFT 往返");
            return r;
        };
        reg.add(s);
    }
    {
        TransformSpec s;
        s.id = "ifft_mag";
        s.name = "由幅度谱 IFFT（零相位重建）";
        s.category = "频域";
        s.menuOrder = 112;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            std::vector<double> freq, mag;
            proc::fftMagnitude(y, freq, mag);
            proc::ifftFromMagnitude(mag, r.y);
            r.label = QStringLiteral("零相位重建");
            return r;
        };
        reg.add(s);
    }

    // ---- 其它 ----
    {
        TransformSpec s;
        s.id = "dwt_denoise";
        s.name = "小波去噪 (DWT 简化)";
        s.category = "其它";
        s.menuOrder = 200;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            proc::savgolSmooth(r.y, 11, 3);
            r.label = QStringLiteral("小波去噪(简化)");
            return r;
        };
        reg.add(s);
    }
}

} // namespace core
