#pragma once
#include <QImage>
#include <QString>
#include <functional>

namespace data {

struct GeoTiffInfo {
    int width = 0, height = 0, samples = 3;
    int bitsPerSample = 8;
    int compression = 1;
    int photometric = 2;
    double west = 0, east = 0, south = 0, north = 0;
    QString crs = QStringLiteral("EPSG:4326");
    bool hasGeo = false;
    QString error;
};

// Classic TIFF (not BigTIFF). Compressions: none / PackBits / LZW / Deflate.
// Reads georeference from ModelPixelScale + ModelTiepoint (or ModelTransformation).
bool readGeoTiffInfo(const QString& path, GeoTiffInfo& info);

// Decoded preview (RGB32), long side <= maxDim, with WGS84-ish bounds from geo tags.
// Assumes geographic CRS when only ModelPixelScale/Tiepoint present (NE1 / EPSG:4326).
bool readGeoTiffPreview(const QString& path, int maxDim, QImage& out,
                        double& west, double& east, double& south, double& north,
                        GeoTiffInfo& info);

// Build z/x/y.png + meta.json (same layout as tools/build_tile_pyramid.py).
// Returns false if source cannot be decoded without Python.
bool buildTilePyramidNative(const QString& srcTif, const QString& outDir,
                            const std::function<void(int z, int zmax, int tiles)>& progress = {});

} // namespace data
