#include "ui/InfoPanel.h"
#include "core/I18n.h"
#include "processing/Metrics.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace ui {

InfoPanel::InfoPanel(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    auto* w1 = new QWidget();
    auto* l1 = new QVBoxLayout(w1);
    m_info = new QLabel(i18n::t("hint_info"), w1);
    m_info->setWordWrap(true);
    m_info->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    l1->addWidget(m_info);
    m_tabs->addTab(w1, i18n::t("tab_file_info"));

    auto* w2 = new QWidget();
    auto* l2 = new QVBoxLayout(w2);
    auto* btn = new QPushButton(i18n::t("act_recompute"), w2);
    connect(btn, &QPushButton::clicked, this, &InfoPanel::recomputeRequested);
    l2->addWidget(btn, 0, Qt::AlignRight);
    m_table = new QTableWidget(0, 2, w2);
    m_table->setHorizontalHeaderLabels({i18n::t("tab_metrics"), QStringLiteral("Value")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    l2->addWidget(m_table);
    m_tabs->addTab(w2, i18n::t("tab_metrics"));
    lay->addWidget(m_tabs);
}

void InfoPanel::retranslate()
{
    m_tabs->setTabText(0, i18n::t("tab_file_info"));
    m_tabs->setTabText(1, i18n::t("tab_metrics"));
}

void InfoPanel::showFile(const data::GediFile* f)
{
    m_tabs->setCurrentIndex(0);
    if (!f) {
        m_info->setText(i18n::t("hint_info"));
        return;
    }
    QStringList beams;
    for (auto it = f->beams.constBegin(); it != f->beams.constEnd(); ++it)
        beams << it.key();
    m_info->setText(QStringLiteral("%1\n%2: %3\n%4: %5\n%6: %7\n%8: %9")
                        .arg(QFileInfo(f->path).fileName(), i18n::t("tab_file_info"),
                             data::productName(f->product), QStringLiteral("shots"),
                             QString::number(f->nShots), QStringLiteral("path"), f->path,
                             QStringLiteral("beams"), beams.join(QLatin1Char(','))));
}

void InfoPanel::showMessage(const QString& msg)
{
    m_tabs->setCurrentIndex(0);
    m_info->setText(msg);
}

void InfoPanel::showMetrics(const data::WaveformResult& r, const QMap<QString, double>& m)
{
    m_tabs->setCurrentIndex(1);
    Q_UNUSED(r);
    const QStringList order = {"shot_number", "lon",  "lat",         "canopy_height", "sensitivity",
                               "quality",     "wf_mean", "wf_std",   "wf_energy",     "wf_centroid",
                               "wf_fwhm",     "wf_peak_count", "gcr", "rh50",          "rh98",
                               "rh100",       "energy_total"};
    QStringList rows;
    for (const auto& k : order)
        if (m.contains(k))
            rows << k;
    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        m_table->setItem(i, 0, new QTableWidgetItem(rows[i]));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::number(m.value(rows[i]), 'g', 8)));
    }
}

} // namespace ui
