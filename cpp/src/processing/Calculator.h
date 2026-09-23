#pragma once
#include "core/TransformRegistry.h"
#include "data/GediLoader.h"
#include <QMap>
#include <QVariantMap>
#include <QVector>
#include <functional>

namespace proc {

struct ChainStep {
    QString id;
    QVariantMap params;
};

struct ProcessChain {
    QString name = QStringLiteral("custom_chain");
    QVector<ChainStep> steps;
    QString summary() const;
};

struct ChainOut {
    std::vector<double> y;
    QString label;
    bool hasSpectrum = false;
    std::vector<double> auxX, auxY;
    QString auxLabel;
};

ChainOut runChain(const std::vector<double>& y, const ProcessChain& chain);

struct BatchRow {
    QString file, beam, product;
    double shot = 0, lon = 0, lat = 0;
    double outMean = 0, outStd = 0, outMin = 0, outMax = 0, outEnergy = 0;
    QMap<QString, double> metrics;
};

struct BatchResult {
    QVector<BatchRow> rows;
    int nOk = 0, nSkip = 0;
};

BatchResult runBatch(const data::GediLoader& loader, const QList<int>& fileIds,
                     const ProcessChain& chain, bool qualityOnly, int stride, int maxPerBeam,
                     const std::function<void(int, int, QString)>& progress = {});

} // namespace proc
