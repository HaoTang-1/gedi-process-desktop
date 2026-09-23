#include "ui/Hdf5ViewerDialog.h"
#include "core/I18n.h"
#include "data/FieldDocs.h"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace ui {

Hdf5ViewerDialog::Hdf5ViewerDialog(const QString& path, QWidget* parent) : QDialog(parent), m_path(path)
{
    setWindowTitle(i18n::t("act_hdf5_viewer") + QStringLiteral(" — ") + QFileInfo(path).fileName());
    resize(1000, 700);
    auto* lay = new QVBoxLayout(this);
    auto* split = new QSplitter(Qt::Horizontal, this);
    m_tree = new QTreeWidget(split);
    m_tree->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Type"), QStringLiteral("Shape")});
    auto* right = new QWidget(split);
    auto* rl = new QVBoxLayout(right);
    m_doc = new QLabel(right);
    m_doc->setWordWrap(true);
    m_tbl = new QTableWidget(0, 2, right);
    m_tbl->setHorizontalHeaderLabels({QStringLiteral("Property"), QStringLiteral("Value")});
    m_tbl->horizontalHeader()->setStretchLastSection(true);
    rl->addWidget(m_doc);
    rl->addWidget(m_tbl, 1);
    split->addWidget(m_tree);
    split->addWidget(right);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    lay->addWidget(split, 1);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(bb, &QDialogButtonBox::clicked, this, &QDialog::accept);
    lay->addWidget(bb);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &Hdf5ViewerDialog::onSelect);
    loadTree();
}

Hdf5ViewerDialog::~Hdf5ViewerDialog()
{
    if (m_file > 0)
        h5::fileClose(m_file);
}

void Hdf5ViewerDialog::loadTree()
{
    if (!h5::ensureLoaded()) {
        m_doc->setText(h5::errorString());
        return;
    }
    if (!h5::fileOpen(m_path, m_file)) {
        m_doc->setText(QStringLiteral("Open failed: ") + h5::errorString());
        return;
    }
    auto* root = new QTreeWidgetItem();
    root->setText(0, QFileInfo(m_path).fileName());
    root->setText(1, QStringLiteral("File"));
    QVariantMap rm;
    rm.insert("k", "g");
    rm.insert("p", "/");
    root->setData(0, Qt::UserRole, rm);
    m_tree->addTopLevelItem(root);
    fillGroup(m_file, QStringLiteral("/"), root);
    root->setExpanded(true);
    m_tree->collapseAll();
    root->setExpanded(true);
}

void Hdf5ViewerDialog::fillGroup(long long file, const QString& path, QTreeWidgetItem* parent)
{
    QStringList names;
    QList<int> isGrp;
    h5::listNames(file, path, names, isGrp);
    for (int i = 0; i < names.size(); ++i) {
        auto* it = new QTreeWidgetItem();
        it->setText(0, names[i]);
        QString full = (path == QLatin1String("/") ? QString() : path) + QLatin1Char('/') + names[i];
        QVariantMap m;
        if (isGrp.value(i) == 0) {
            QVector<qint64> dims;
            int cls = 0, sz = 0;
            h5::datasetInfo(file, full, dims, cls, sz);
            QStringList ds;
            for (auto d : dims)
                ds << QString::number(d);
            it->setText(1, QStringLiteral("Dataset"));
            it->setText(2, ds.join('x'));
            m.insert("k", "d");
            m.insert("p", full);
        } else {
            it->setText(1, QStringLiteral("Group"));
            m.insert("k", "g");
            m.insert("p", full);
        }
        it->setData(0, Qt::UserRole, m);
        parent->addChild(it);
        if (m.value("k") == QLatin1String("g"))
            fillGroup(file, full, it);
    }
}

void Hdf5ViewerDialog::onSelect(QTreeWidgetItem* cur, QTreeWidgetItem*)
{
    if (!cur)
        return;
    auto m = cur->data(0, Qt::UserRole).toMap();
    QString p = m.value("p").toString();
    m_doc->setText(data::fieldDoc(p));
    m_tbl->setRowCount(0);
    if (m.value("k") == QLatin1String("d")) {
        QVector<qint64> dims;
        int cls = 0, sz = 0;
        if (!h5::datasetInfo(m_file, p, dims, cls, sz))
            return;
        QStringList ds;
        for (auto d : dims)
            ds << QString::number(d);
        QStringList rows;
        rows << QStringLiteral("path") << p;
        rows << QStringLiteral("shape") << ds.join('x');
        rows << QStringLiteral("dtype") << (cls == 1 ? QStringLiteral("float") : QStringLiteral("int"))
                 + QStringLiteral(" size=%1").arg(sz);
        qint64 n = 1;
        for (auto d : dims)
            n *= d;
        rows << QStringLiteral("n_elements") << QString::number(n);
        if (!data::fieldDoc(p).isEmpty())
            rows << QStringLiteral("说明") << data::fieldDoc(p);

        std::vector<double> data;
        QVector<qint64> dd;
        if (n > 0 && n < 200000000 && h5::readAsDouble(m_file, p, data, dd)) {
            double mn = data.empty() ? 0.0 : data[0], mx = mn, sum = 0;
            int finite = 0;
            for (double v : data) {
                if (!std::isfinite(v))
                    continue;
                mn = std::min(mn, v);
                mx = std::max(mx, v);
                sum += v;
                ++finite;
            }
            if (finite > 0) {
                rows << QStringLiteral("min") << QString::number(mn, 'g', 10);
                rows << QStringLiteral("max") << QString::number(mx, 'g', 10);
                rows << QStringLiteral("mean") << QString::number(sum / finite, 'g', 10);
                rows << QStringLiteral("finite") << QStringLiteral("%1 / %2").arg(finite).arg(n);
            }
            int show = std::min<int>(40, (int)data.size());
            QStringList prev;
            for (int i = 0; i < show; ++i)
                prev << QString::number(data[i], 'g', 8);
            rows << QStringLiteral("preview[0:40]") << prev.join(QStringLiteral(", "));
        }
        m_tbl->setRowCount(rows.size() / 2);
        for (int i = 0; i < rows.size() / 2; ++i) {
            m_tbl->setItem(i, 0, new QTableWidgetItem(rows[i * 2]));
            m_tbl->setItem(i, 1, new QTableWidgetItem(rows[i * 2 + 1]));
        }
    }
}

} // namespace ui
