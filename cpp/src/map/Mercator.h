#pragma once
#include <algorithm>
#include <cmath>

namespace map {

// EPSG:3857 helpers. Display space is Web Mercator so XYZ tiles never stretch.
constexpr double kMercR = 6378137.0;
constexpr double kMercOrigin = 20037508.342789244; // pi * R
constexpr double kMercMaxLat = 85.05112878;

inline double lonToMercX(double lon)
{
    return lon * M_PI / 180.0 * kMercR;
}

inline double latToMercY(double lat)
{
    lat = std::max(-kMercMaxLat, std::min(kMercMaxLat, lat));
    return std::log(std::tan(M_PI / 4.0 + lat * M_PI / 360.0)) * kMercR;
}

inline double mercXToLon(double x)
{
    return x / kMercR * 180.0 / M_PI;
}

inline double mercYToLat(double y)
{
    return (2.0 * std::atan(std::exp(y / kMercR)) - M_PI / 2.0) * 180.0 / M_PI;
}

// XYZ tile bounds in mercator meters
inline double tileCountAt(int z)
{
    return std::pow(2.0, double(z));
}

inline void tileMercBounds(int z, int x, int y, double& mx0, double& my0, double& mx1, double& my1)
{
    const double n = tileCountAt(z);
    const double tw = 2.0 * kMercOrigin / n;
    mx0 = -kMercOrigin + x * tw;
    mx1 = mx0 + tw;
    my1 = kMercOrigin - y * tw;
    my0 = my1 - tw;
}

inline int lonToTileX(double lon, int z)
{
    const double n = tileCountAt(z);
    int x = int(std::floor((lon + 180.0) / 360.0 * n));
    const int maxv = (1 << z) - 1;
    return std::clamp(x, 0, maxv);
}

inline int latToTileY(double lat, int z)
{
    lat = std::max(-kMercMaxLat, std::min(kMercMaxLat, lat));
    const double n = tileCountAt(z);
    const double latR = lat * M_PI / 180.0;
    int y = int(std::floor((1.0 - std::asinh(std::tan(latR)) / M_PI) / 2.0 * n));
    const int maxv = (1 << z) - 1;
    return std::clamp(y, 0, maxv);
}

} // namespace map
