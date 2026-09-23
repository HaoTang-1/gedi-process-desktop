#pragma once
#include <QString>
#include <cstdint>
#include <functional>
#include <vector>

// Dynamic loader for hdf5.dll (C ABI) — works with Anaconda MSVC hdf5.dll from MinGW.
namespace h5 {

bool available();
QString errorString();
bool ensureLoaded(const QString& hintPath = QString());
QStringList searchHintPaths();

// Opaque handles as void*
using hid_t = long long;

bool fileOpen(const QString& path, hid_t& out);
void fileClose(hid_t id);

// list immediate names under path ("" = root)
bool listNames(hid_t fileId, const QString& groupPath, QStringList& outNames, QList<int>& outIsGroup);

bool datasetInfo(hid_t fileId, const QString& path,
                 QVector<qint64>& dims, int& dtypeClass, int& dtypeSize);

// Read dataset as doubles (any numeric type)
bool readAsDouble(hid_t fileId, const QString& path, std::vector<double>& out, QVector<qint64>& dims);

// Fast 1D hyperslab: read count values starting at start0 (0-based) from a 1D/flat dataset
bool readSlice1D(hid_t fileId, const QString& path, long long start0, long long count,
                 std::vector<double>& out);

bool readAttrString(hid_t fileId, const QString& objPath, const QString& attrName, QString& value);

} // namespace h5
