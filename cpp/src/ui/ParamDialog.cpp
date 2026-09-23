#include "ui/ParamDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ui {

ParamDialog::ParamDialog(const QString& title, const QString& desc,
                         const QVector<core::ParamSpec>& params, QWidget* parent)
    : QDialog(parent), m_params(params)
{
    setWindowTitle(title);
    auto* lay = new QVBoxLayout(this);
    if (!desc.isEmpty()) {
        auto* l = new QLabel(desc, this);
        l->setWordWrap(true);
        lay->addWidget(l);
    }
    auto* form = new QFormLayout();
    for (const auto& p : params) {
        if (p.kind == QLatin1String("int")) {
            auto* w = new QSpinBox(this);
            w->setRange(int(p.minV), int(p.maxV));
            w->setValue(int(p.def));
            m_widgets << w;
            form->addRow(p.label, w);
        } else if (p.kind == QLatin1String("choice")) {
            auto* w = new QComboBox(this);
            w->addItems(p.choices);
            w->setCurrentIndex(int(p.def));
            m_widgets << w;
            form->addRow(p.label, w);
        } else {
            auto* w = new QDoubleSpinBox(this);
            w->setRange(p.minV, p.maxV);
            w->setDecimals(4);
            w->setSingleStep(p.step);
            w->setValue(p.def);
            m_widgets << w;
            form->addRow(p.label, w);
        }
    }
    lay->addLayout(form);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

QVariantMap ParamDialog::values() const
{
    QVariantMap m;
    for (int i = 0; i < m_params.size(); ++i) {
        const auto& p = m_params[i];
        QWidget* w = m_widgets[i];
        if (auto* sb = qobject_cast<QDoubleSpinBox*>(w))
            m.insert(p.name, sb->value());
        else if (auto* ib = qobject_cast<QSpinBox*>(w))
            m.insert(p.name, ib->value());
        else if (auto* cb = qobject_cast<QComboBox*>(w))
            m.insert(p.name, cb->currentIndex());
    }
    return m;
}

} // namespace ui
