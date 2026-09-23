#include "core/I18n.h"
#include "data/GeoTiffLite.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QImage>

int main(int argc, char** argv)
{
    // Headless helpers (no GUI) — used to verify native GeoTIFF support
    if (argc >= 4) {
        const QString mode = QString::fromLocal8Bit(argv[1]);
        if (mode == QLatin1String("--tif-preview")) {
            QCoreApplication app(argc, argv);
            QImage img;
            double w, e, s, n;
            data::GeoTiffInfo gi;
            const bool ok = data::readGeoTiffPreview(QString::fromLocal8Bit(argv[2]), 2048, img, w, e,
                                                     s, n, gi);
            if (!ok) {
                fprintf(stderr, "FAIL info=%s\n", qPrintable(gi.error));
                return 1;
            }
            if (!img.save(QString::fromLocal8Bit(argv[3]))) {
                fprintf(stderr, "FAIL save png\n");
                return 2;
            }
            fprintf(stdout,
                    "OK %dx%d samples=%d comp=%d geo=%d crs=%s bounds=%.6f,%.6f,%.6f,%.6f png=%s\n",
                    gi.width, gi.height, gi.samples, gi.compression, int(gi.hasGeo),
                    qPrintable(gi.crs), w, s, e, n, argv[3]);
            return 0;
        }
        if (mode == QLatin1String("--tif-pyramid")) {
            QCoreApplication app(argc, argv);
            const bool ok = data::buildTilePyramidNative(
                QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]),
                [](int z, int zmax, int tiles) {
                    fprintf(stdout, "level %d/%d tiles=%d\n", z + 1, zmax + 1, tiles);
                    fflush(stdout);
                });
            return ok ? 0 : 3;
        }
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("GEDIProcessDesktopCpp"));
    QApplication::setOrganizationName(QStringLiteral("GEDIProcessDesktop"));
    // placeholder logo — replace cpp/assets/app.ico (+ app.png) later
    QIcon icon(QStringLiteral(":/icons/app.ico"));
    if (icon.isNull()) {
        const QString appDir = QCoreApplication::applicationDirPath();
        icon = QIcon(appDir + QStringLiteral("/assets/app.ico"));
        if (icon.isNull())
            icon = QIcon(appDir + QStringLiteral("/../assets/app.ico"));
    }
    if (!icon.isNull())
        QApplication::setWindowIcon(icon);
    ui::MainWindow w;
    w.show();
    return app.exec();
}
