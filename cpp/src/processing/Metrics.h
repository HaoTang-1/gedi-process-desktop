#pragma once
#include "data/GediLoader.h"
#include <QMap>
#include <QString>
#include <vector>

namespace proc {

QMap<QString, double> computeMetrics(const data::WaveformResult& r);

}
