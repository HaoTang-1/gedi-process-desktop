#pragma once
#include "data/GediLoader.h"
#include "processing/Calculator.h"
#include <QDialog>

class QListWidget;
class QProgressBar;
class QLabel;
class QSpinBox;
class QCheckBox;

namespace ui {
class BatchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BatchDialog(data::GediLoader* loader, QWidget* parent = nullptr);
private slots:
    void run();

private:
    data::GediLoader* m_loader = nullptr;
    QListWidget* m_list = nullptr;
    QProgressBar* m_prog = nullptr;
    QLabel* m_status = nullptr;
    QSpinBox *m_stride = nullptr, *m_max = nullptr;
    QCheckBox* m_quality = nullptr;
};
}
