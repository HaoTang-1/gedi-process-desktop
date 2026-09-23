#include "data/GeoTiffLite.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef HAS_ZLIB
#include <zlib.h>
#endif

namespace data {
namespace {

// TIFF types
enum : quint16 {
    T_BYTE = 1, T_ASCII = 2, T_SHORT = 3, T_LONG = 4, T_RATIONAL = 5,
    T_SBYTE = 6, T_UNDEFINED = 7, T_SSHORT = 8, T_SLONG = 9, T_SRATIONAL = 10,
    T_FLOAT = 11, T_DOUBLE = 12
};

enum : quint16 {
    TagImageWidth = 256,
    TagImageLength = 257,
    TagBitsPerSample = 258,
    TagCompression = 259,
    TagPhotometric = 262,
    TagStripOffsets = 273,
    TagSamplesPerPixel = 277,
    TagRowsPerStrip = 278,
    TagStripByteCounts = 279,
    TagPlanarConfig = 284,
    TagPredictor = 317,
    TagTileWidth = 322,
    TagTileLength = 323,
    TagTileOffsets = 324,
    TagTileByteCounts = 325,
    TagModelPixelScale = 33550,
    TagModelTiepoint = 33922,
    TagModelTransformation = 34264,
};

struct IfdEntry {
    quint16 tag = 0, type = 0;
    quint32 count = 0;
    quint32 valueOrOffset = 0;
};

struct TiffFile {
    QFile f;
    bool le = true;
    quint32 nextIfd = 0;

    bool open(const QString& path)
    {
        f.setFileName(path);
        if (!f.open(QIODevice::ReadOnly))
            return false;
        char hdr[8];
        if (f.read(hdr, 8) != 8)
            return false;
        if (hdr[0] == 'I' && hdr[1] == 'I')
            le = true;
        else if (hdr[0] == 'M' && hdr[1] == 'M')
            le = false;
        else
            return false;
        auto u16 = [&](const char* p) {
            quint16 v;
            std::memcpy(&v, p, 2);
            return le ? v : qFromBigEndian(v);
        };
        auto u32 = [&](const char* p) {
            quint32 v;
            std::memcpy(&v, p, 4);
            return le ? v : qFromBigEndian(v);
        };
        if (u16(hdr + 2) != 42)
            return false; // BigTIFF (43) not supported
        nextIfd = u32(hdr + 4);
        return true;
    }

    quint16 u16(const char* p) const
    {
        quint16 v;
        std::memcpy(&v, p, 2);
        return le ? v : qFromBigEndian(v);
    }
    quint32 u32(const char* p) const
    {
        quint32 v;
        std::memcpy(&v, p, 4);
        return le ? v : qFromBigEndian(v);
    }
    quint64 u64(const char* p) const
    {
        quint64 v;
        std::memcpy(&v, p, 8);
        return le ? v : qFromBigEndian(v);
    }

    bool readIfd(quint32 offset, std::vector<IfdEntry>& ents)
    {
        ents.clear();
        if (!offset || !f.seek(offset))
            return false;
        char nbuf[2];
        if (f.read(nbuf, 2) != 2)
            return false;
        const quint16 n = u16(nbuf);
        ents.resize(n);
        for (int i = 0; i < n; ++i) {
            char e[12];
            if (f.read(e, 12) != 12)
                return false;
            ents[i].tag = u16(e);
            ents[i].type = u16(e + 2);
            ents[i].count = u32(e + 4);
            ents[i].valueOrOffset = u32(e + 8);
        }
        char nb[4];
        if (f.read(nb, 4) == 4)
            nextIfd = u32(nb);
        return true;
    }

    int typeSize(quint16 t) const
    {
        switch (t) {
        case T_BYTE:
        case T_ASCII:
        case T_SBYTE:
        case T_UNDEFINED:
            return 1;
        case T_SHORT:
        case T_SSHORT:
            return 2;
        case T_LONG:
        case T_SLONG:
        case T_FLOAT:
            return 4;
        case T_RATIONAL:
        case T_SRATIONAL:
        case T_DOUBLE:
            return 8;
        default:
            return 1;
        }
    }

    // Load numeric values of an entry (as double / qulonglong)
    bool entryValues(const IfdEntry& e, std::vector<double>& out)
    {
        const int es = typeSize(e.type);
        const qint64 nbytes = qint64(es) * e.count;
        QByteArray raw;
        if (nbytes <= 4) {
            raw = QByteArray(reinterpret_cast<const char*>(&e.valueOrOffset), 4);
            raw.resize(int(nbytes));
        } else {
            if (!f.seek(e.valueOrOffset))
                return false;
            raw = f.read(nbytes);
            if (raw.size() != nbytes)
                return false;
        }
        out.resize(e.count);
        const char* p = raw.constData();
        for (quint32 i = 0; i < e.count; ++i) {
            switch (e.type) {
            case T_BYTE:
            case T_SBYTE:
            case T_UNDEFINED:
                out[i] = quint8(p[i]);
                break;
            case T_SHORT:
            case T_SSHORT:
                out[i] = u16(p + i * 2);
                break;
            case T_LONG:
            case T_SLONG:
                out[i] = u32(p + i * 4);
                break;
            case T_FLOAT: {
                float fv;
                std::memcpy(&fv, p + i * 4, 4);
                if (!le) {
                    quint32 be = qFromBigEndian(quint32(*reinterpret_cast<quint32*>(&fv)));
                    std::memcpy(&fv, &be, 4);
                }
                out[i] = fv;
                break;
            }
            case T_DOUBLE: {
                double dv;
                std::memcpy(&dv, p + i * 8, 8);
                if (!le) {
                    quint64 be = qFromBigEndian(quint64(*reinterpret_cast<quint64*>(&dv)));
                    std::memcpy(&dv, &be, 8);
                }
                out[i] = dv;
                break;
            }
            case T_RATIONAL:
            case T_SRATIONAL: {
                const quint32 a = u32(p + i * 8);
                const quint32 b = u32(p + i * 8 + 4);
                out[i] = b ? double(a) / double(b) : 0.0;
                break;
            }
            default:
                out[i] = 0;
                break;
            }
        }
        return true;
    }

    bool entryU32s(const IfdEntry& e, std::vector<quint32>& out)
    {
        std::vector<double> tmp;
        if (!entryValues(e, tmp))
            return false;
        out.resize(tmp.size());
        for (size_t i = 0; i < tmp.size(); ++i)
            out[i] = quint32(tmp[i]);
        return true;
    }
};

// --- LZW (TIFF): codes packed MSB-first ---
bool lzwDecode(const QByteArray& src, QByteArray& dst, int minCodeSizeHint = 8)
{
    Q_UNUSED(minCodeSizeHint);
    dst.clear();
    if (src.isEmpty())
        return false;
    const quint8* data = reinterpret_cast<const quint8*>(src.constData());
    const int nBits = src.size() * 8;
    int bitPos = 0;
    auto readCode = [&](int codeLen) -> int {
        if (bitPos + codeLen > nBits)
            return -1;
        int code = 0;
        for (int i = 0; i < codeLen; ++i) {
            const int bit = (data[bitPos >> 3] >> (7 - (bitPos & 7))) & 1;
            code = (code << 1) | bit;
            ++bitPos;
        }
        return code;
    };

    const int CLEAR = 256, EOI = 257;
    std::vector<QByteArray> dict(4096);
    for (int i = 0; i < 256; ++i)
        dict[i] = QByteArray(1, char(i));
    int dictSize = 258;
    int codeLen = 9;
    QByteArray prev;
    dst.reserve(src.size() * 3);
    for (;;) {
        const int code = readCode(codeLen);
        if (code < 0 || code == EOI)
            break;
        if (code == CLEAR) {
            for (int i = 0; i < 256; ++i)
                dict[i] = QByteArray(1, char(i));
            dictSize = 258;
            codeLen = 9;
            prev.clear();
            continue;
        }
        QByteArray cur;
        if (code < dictSize && !dict[code].isNull())
            cur = dict[code];
        else if (code == dictSize && !prev.isEmpty())
            cur = prev + prev.left(1);
        else
            break;
        dst += cur;
        if (!prev.isEmpty() && dictSize < 4096) {
            dict[dictSize++] = prev + cur.left(1);
            if (dictSize == 511)
                codeLen = 10;
            else if (dictSize == 1023)
                codeLen = 11;
            else if (dictSize == 2047)
                codeLen = 12;
        }
        prev = cur;
    }
    return !dst.isEmpty();
}

#ifdef HAS_ZLIB
bool inflateZlib(const QByteArray& src, QByteArray& dst, int expected)
{
    dst.resize(expected > 0 ? expected : src.size() * 4);
    uLongf outLen = uLongf(dst.size());
    int rc = ::uncompress(reinterpret_cast<Bytef*>(dst.data()), &outLen,
                          reinterpret_cast<const Bytef*>(src.constData()), uLong(src.size()));
    if (rc == Z_OK) {
        dst.resize(int(outLen));
        return true;
    }
    // try raw deflate (some TIFFs)
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -15) != Z_OK)
        return false;
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(src.constData()));
    zs.avail_in = uInt(src.size());
    dst.resize(expected > 0 ? expected : src.size() * 8);
    zs.next_out = reinterpret_cast<Bytef*>(dst.data());
    zs.avail_out = uInt(dst.size());
    rc = ::inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (rc == Z_STREAM_END || rc == Z_OK) {
        dst.resize(int(zs.total_out));
        return true;
    }
    return false;
}
#endif

bool packBitsDecode(const QByteArray& src, QByteArray& dst, int expected)
{
    dst.clear();
    if (expected > 0)
        dst.reserve(expected);
    int i = 0;
    const int n = src.size();
    while (i < n) {
        const qint8 n2 = qint8(src[i++]);
        if (n2 >= 0) {
            const int cnt = n2 + 1;
            if (i + cnt > n)
                return false;
            dst.append(src.constData() + i, cnt);
            i += cnt;
        } else if (n2 != -128) {
            const int cnt = 1 - n2;
            if (i >= n)
                return false;
            dst.append(cnt, src[i++]);
        }
    }
    return true;
}

void applyPredictor(QByteArray& raw, int width, int samples, int bytesPerSample, int rows)
{
    // predictor 2: horizontal differencing
    const int rowBytes = width * samples * bytesPerSample;
    for (int y = 0; y < rows; ++y) {
        char* row = raw.data() + qint64(y) * rowBytes;
        if (bytesPerSample == 1) {
            for (int x = samples; x < width * samples; ++x)
                row[x] = char(quint8(row[x]) + quint8(row[x - samples]));
        } else if (bytesPerSample == 2) {
            quint16* r = reinterpret_cast<quint16*>(row);
            for (int x = samples; x < width * samples; ++x)
                r[x] = quint16(r[x] + r[x - samples]);
        }
    }
}

struct StripPlan {
    bool tiled = false;
    quint32 tileW = 0, tileH = 0;
    quint32 rowsPerStrip = 0;
    std::vector<quint32> offsets, byteCounts;
    quint32 tilesAcross = 0, tilesDown = 0;
};

bool decodeBlock(TiffFile& tf, int compression, int predictor, int width, int height,
                 int samples, int bps, const QByteArray& comp, QByteArray& out)
{
    out.clear();
    const int expect = width * height * samples * (bps / 8);
    switch (compression) {
    case 1:
        out = comp;
        break;
    case 32773:
        if (!packBitsDecode(comp, out, expect))
            return false;
        break;
    case 5:
        if (!lzwDecode(comp, out, 8))
            return false;
        break;
    case 8:
    case 32946:
#ifdef HAS_ZLIB
        if (!inflateZlib(comp, out, expect))
            return false;
#else
        return false;
#endif
        break;
    default:
        return false;
    }
    if (predictor == 2 && bps == 8)
        applyPredictor(out, width, samples, 1, height);
    else if (predictor == 2 && bps == 16)
        applyPredictor(out, width, samples, 2, height);
    Q_UNUSED(tf);
    return out.size() >= expect / 2; // allow slight shortfall
}

void sampleRowToRgb(const char* row, int srcW, int samples, int bps, int photometric,
                    int outW, QRgb* dst)
{
    const int step = std::max(1, srcW / outW);
    for (int x = 0; x < outW; ++x) {
        const int sx = std::min(srcW - 1, int(x * step));
        int r = 0, g = 0, b = 0;
        if (bps == 8) {
            const quint8* p = reinterpret_cast<const quint8*>(row) + sx * samples;
            if (photometric == 1 || samples == 1) {
                r = g = b = p[0];
            } else if (photometric == 0) {
                r = g = b = 255 - p[0];
            } else {
                r = p[0];
                g = samples > 1 ? p[1] : p[0];
                b = samples > 2 ? p[2] : p[0];
            }
        } else if (bps == 16) {
            const quint16* p = reinterpret_cast<const quint16*>(row) + sx * samples;
            auto to8 = [](quint16 v) { return int(std::min(255, int(v >> 8))); };
            if (photometric == 1 || samples == 1)
                r = g = b = to8(p[0]);
            else {
                r = to8(p[0]);
                g = samples > 1 ? to8(p[1]) : r;
                b = samples > 2 ? to8(p[2]) : r;
            }
        }
        dst[x] = qRgb(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
    }
}

bool parseHeader(const QString& path, TiffFile& tf, GeoTiffInfo& info, std::vector<IfdEntry>& ifd,
                 StripPlan& plan)
{
    if (!tf.open(path)) {
        info.error = QStringLiteral("无法打开 TIFF");
        return false;
    }
    if (!tf.readIfd(tf.nextIfd, ifd) || ifd.empty()) {
        info.error = QStringLiteral("无效的 TIFF IFD");
        return false;
    }
    auto findTag = [&](quint16 tag) -> const IfdEntry* {
        for (const auto& e : ifd)
            if (e.tag == tag)
                return &e;
        return nullptr;
    };
    auto u32Of = [&](quint16 tag, quint32 def) {
        const IfdEntry* e = findTag(tag);
        if (!e)
            return def;
        std::vector<double> v;
        if (!tf.entryValues(*e, v) || v.empty())
            return def;
        return quint32(v[0]);
    };

    info.width = int(u32Of(TagImageWidth, 0));
    info.height = int(u32Of(TagImageLength, 0));
    info.samples = int(u32Of(TagSamplesPerPixel, 3));
    info.bitsPerSample = int(u32Of(TagBitsPerSample, 8));
    info.compression = int(u32Of(TagCompression, 1));
    info.photometric = int(u32Of(TagPhotometric, 2));
    const int planar = int(u32Of(TagPlanarConfig, 1));
    const int predictor = int(u32Of(TagPredictor, 1));

    if (info.width <= 0 || info.height <= 0) {
        info.error = QStringLiteral("TIFF 尺寸无效");
        return false;
    }
    if (info.bitsPerSample != 8 && info.bitsPerSample != 16) {
        info.error = QStringLiteral("仅支持 8/16 位 GeoTIFF（当前 %1 位）").arg(info.bitsPerSample);
        return false;
    }
    if (planar != 1) {
        info.error = QStringLiteral("暂不支持 PlanarConfiguration=2");
        return false;
    }

    // strips or tiles
    const IfdEntry* tileOff = findTag(TagTileOffsets);
    if (tileOff) {
        plan.tiled = true;
        plan.tileW = u32Of(TagTileWidth, 256);
        plan.tileH = u32Of(TagTileLength, 256);
        tf.entryU32s(*tileOff, plan.offsets);
        const IfdEntry* tileBc = findTag(TagTileByteCounts);
        if (tileBc)
            tf.entryU32s(*tileBc, plan.byteCounts);
        plan.tilesAcross = (info.width + plan.tileW - 1) / plan.tileW;
        plan.tilesDown = (info.height + plan.tileH - 1) / plan.tileH;
    } else {
        plan.rowsPerStrip = u32Of(TagRowsPerStrip, info.height);
        if (plan.rowsPerStrip == 0)
            plan.rowsPerStrip = info.height;
        const IfdEntry* so = findTag(TagStripOffsets);
        const IfdEntry* sb = findTag(TagStripByteCounts);
        if (!so || !sb) {
            info.error = QStringLiteral("缺少 StripOffsets");
            return false;
        }
        tf.entryU32s(*so, plan.offsets);
        tf.entryU32s(*sb, plan.byteCounts);
    }

    // geo
    const IfdEntry* scale = findTag(TagModelPixelScale);
    const IfdEntry* tie = findTag(TagModelTiepoint);
    const IfdEntry* trans = findTag(TagModelTransformation);
    if (scale && tie) {
        std::vector<double> s, t;
        tf.entryValues(*scale, s);
        tf.entryValues(*tie, t);
        if (s.size() >= 2 && t.size() >= 6) {
            // tie: I,J,K → X,Y,Z ; scale: Xsize, Ysize
            const double xs = s[0], ys = s[1];
            const double x0 = t[3], y0 = t[4];
            const double i0 = t[0], j0 = t[1];
            const double west = x0 - i0 * xs;
            const double north = y0 + j0 * ys;
            const double east = west + info.width * xs;
            const double south = north - info.height * ys;
            info.west = west;
            info.east = east;
            info.south = south;
            info.north = north;
            info.hasGeo = true;
            // if values look like degrees → 4326; if meters → treat as 3857 world
            if (std::abs(west) <= 180.5 && std::abs(east) <= 180.5
                && std::abs(south) <= 90.5 && std::abs(north) <= 90.5) {
                info.crs = QStringLiteral("EPSG:4326");
            } else if (std::abs(west) <= 20037509 && std::abs(east) <= 20037509) {
                // approximate mercator meters → lon/lat
                auto mx2lon = [](double x) { return x / 6378137.0 * 180.0 / M_PI; };
                auto my2lat = [](double y) {
                    return (2.0 * std::atan(std::exp(y / 6378137.0)) - M_PI / 2.0) * 180.0 / M_PI;
                };
                info.west = mx2lon(west);
                info.east = mx2lon(east);
                info.south = my2lat(south);
                info.north = my2lat(north);
                info.crs = QStringLiteral("EPSG:3857");
                if (info.west > info.east)
                    std::swap(info.west, info.east);
                if (info.south > info.north)
                    std::swap(info.south, info.north);
            }
        }
    } else if (trans) {
        std::vector<double> m;
        tf.entryValues(*trans, m);
        if (m.size() >= 16) {
            info.west = m[3];
            info.north = m[7];
            info.east = m[3] + m[0] * info.width;
            info.south = m[7] + m[5] * info.height;
            if (info.south > info.north)
                std::swap(info.south, info.north);
            info.hasGeo = true;
        }
    }
    if (!info.hasGeo) {
        info.west = -180;
        info.east = 180;
        info.south = -90;
        info.north = 90;
        info.crs = QStringLiteral("（无地理标签，按全球）");
    }

    Q_UNUSED(predictor);
    return true;
}

// Iterate source rows via strips/tiles, filling a full-width scanline buffer.
class ScanlineReader
{
public:
    TiffFile* tf = nullptr;
    GeoTiffInfo info;
    StripPlan plan;
    int predictor = 1;
    int curIndex = -1;
    QByteArray block;
    int blockRows = 0;
    int blockY0 = 0;
    int blockW = 0;

    bool init(TiffFile* t, const GeoTiffInfo& gi, const StripPlan& pl, int pred)
    {
        tf = t;
        info = gi;
        plan = pl;
        predictor = pred;
        return !plan.offsets.empty();
    }

    // Get pointer to row y (full width samples). Returns nullptr on failure.
    const char* row(int y)
    {
        if (y < 0 || y >= info.height)
            return nullptr;
        if (!plan.tiled) {
            const quint32 rps = plan.rowsPerStrip ? plan.rowsPerStrip : info.height;
            const int idx = y / int(rps);
            const int yIn = y % int(rps);
            if (idx != curIndex) {
                if (!loadStrip(idx))
                    return nullptr;
                blockY0 = idx * int(rps);
                blockRows = std::min(int(rps), info.height - blockY0);
                blockW = info.width;
                curIndex = idx;
            }
            if (yIn >= blockRows)
                return nullptr;
            const int bpsBytes = info.bitsPerSample / 8;
            const int rowBytes = blockW * info.samples * bpsBytes;
            return block.constData() + qint64(yIn) * rowBytes;
        }
        // tiled
        const int ty = y / int(plan.tileH);
        const int yIn = y % int(plan.tileH);
        // load one tile row-of-tiles lazily per tile
        // simple: load tile at (0,ty) only if width==tileW; else loop tiles and we need full row
        // For preview we assemble full row from tiles
        m_rowBuf.resize(info.width * info.samples * (info.bitsPerSample / 8));
        const int bpsBytes = info.bitsPerSample / 8;
        for (int tx = 0; tx < int(plan.tilesAcross); ++tx) {
            const int idx = ty * int(plan.tilesAcross) + tx;
            if (!loadTile(idx, plan.tileW, plan.tileH))
                continue;
            const int yb = yIn;
            if (yb >= blockRows)
                continue;
            const int x0 = tx * int(plan.tileW);
            const int w = std::min(int(plan.tileW), info.width - x0);
            const int rowBytes = blockW * info.samples * bpsBytes;
            const char* src = block.constData() + qint64(yb) * rowBytes;
            std::memcpy(m_rowBuf.data() + qint64(x0) * info.samples * bpsBytes, src,
                        qint64(w) * info.samples * bpsBytes);
        }
        return m_rowBuf.constData();
    }

private:
    QByteArray m_rowBuf;

    bool loadStrip(int idx)
    {
        if (idx < 0 || idx >= int(plan.offsets.size()))
            return false;
        const qint64 off = plan.offsets[idx];
        const qint64 len = idx < int(plan.byteCounts.size()) ? plan.byteCounts[idx] : 0;
        if (!tf->f.seek(off))
            return false;
        QByteArray comp = tf->f.read(len > 0 ? len : 1);
        const quint32 rps = plan.rowsPerStrip ? plan.rowsPerStrip : info.height;
        const int y0 = idx * int(rps);
        const int rows = std::min(int(rps), info.height - y0);
        return decodeBlock(*tf, info.compression, predictor, info.width, rows, info.samples,
                           info.bitsPerSample, comp, block);
    }

    bool loadTile(int idx, int tw, int th)
    {
        if (idx < 0 || idx >= int(plan.offsets.size()))
            return false;
        const qint64 off = plan.offsets[idx];
        const qint64 len = idx < int(plan.byteCounts.size()) ? plan.byteCounts[idx] : 0;
        if (!tf->f.seek(off))
            return false;
        QByteArray comp = tf->f.read(len > 0 ? len : 1);
        return decodeBlock(*tf, info.compression, predictor, tw, th, info.samples,
                           info.bitsPerSample, comp, block);
    }
};

} // namespace

bool readGeoTiffInfo(const QString& path, GeoTiffInfo& info)
{
    TiffFile tf;
    std::vector<IfdEntry> ifd;
    StripPlan plan;
    return parseHeader(path, tf, info, ifd, plan);
}

bool readGeoTiffPreview(const QString& path, int maxDim, QImage& out,
                        double& west, double& east, double& south, double& north,
                        GeoTiffInfo& info)
{
    TiffFile tf;
    std::vector<IfdEntry> ifd;
    StripPlan plan;
    if (!parseHeader(path, tf, info, ifd, plan))
        return false;
    auto findTag = [&](quint16 tag) -> const IfdEntry* {
        for (const auto& e : ifd)
            if (e.tag == tag)
                return &e;
        return nullptr;
    };
    int predictor = 1;
    if (const IfdEntry* e = findTag(TagPredictor)) {
        std::vector<double> v;
        if (tf.entryValues(*e, v) && !v.empty())
            predictor = int(v[0]);
    }

    const int srcW = info.width, srcH = info.height;
    const int outW = std::max(1, std::min(srcW, maxDim));
    const int outH = std::max(1, std::min(srcH, maxDim * srcH / std::max(1, srcW)));
    const int stepY = std::max(1, srcH / outH);

    out = QImage(outW, outH, QImage::Format_RGB32);
    out.fill(Qt::darkGray);

    ScanlineReader rd;
    if (!rd.init(&tf, info, plan, predictor)) {
        info.error = QStringLiteral("无法初始化 TIFF 扫描读取");
        return false;
    }

    for (int y = 0; y < outH; ++y) {
        const int sy = std::min(srcH - 1, y * stepY);
        const char* row = rd.row(sy);
        if (!row) {
            // try nearby rows
            continue;
        }
        sampleRowToRgb(row, srcW, info.samples, info.bitsPerSample, info.photometric, outW,
                       reinterpret_cast<QRgb*>(out.scanLine(y)));
    }

    west = info.west;
    east = info.east;
    south = info.south;
    north = info.north;
    return true;
}

bool buildTilePyramidNative(const QString& srcTif, const QString& outDir,
                            const std::function<void(int, int, int)>& progress)
{
    GeoTiffInfo info;
    TiffFile tf;
    std::vector<IfdEntry> ifd;
    StripPlan plan;
    if (!parseHeader(srcTif, tf, info, ifd, plan))
        return false;
    int predictor = 1;
    for (const auto& e : ifd) {
        if (e.tag == TagPredictor) {
            std::vector<double> v;
            if (tf.entryValues(e, v) && !v.empty())
                predictor = int(v[0]);
        }
    }

    const int maxDim = std::max(info.width, info.height);
    int zmax = 0;
    while (zmax < 6 && (1 << zmax) * 256 < maxDim)
        ++zmax;

    QDir().mkpath(outDir);
    int nTiles = 0;

    for (int z = 0; z <= zmax; ++z) {
        const int decim = 1 << (zmax - z);
        const int lw = std::max(1, info.width / decim);
        const int lh = std::max(1, info.height / decim);
        const int nx = (lw + 255) / 256;
        const int ny = (lh + 255) / 256;

        // one output tile-row at a time (bounded memory even for 900MB sources)
        for (int ty = 0; ty < ny; ++ty) {
            const int ly0 = ty * 256;
            const int th = std::min(256, lh - ly0);
            QImage band(lw, th, QImage::Format_RGB32);
            band.fill(Qt::black);
            {
                ScanlineReader rd;
                rd.init(&tf, info, plan, predictor);
                for (int ly = 0; ly < th; ++ly) {
                    const int sy = std::min(info.height - 1, (ly0 + ly) * decim);
                    const char* row = rd.row(sy);
                    if (!row)
                        continue;
                    sampleRowToRgb(row, info.width, info.samples, info.bitsPerSample,
                                   info.photometric, lw,
                                   reinterpret_cast<QRgb*>(band.scanLine(ly)));
                }
            }
            for (int tx = 0; tx < nx; ++tx) {
                const int sx = tx * 256;
                const int tw = std::min(256, lw - sx);
                QImage tile = band.copy(sx, 0, tw, th);
                const QString dir = QStringLiteral("%1/%2/%3").arg(outDir).arg(z).arg(tx);
                QDir().mkpath(dir);
                tile.save(dir + QStringLiteral("/%1.png").arg(ty), "PNG");
                ++nTiles;
            }
        }
        if (progress)
            progress(z, zmax, nTiles);
    }

    QJsonObject o;
    o[QStringLiteral("west")] = info.west;
    o[QStringLiteral("east")] = info.east;
    o[QStringLiteral("south")] = info.south;
    o[QStringLiteral("north")] = info.north;
    o[QStringLiteral("zmax")] = zmax;
    o[QStringLiteral("width")] = info.width;
    o[QStringLiteral("height")] = info.height;
    o[QStringLiteral("tiles")] = nTiles;
    QFile mf(outDir + QStringLiteral("/meta.json"));
    if (mf.open(QIODevice::WriteOnly))
        mf.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    return true;
}

} // namespace data
