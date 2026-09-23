#include "data/RasterInfo.h"

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
    const QString env = qEnvironmentVariable("GEDI_PYTHON");
    if (!env.isEmpty() && QFileInfo::exists(env))
        return env;
    return QStandardPaths::findExecutable(QStringLiteral("python"));
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
    const QString py = anacondaPython();
    if (py.isEmpty()) {
        err = QStringLiteral("找不到 Python（用于解码 GeoTIFF）");
        return false;
    }
    const QString script = converterScript();
    if (script.isEmpty()) {
        err = QStringLiteral("找不到 tif_to_png.py（tools 目录）");
        return false;
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

    QProcess pr;
    pr.start(py, {script, src, pngOut});
    if (!pr.waitForStarted(8000)) {
        err = QStringLiteral("无法启动 Python 转换 GeoTIFF: ").arg(py);
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
        if (!convertGeoTiffToPreview(path, png, meta, err))
            return QImage();
        QImage img(png);
        if (img.isNull() || (img.width() <= maxDim && img.height() <= maxDim))
            return img;
        return img.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QImage img(path);
    if (img.isNull())
        return img;
    if (img.width() <= maxDim && img.height() <= maxDim)
        return img;
    return img.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace data
