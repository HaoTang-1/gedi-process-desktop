#pragma once
#include <QDialog>

namespace ui {
class RasterPropsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RasterPropsDialog(const QString& path, QWidget* parent = nullptr);
};
}
