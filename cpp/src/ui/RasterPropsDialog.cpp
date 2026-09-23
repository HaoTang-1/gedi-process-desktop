#include "ui/RasterPropsDialog.h"
#include "data/RasterInfo.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ui {

RasterPropsDialog::RasterPropsDialog(const QString& path, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Raster Properties / 栅格属性"));
    resize(640, 460);
    auto info = data::inspectRaster(path, false);
    auto* lay = new QVBoxLayout(this);
    auto* t = new QTableWidget(0, 2, this);
    t->setHorizontalHeaderLabels({QStringLiteral("Property 属性"), QStringLiteral("Value 值")});
    t->horizontalHeader()->setStretchLastSection(true);
    QStringList rows;
    rows << "path" << info.path
         << "size(MB)" << QString::number(info.sizeBytes / 1e6, 'f', 3)
         << "driver" << info.driver
         << "width×height" << QStringLiteral("%1×%2").arg(info.width).arg(info.height)
         << "bands" << QString::number(info.bands)
         << "crs" << info.crs
         << "金字塔 pyramid" << (info.hasPyramid ? QStringLiteral("有 / YES") : QStringLiteral("无 / NO — 建议 gdaladdo -r average x.tif 2 4 8 16"))
         << "overview" << info.overview
         << "error" << info.error;
    t->setRowCount(rows.size() / 2);
    for (int i = 0; i < rows.size() / 2; ++i) {
        t->setItem(i, 0, new QTableWidgetItem(rows[i * 2]));
        t->setItem(i, 1, new QTableWidgetItem(rows[i * 2 + 1]));
    }
    lay->addWidget(t);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

} // namespace ui
