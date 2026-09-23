#include "ui/PropertyPanel.h"
#include "core/I18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ui {

PropertyPanel::PropertyPanel(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    g1 = new QGroupBox(i18n::t("panel_display"), this);
    auto* v1 = new QVBoxLayout(g1);
    m_scalar = new QComboBox(g1);
    m_scalar->addItem(QStringLiteral("beam"), QStringLiteral("beam"));
    m_scalar->addItem(QStringLiteral("file"), QStringLiteral("file"));
    m_scalar->addItem(QStringLiteral("sensitivity"), QStringLiteral("sensitivity"));
    m_scalar->addItem(QStringLiteral("quality"), QStringLiteral("quality"));
    m_scalar->addItem(QStringLiteral("canopy"), QStringLiteral("canopy"));
    v1->addWidget(m_scalar);
    g2 = new QGroupBox(QStringLiteral("Display"), this);
    auto* v2 = new QVBoxLayout(g2);
    m_size = new QSpinBox(g2);
    m_size->setRange(2, 24);
    m_size->setValue(5);
    m_circles = new QCheckBox(QStringLiteral("~25 m"), g2);
    m_circles->setChecked(true);
    v2->addWidget(m_size);
    v2->addWidget(m_circles);
    m_quality = new QCheckBox(i18n::t("filter_quality"), g2);
    m_quality->setChecked(false);
    v2->addWidget(m_quality);
    connect(m_quality, &QCheckBox::toggled, this, [this] { emit displayChanged(); });
    g3 = new QGroupBox(QStringLiteral("Beam"), this);
    auto* v3 = new QVBoxLayout(g3);
    m_beam = new QComboBox(g3);
    m_beam->addItem(QStringLiteral("ALL"), QString());
    for (const char* b :
         {"BEAM0000", "BEAM0001", "BEAM0010", "BEAM0011", "BEAM0101", "BEAM0110", "BEAM1000", "BEAM1011"})
        m_beam->addItem(QString::fromLatin1(b), QString::fromLatin1(b));
    v3->addWidget(m_beam);
    lay->addWidget(g1);
    lay->addWidget(g2);
    lay->addWidget(g3);
    lay->addStretch(1);

    auto changed = [this] { emit displayChanged(); };
    connect(m_scalar, &QComboBox::currentIndexChanged, this, changed);
    connect(m_beam, &QComboBox::currentIndexChanged, this, changed);
    connect(m_size, qOverload<int>(&QSpinBox::valueChanged), this, changed);
    connect(m_circles, &QCheckBox::toggled, this, changed);
}

void PropertyPanel::retranslate()
{
    g1->setTitle(i18n::t("panel_display"));
}

QString PropertyPanel::scalar() const { return m_scalar->currentData().toString(); }
QString PropertyPanel::beamFilter() const { return m_beam->currentData().toString(); }
int PropertyPanel::pointSize() const { return m_size->value(); }
bool PropertyPanel::showCircles() const { return m_circles->isChecked(); }
bool PropertyPanel::qualityOnly() const { return m_quality && m_quality->isChecked(); }

} // namespace ui
