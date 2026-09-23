#include "ui/BatchDialog.h"
#include "processing/Calculator.h"
#include "processing/Metrics.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTextStream>
#include <QVBoxLayout>

namespace ui {

BatchDialog::BatchDialog(data::GediLoader* loader, QWidget* parent) : QDialog(parent), m_loader(loader)
{
    setWindowTitle(QStringLiteral("Batch"));
    resize(640, 480);
    auto* lay = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    for (auto* f : m_loader->files()) {
        auto* it = new QListWidgetItem(f->displayName());
        it->setData(Qt::UserRole, f->fileId);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Checked);
        m_list->addItem(it);
    }
    lay->addWidget(m_list, 1);
    m_quality = new QCheckBox(QStringLiteral("quality==1"), this);
    m_quality->setChecked(true);
    m_stride = new QSpinBox(this);
    m_stride->setRange(1, 500);
    m_stride->setValue(25);
    m_max = new QSpinBox(this);
    m_max->setRange(0, 100000);
    m_max->setValue(100);
    m_max->setSpecialValueText(QStringLiteral("ALL"));
    auto* row = new QHBoxLayout();
    row->addWidget(m_quality);
    row->addWidget(new QLabel(QStringLiteral("stride")));
    row->addWidget(m_stride);
    row->addWidget(new QLabel(QStringLiteral("max/beam")));
    row->addWidget(m_max);
    lay->addLayout(row);
    m_prog = new QProgressBar(this);
    m_status = new QLabel(QStringLiteral("Ready"), this);
    lay->addWidget(m_prog);
    lay->addWidget(m_status);
    auto* runBtn = new QPushButton(QStringLiteral("Run"), this);
    connect(runBtn, &QPushButton::clicked, this, &BatchDialog::run);
    lay->addWidget(runBtn);
}

void BatchDialog::run()
{
    QList<int> ids;
    for (int i = 0; i < m_list->count(); ++i)
        if (m_list->item(i)->checkState() == Qt::Checked)
            ids << m_list->item(i)->data(Qt::UserRole).toInt();
    proc::ProcessChain chain; // empty = raw metrics
    auto res = proc::runBatch(*m_loader, ids, chain, m_quality->isChecked(), m_stride->value(),
                              m_max->value(), [this](int c, int t, QString m) {
                                  m_prog->setValue(t ? c * 100 / t : 0);
                                  m_status->setText(m);
                              });
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export CSV"), "metrics.csv");
    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "file,beam,shot,lon,lat,wf_mean,wf_energy,rh50,rh98\n";
            for (const auto& r : res.rows) {
                auto m = r.metrics;
                ts << r.file << "," << r.beam << "," << r.shot << "," << r.lon << "," << r.lat << ","
                   << m.value("wf_mean") << "," << m.value("wf_energy") << "," << m.value("rh50") << ","
                   << m.value("rh98") << "\n";
            }
        }
    }
    QMessageBox::information(this, QStringLiteral("Done"),
                             QStringLiteral("ok=%1 skip=%2").arg(res.nOk).arg(res.nSkip));
}

} // namespace ui
