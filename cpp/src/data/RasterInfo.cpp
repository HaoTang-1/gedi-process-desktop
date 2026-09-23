#include "data/RasterInfo.h"
#include "data/GeoTiffLite.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace data {

QString findPython()
{
    // 1) explicit override
    const QString env = qEnvironmentVariable("GEDI_PYTHON");
    if (!env.isEmpty() && QFileInfo::exists(env))
        return env;

    // 2) PATH
    for (const char* name : {"python", "python3", "python.exe"}) {
        const QString p = QStandardPaths::findExecutable(QString::fromLatin1(name));
        if (!p.isEmpty())
            return p;
    }

#ifdef Q_OS_WIN
    // 3) Windows py launcher (resolves installed CPython)
    const QString pyLauncher = QStandardPaths::findExecutable(QStringLiteral("py"));
    if (!pyLauncher.isEmpty()) {
        QProcess pr;
        pr.start(pyLauncher, {QStringLiteral("-3"), QStringLiteral("-c"),
                              QStringLiteral("import sys; print(sys.executable)")});
        if (pr.waitForFinished(5000)) {
            const QString out = QString::fromLocal8Bit(pr.readAllStandardOutput()).trimmed();
            if (!out.isEmpty() && QFileInfo::exists(out))
                return out;
        }
    }
#endif

    // 4) common install prefixes (portable + Anaconda/Miniconda)
    QStringList cands;
    const QStringList roots = {
        QStringLiteral("C:/"),
        QStringLiteral("D:/"),
        QStringLiteral("E:/"),
        QStringLiteral("F:/"),
        qEnvironmentVariable("USERPROFILE"),
        qEnvironmentVariable("LOCALAPPDATA"),
        qEnvironmentVariable("PROGRAMDATA"),
    };
    for (const QString& root : roots) {
        if (root.isEmpty())
            continue;
        const QString r = QDir::toNativeSeparators(root);
        cands << r + QStringLiteral("software/Anaconda/python.exe");
        cands << r + QStringLiteral("software/Anaconda/envs/gedi_desktop/python.exe");
        cands << r + QStringLiteral("Anaconda3/python.exe");
        cands << r + QStringLiteral("Miniconda3/python.exe");
        cands << r + QStringLiteral("ProgramData/Anaconda3/python.exe");
        cands << r + QStringLiteral("ProgramData/Miniconda3/python.exe");
        cands << r + QStringLiteral("Python/Python311/python.exe");
        cands << r + QStringLiteral("Python/Python312/python.exe");
        cands << r + QStringLiteral("Python/Python310/python.exe");
        cands << r + QStringLiteral("Users/") + qEnvironmentVariable("USERNAME")
                + QStringLiteral("/anaconda3/python.exe");
        cands << r + QStringLiteral("Users/") + qEnvironmentVariable("USERNAME")
                + QStringLiteral("/AppData/Local/Programs/Python/Python311/python.exe");
        cands << r + QStringLiteral("Users/") + qEnvironmentVariable("USERNAME")
                + QStringLiteral("/AppData/Local/Programs/Python/Python312/python.exe");
    }
    for (const QString& c : cands) {
        if (!c.isEmpty() && QFileInfo::exists(c))
            return c;
    }
    return {};
}

QString findToolScript(const QString& fileName)
{
    const QString env = qEnvironmentVariable("GEDI_TOOLS_DIR");
    if (!env.isEmpty()) {
        const QString p = env + QLatin1Char('/') + fileName;
        if (QFileInfo::exists(p))
            return p;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList cands = {
        appDir + QStringLiteral("/tools/") + fileName,
        appDir + QStringLiteral("/../tools/") + fileName,
        appDir + QStringLiteral("/../../tools/") + fileName,
        appDir + QStringLiteral("/../../cpp/tools/") + fileName,
    };
    for (const QString& c : cands)
        if (QFileInfo::exists(c))
            return QDir(c).absolutePath();
    return {};
}

QString findSystemTool(const QString& name)
{
    const QString onPath = QStandardPaths::findExecutable(name);
    if (!onPath.isEmpty())
        return onPath;
    const QString env = qEnvironmentVariable("GEDI_HDF5_BIN");
    if (!env.isEmpty()) {
        const QString p = env + QLatin1Char('/') + name + QStringLiteral(".exe");
        if (QFileInfo::exists(p))
            return p;
        const QString p2 = env + QLatin1Char('/') + name;
        if (QFileInfo::exists(p2))
            return p2;
    }
#ifdef Q_OS_WIN
    const QString win = QStandardPaths::findExecutable(name + QStringLiteral(".exe"));
    if (!win.isEmpty())
        return win;
#endif
    return {};
}

QString sampleDataDir()
{
    const QString env = qEnvironmentVariable("GEDI_SAMPLE_DIR");
    if (!env.isEmpty() && QFileInfo::exists(env))
        return env;
    const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/../data_sample");
    if (QFileInfo::exists(local))
        return QDir(local).absolutePath();
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

static QString anacondaPython()
{
    return findPython();
}

static QString converterScript()
{
    return findToolScript(QStringLiteral("tif_to_png.py"));
}

bool convertGeoTiffToPreview(const QString& src, QString& pngOut, QString& metaJson, QString& err)
{
    // PNG/JPG/BMP: no Python needed
    const QString suffix = QFileInfo(src).suffix().toLower();
    const bool maybeTiff = (suffix == QLatin1String("tif") || suffix == QLatin1String("tiff")
                            || suffix == QLatin1String("geotiff") || suffix == QLatin1String("jp2"));
    if (!maybeTiff) {
        QImage img(src);
        if (img.isNull()) {
            err = QStringLiteral("无法解码该影像文件");
            return false;
        }
        pngOut = src;
        metaJson.clear();
        return true;
    }

    QFileInfo fi(src);
    QDir cache = fi.absoluteDir().filePath(QStringLiteral(".gedi_preview"));
    if (!cache.exists())
        cache.mkpath(QStringLiteral("."));
    pngOut = cache.filePath(fi.completeBaseName() + QStringLiteral(".preview.png"));
    metaJson = cache.filePath(fi.completeBaseName() + QStringLiteral(".preview.bounds.json"));

    // reuse cache
    if (QFileInfo::exists(pngOut) && QFileInfo::exists(metaJson)) {
        return true;
    }

    // 1) native C++ (no Python)
    {
        QImage img;
        double w, e, s, n;
        GeoTiffInfo gi;
        if (readGeoTiffPreview(src, 2048, img, w, e, s, n, gi) && !img.isNull()) {
            if (!img.save(pngOut, "PNG")) {
                err = QStringLiteral("写入预览 PNG 失败");
                return false;
            }
            QJsonObject o;
            o[QStringLiteral("west")] = w;
            o[QStringLiteral("east")] = e;
            o[QStringLiteral("south")] = s;
            o[QStringLiteral("north")] = n;
            o[QStringLiteral("width")] = gi.width;
            o[QStringLiteral("height")] = gi.height;
            o[QStringLiteral("bands")] = gi.samples;
            o[QStringLiteral("crs")] = gi.crs;
            o[QStringLiteral("driver")] = QStringLiteral("GeoTiffLite (native C++)");
            o[QStringLiteral("has_pyramid")] = false;
            o[QStringLiteral("overviews")] = QJsonArray();
            QFile mf(metaJson);
            if (mf.open(QIODevice::WriteOnly))
                mf.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
            return true;
        }
    }

    // 2) optional Python / rasterio
    const QString py = anacondaPython();
    if (py.isEmpty()) {
        err = QStringLiteral(
            "无法在本程序内解码该 GeoTIFF，且未找到 Python。\n"
            "原生支持：未压缩 / PackBits / LZW / Deflate 的 8/16 位。\n"
            "如需其它压缩，请安装 Python 并: pip install rasterio numpy pillow\n"
            "或设置环境变量 GEDI_PYTHON。");
        return false;
    }
    const QString script = converterScript();
    if (script.isEmpty()) {
        err = QStringLiteral(
            "找不到 tif_to_png.py（tools 目录）\n"
            "请将 tools\\*.py 放在程序目录 tools\\ 下，"
            "或设置环境变量 GEDI_TOOLS_DIR。");
        return false;
    }

    QProcess pr;
    pr.start(py, {script, src, pngOut});
    if (!pr.waitForStarted(8000)) {
        err = QStringLiteral("无法启动 Python 转换 GeoTIFF:\n%1").arg(py);
        return false;
    }
    if (!pr.waitForFinished(180000)) {
        pr.kill();
        err = QStringLiteral("GeoTIFF 转换超时（大文件请先建金字塔或转 PNG）");
        return false;
    }
    const QString errOut = QString::fromUtf8(pr.readAllStandardError());
    if (!QFileInfo::exists(pngOut)) {
        err = errOut.isEmpty() ? QStringLiteral("转换失败：未生成 PNG") : errOut;
        return false;
    }
    return true;
}

static bool isTiffName(const QString& path)
{
    const QString s = QFileInfo(path).suffix().toLower();
    return s == QLatin1String("tif") || s == QLatin1String("tiff")
        || s == QLatin1String("geotiff") || s == QLatin1String("jp2");
}

RasterInfo inspectRaster(const QString& path, bool tryPyramid)
{
    Q_UNUSED(tryPyramid);
    RasterInfo info;
    info.path = path;
    QFileInfo fi(path);
    info.sizeBytes = fi.size();
    if (QFileInfo::exists(path + QStringLiteral(".ovr")))
        info.hasPyramid = true;

    if (isTiffName(path)) {
        QString png, meta, err;
        if (!convertGeoTiffToPreview(path, png, meta, err)) {
            info.error = err;
            return info;
        }
        info.driver = QStringLiteral("GeoTIFF → preview (rasterio / EPSG:4326)");
        QImage img(png);
        info.width = img.width();
        info.height = img.height();
        info.bands = img.isGrayscale() ? 1 : 3;
        if (QFileInfo::exists(meta)) {
            QFile f(meta);
            if (f.open(QIODevice::ReadOnly)) {
                QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
                info.west = o.value(QStringLiteral("west")).toDouble();
                info.east = o.value(QStringLiteral("east")).toDouble();
                info.south = o.value(QStringLiteral("south")).toDouble();
                info.north = o.value(QStringLiteral("north")).toDouble();
                info.crs = o.value(QStringLiteral("crs")).toString();
                info.hasPyramid = o.value(QStringLiteral("has_pyramid")).toBool() || info.hasPyramid;
                QJsonArray ov = o.value(QStringLiteral("overviews")).toArray();
                QStringList lv;
                for (const auto& v : ov)
                    lv << QString::number(v.toInt());
                info.overview = lv.isEmpty() ? QStringLiteral("(none)") : lv.join(QLatin1Char(','));
                info.width = o.value(QStringLiteral("width")).toInt(info.width);
                info.height = o.value(QStringLiteral("height")).toInt(info.height);
                info.bands = o.value(QStringLiteral("bands")).toInt(info.bands);
            }
        }
        return info;
    }

    QImage img(path);
    if (img.isNull()) {
        info.error = QStringLiteral("无法解码该影像文件");
        return info;
    }
    info.width = img.width();
    info.height = img.height();
    info.bands = img.isGrayscale() ? 1 : 3;
    info.driver = QStringLiteral("QImage");
    info.crs = QStringLiteral("（图片，默认按 EPSG:4326 全球范围显示）");
    info.west = -180;
    info.east = 180;
    info.south = -90;
    info.north = 90;
    return info;
}

QImage loadRasterPreview(const QString& path, int maxDim)
{
    if (maxDim <= 0)
        maxDim = 1600;
    if (isTiffName(path)) {
        QString png, meta, err;
        if (convertGeoTiffToPreview(path, png, meta, err)) {
            QImage img(png);
            if (img.isNull() || (img.width() <= maxDim && img.height() <= maxDim))
                return img;
            return img.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        QImage img;
        double w, e, s, n;
        GeoTiffInfo gi;
        if (readGeoTiffPreview(path, maxDim, img, w, e, s, n, gi) && !img.isNull())
            return img;
        return QImage();
    }
    QImage img(path);
    if (img.isNull())
        return img;
    if (img.width() <= maxDim && img.height() <= maxDim)
        return img;
    return img.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

bool buildTilePyramid(const QString& srcTif, const QString& outDir,
                      const std::function<void(int, int, QString)>& progress)
{
    const bool okNative = buildTilePyramidNative(
        srcTif, outDir, [&](int z, int zmax, int tiles) {
            if (progress)
                progress(z, zmax,
                         QStringLiteral("C++ 金字塔 层 %1/%2  瓦片 %3")
                             .arg(z + 1)
                             .arg(zmax + 1)
                             .arg(tiles));
        });
    if (okNative)
        return true;

    const QString py = findPython();
    const QString script = findToolScript(QStringLiteral("build_tile_pyramid.py"));
    if (py.isEmpty() || script.isEmpty())
        return false;
    QProcess pr;
    pr.start(py, {script, srcTif, outDir});
    if (!pr.waitForStarted(8000))
        return false;
    while (!pr.waitForFinished(200)) {
        while (pr.canReadLine()) {
            const QString line = QString::fromUtf8(pr.readLine());
            if (progress && line.startsWith(QLatin1String("level"))) {
                const int lv = line.section(QLatin1Char(' '), 1, 1).toInt();
                progress(lv, 6, line.trimmed());
            }
        }
    }
    return QFileInfo::exists(outDir + QStringLiteral("/meta.json"));
}

} // namespace data
