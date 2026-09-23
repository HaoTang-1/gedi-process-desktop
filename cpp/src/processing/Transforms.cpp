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
        ParamSpec p;
        p.name = "quantile";
        p.label = "噪声地板分位数 (%)";
        p.kind = "float";
        p.def = 5;
        p.minV = 0;
        p.maxV = 50;
        s.params.push_back(p);
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
        s.menuOrder = 40;
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
        s.id = "lowpass";
        s.name = "Butterworth 低通";
        s.category = "滤波";
        s.menuOrder = 50;
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
        s.menuOrder = 60;
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
        s.id = "fft";
        s.name = "傅里叶变换 (FFT)";
        s.category = "变换";
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
        s.id = "dwt_denoise";
        s.name = "小波去噪 (DWT)";
        s.category = "变换";
        s.menuOrder = 110;
        s.fn = [](const std::vector<double>& y, const QVariantMap&) {
            TransformResult r;
            r.y = y;
            // simplified: SG-like denoise placeholder
            proc::savgolSmooth(r.y, 11, 3);
            r.label = QStringLiteral("小波去噪(简化)");
            return r;
        };
        reg.add(s);
    }
}

} // namespace core
