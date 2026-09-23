#include "ui/WaveformCalculatorDialog.h"
#include "core/TransformRegistry.h"
#include "processing/Calculator.h"
#include "ui/ParamDialog.h"
#include "ui/WaveformWidget.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QSplitter>
#include <QTextStream>
#include <QVBoxLayout>

namespace ui {

WaveformCalculatorDialog::WaveformCalculatorDialog(data::GediLoader* loader,
                                                   const data::WaveformResult* current, QWidget* parent)
    : QDialog(parent), m_loader(loader)
{
    if (current)
        m_current = *current;
    setWindowTitle(QStringLiteral("Waveform Calculator"));
    resize(1100, 680);
    auto* root = new QVBoxLayout(this);
    auto* split = new QSplitter(Qt::Horizontal, this);
    m_modules = new QListWidget(split);
    for (const auto& s : core::transforms().all()) {
        auto* it = new QListWidgetItem(s.category + QStringLiteral(" / ") + s.name);
        it->setData(Qt::UserRole, s.id);
        m_modules->addItem(it);
    }
    m_chainList = new QListWidget(split);
    m_wave = new WaveformWidget(split);
    split->addWidget(m_modules);
    split->addWidget(m_chainList);
    split->addWidget(m_wave);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 2);
    root->addWidget(split, 1);

    auto* row = new QHBoxLayout();
    auto* add = new QPushButton(QStringLiteral("Add →"));
    connect(add, &QPushButton::clicked, this, [this] {
        auto* it = m_modules->currentItem();
        if (!it)
            return;
        QString id = it->data(Qt::UserRole).toString();
        auto* sp = core::transforms().get(id);
        QVariantMap params;
        if (sp && !sp->params.isEmpty()) {
            ParamDialog dlg(sp->name, sp->description, sp->params, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            params = dlg.values();
        }
        m_chain.steps.push_back({id, params});
        refreshChain();
        preview();
    });
    auto* del = new QPushButton(QStringLiteral("Remove"));
    connect(del, &QPushButton::clicked, this, [this] {
        int r = m_chainList->currentRow();
        if (r >= 0) {
            m_chain.steps.remove(r);
            refreshChain();
            preview();
        }
    });
    auto* save = new QPushButton(QStringLiteral("Save…"));
    connect(save, &QPushButton::clicked, this, &WaveformCalculatorDialog::saveChain);
    auto* load = new QPushButton(QStringLiteral("Load…"));
    connect(load, &QPushButton::clicked, this, &WaveformCalculatorDialog::loadChain);
    auto* prev = new QPushButton(QStringLiteral("Preview"));
    connect(prev, &QPushButton::clicked, this, &WaveformCalculatorDialog::preview);
    auto* batch = new QPushButton(QStringLiteral("Batch…"));
    batch->setObjectName(QStringLiteral("primary"));
    connect(batch, &QPushButton::clicked, this, &WaveformCalculatorDialog::runBatch);
    for (auto* b : {add, del, save, load, prev, batch})
        row->addWidget(b);
    row->addStretch(1);
    root->addLayout(row);
    m_prog = new QProgressBar(this);
    m_status = new QLabel(QStringLiteral("Ready"), this);
    root->addWidget(m_prog);
    root->addWidget(m_status);
    preview();
}

void WaveformCalculatorDialog::refreshChain()
{
    m_chainList->clear();
    int i = 1;
    for (const auto& s : m_chain.steps) {
        auto* sp = core::transforms().get(s.id);
        m_chainList->addItem(QStringLiteral("%1. %2").arg(i++, 2, 10, QLatin1Char('0')).arg(sp ? sp->name : s.id));
    }
}

void WaveformCalculatorDialog::preview()
{
    std::vector<double> src;
    if (m_current.hasWaveform())
        src = m_current.waveform;
    else if (m_current.hasRh())
        src = m_current.rh;
    if (src.empty()) {
        m_wave->clearMessage(QStringLiteral("Select a footprint first"));
        return;
    }
    auto out = proc::runChain(src, m_chain);
    m_wave->showCompare(src, out.y, m_current.hasRh());
    m_status->setText(out.label);
}

void WaveformCalculatorDialog::saveChain()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save"), "chain.txt");
    if (path.isEmpty())
        return;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream ts(&f);
    ts << "name=" << m_chain.name << "\n";
    for (const auto& s : m_chain.steps)
        ts << s.id << "\n";
    f.commit();
}

void WaveformCalculatorDialog::loadChain()
{
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Load"), QString(), QStringLiteral("*.txt"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    m_chain.steps.clear();
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith(QLatin1String("name=")))
            m_chain.name = line.mid(5);
        else if (!line.isEmpty() && core::transforms().get(line))
            m_chain.steps.push_back({line, {}});
    }
    refreshChain();
    preview();
}

void WaveformCalculatorDialog::runBatch()
{
    if (m_chain.steps.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Batch"), QStringLiteral("Empty chain"));
        return;
    }
    QList<int> ids;
    for (auto* f : m_loader->files())
        ids << f->fileId;
    m_prog->setRange(0, 100);
    auto res = proc::runBatch(*m_loader, ids, m_chain, true, 50, 50, [this](int c, int t, QString m) {
        m_prog->setValue(t ? c * 100 / t : 0);
        m_status->setText(m);
    });
    m_status->setText(QStringLiteral("ok=%1 skip=%2").arg(res.nOk).arg(res.nSkip));
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export"), "batch.csv",
                                                QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "file,beam,shot,lon,lat,out_mean,out_energy\n";
        for (const auto& r : res.rows)
            ts << r.file << "," << r.beam << "," << r.shot << "," << r.lon << "," << r.lat << ","
               << r.outMean << "," << r.outEnergy << "\n";
    }
    QMessageBox::information(this, QStringLiteral("Batch"),
                             QStringLiteral("Exported %1 rows").arg(res.rows.size()));
}

} // namespace ui
