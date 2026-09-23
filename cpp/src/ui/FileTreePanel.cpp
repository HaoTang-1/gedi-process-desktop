#include "ui/FileTreePanel.h"
#include "core/I18n.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace ui {

FileTreePanel::FileTreePanel(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* header = new QLabel(i18n::t("panel_db"), this);
    header->setStyleSheet(QStringLiteral("color:#0B3D91;font-weight:600;"));
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({i18n::t("panel_db"), QStringLiteral("Type"), QStringLiteral("N")});
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &FileTreePanel::onContextMenu);
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* it, int col) {
        if (col != 0)
            return;
        auto m = it->data(0, Qt::UserRole).toMap();
        if (m.value("k") == QLatin1String("r")) {
            int id = m.value("id").toInt();
            bool vis = it->checkState(0) == Qt::Checked;
            emit rasterVisibilityChanged(id, vis);
        }
        emit visibilityChanged();
    });
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        auto items = m_tree->selectedItems();
        if (items.isEmpty())
            return;
        auto* it = items.first();
        if (it->parent() == nullptr)
            return;
        auto data = it->data(0, Qt::UserRole).toMap();
        if (data.value("k") == QLatin1String("g"))
            emit gediSelected(data.value("id").toInt());
        else if (data.value("k") == QLatin1String("r"))
            emit rasterSelected(data.value("id").toInt());
    });
    m_hint = new QLabel(i18n::t("hint_check") + QStringLiteral(" · ") + i18n::t("hint_right_delete"), this);
    m_hint->setStyleSheet(QStringLiteral("color:#5B6B7C;font-size:11px;"));
    lay->addWidget(header);
    lay->addWidget(m_tree, 1);
    lay->addWidget(m_hint);

    auto addCat = [&](const QString& name, QTreeWidgetItem** out) {
        auto* c = new QTreeWidgetItem();
        c->setText(0, name);
        c->setFlags(Qt::ItemIsEnabled);
        m_tree->addTopLevelItem(c);
        *out = c;
    };
    addCat(QStringLiteral("GEDI L1B"), &catL1b);
    addCat(QStringLiteral("GEDI L2A"), &catL2a);
    addCat(i18n::t("layer_cat_raster"), &catRaster);
}

void FileTreePanel::retranslate()
{
    m_tree->setHeaderLabels({i18n::t("panel_db"), QStringLiteral("Type"), QStringLiteral("N")});
    m_hint->setText(i18n::t("hint_check") + QStringLiteral(" · ") + i18n::t("hint_right_delete"));
    if (catRaster)
        catRaster->setText(0, i18n::t("layer_cat_raster"));
}

void FileTreePanel::addGedi(const data::GediFile& f)
{
    m_files.insert(f.fileId, f);
    auto* it = new QTreeWidgetItem();
    it->setText(0, QFileInfo(f.path).fileName());
    it->setText(1, data::productName(f.product));
    it->setText(2, QString::number(f.nShots));
    QVariantMap m;
    m.insert("k", "g");
    m.insert("id", f.fileId);
    it->setData(0, Qt::UserRole, m);
    it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
    it->setCheckState(0, Qt::Checked);
    (f.product == data::Product::L1B ? catL1b : catL2a)->addChild(it);
}

void FileTreePanel::removeGedi(int fileId)
{
    m_files.remove(fileId);
    for (auto* cat : {catL1b, catL2a}) {
        for (int i = 0; i < cat->childCount(); ++i) {
            if (cat->child(i)->data(0, Qt::UserRole).toMap().value("id").toInt() == fileId) {
                delete cat->takeChild(i);
                break;
            }
        }
    }
    emit visibilityChanged();
}

void FileTreePanel::clearAll()
{
    m_files.clear();
    m_rasters.clear();
    for (auto* cat : {catL1b, catL2a, catRaster}) {
        while (cat->childCount())
            delete cat->takeChild(0);
    }
    emit visibilityChanged();
}

void FileTreePanel::addRaster(const RasterLayer& layer)
{
    m_rasters.insert(layer.id, layer);
    auto* it = new QTreeWidgetItem();
    it->setText(0, layer.name);
    it->setText(1, QStringLiteral("Raster"));
    it->setText(2, layer.isOnline ? QStringLiteral("tiles") : QStringLiteral("local"));
    QVariantMap m;
    m.insert("k", "r");
    m.insert("id", layer.id);
    it->setData(0, Qt::UserRole, m);
    it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
    it->setCheckState(0, layer.visible ? Qt::Checked : Qt::Unchecked);
    if (layer.isOnline)
        catRaster->insertChild(0, it);
    else
        catRaster->addChild(it);
}

void FileTreePanel::removeRaster(int id)
{
    m_rasters.remove(id);
    for (int i = 0; i < catRaster->childCount(); ++i) {
        if (catRaster->child(i)->data(0, Qt::UserRole).toMap().value("id").toInt() == id) {
            delete catRaster->takeChild(i);
            break;
        }
    }
    emit visibilityChanged();
}

QSet<int> FileTreePanel::checkedGediIds() const
{
    QSet<int> s;
    for (auto* cat : {catL1b, catL2a}) {
        for (int i = 0; i < cat->childCount(); ++i) {
            auto* it = cat->child(i);
            if (it->checkState(0) == Qt::Checked)
                s.insert(it->data(0, Qt::UserRole).toMap().value("id").toInt());
        }
    }
    return s;
}

QList<int> FileTreePanel::checkedRasterIds() const
{
    QList<int> s;
    for (int i = 0; i < catRaster->childCount(); ++i) {
        auto* it = catRaster->child(i);
        if (it->checkState(0) == Qt::Checked)
            s << it->data(0, Qt::UserRole).toMap().value("id").toInt();
    }
    return s;
}

void FileTreePanel::setAllChecked(bool on)
{
    const auto st = on ? Qt::Checked : Qt::Unchecked;
    for (auto* cat : {catL1b, catL2a, catRaster})
        for (int i = 0; i < cat->childCount(); ++i)
            cat->child(i)->setCheckState(0, st);
    emit visibilityChanged();
}

void FileTreePanel::onContextMenu(const QPoint& pos)
{
    auto* item = m_tree->itemAt(pos);
    if (!item)
        return;
    auto* target = item->parent() ? item : nullptr;
    if (!target)
        target = item;
    auto m = target->data(0, Qt::UserRole).toMap();
    if (m.isEmpty() && item->parent())
        m = item->parent()->data(0, Qt::UserRole).toMap();
    QMenu menu(this);
    const QString k = m.value("k").toString();
    int id = m.value("id").toInt();
    if (k == QLatin1String("g")) {
        menu.addAction(i18n::t("act_hdf5_viewer"), this, [this, id] { emit hdf5ViewRequested(id); });
        menu.addAction(i18n::t("act_hdfview"), this, [this, id] { emit hdfviewRequested(id); });
        menu.addAction(i18n::t("act_h5dump"), this, [this, id] { emit h5dumpRequested(id); });
        menu.addSeparator();
        menu.addAction(i18n::t("ctx_delete"), this, [this, id] { emit removeGediRequested(id); });
    } else if (k == QLatin1String("r")) {
        menu.addAction(i18n::t("ctx_raster_props"), this, [this, id] { emit rasterPropsRequested(id); });
        menu.addAction(i18n::t("ctx_zoom_layer"), this, [this, id] { emit rasterZoomRequested(id); });
        menu.addSeparator();
        menu.addAction(i18n::t("ctx_delete"), this, [this, id] { emit removeRasterRequested(id); });
    }
    menu.addSeparator();
    menu.addAction(i18n::t("ctx_check_all"), this, [this] { setAllChecked(true); });
    menu.addAction(i18n::t("ctx_uncheck_all"), this, [this] { setAllChecked(false); });
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

} // namespace ui
