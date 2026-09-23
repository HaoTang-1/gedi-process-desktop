#pragma once
#include <QImage>
#include <QString>
#include <functional>
#include <optional>

namespace data {
struct RasterInfo {
    QString path;
    int width = 0, height = 0, bands = 0;
    QString driver, crs, dtype;
    double west = 0, east = 0, south = 0, north = 0;
    QString overview;  // pyramid levels summary
    QString error;
    qint64 sizeBytes = 0;
    bool hasPyramid = false;
};

// Load preview + metadata; GeoTIFF via native C++ first, Python/rasterio fallback
RasterInfo inspectRaster(const QString& path, bool tryPyramid = true);
QImage loadRasterPreview(const QString& path, int maxDim = 1600);
bool convertGeoTiffToPreview(const QString& src, QString& pngOut, QString& metaJson, QString& err);
bool buildTilePyramid(const QString& srcTif, const QString& outDir,
                      const std::function<void(int z, int zmax, QString msg)>& progress = {});

// Portable tool discovery (no machine-specific paths required)
QString findPython();
QString findToolScript(const QString& fileName);
QString findSystemTool(const QString& name); // e.g. h5dump
QString sampleDataDir();
} // namespace data
