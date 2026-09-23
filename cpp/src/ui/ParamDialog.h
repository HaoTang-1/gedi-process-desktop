#pragma once
#include "core/TransformRegistry.h"
#include <QDialog>
#include <QVariantMap>

namespace ui {

class ParamDialog : public QDialog
{
    Q_OBJECT
public:
    ParamDialog(const QString& title, const QString& desc, const QVector<core::ParamSpec>& params,
                QWidget* parent = nullptr);
    QVariantMap values() const;

private:
    QVector<core::ParamSpec> m_params;
    QList<QWidget*> m_widgets;
};

} // namespace ui
