#pragma once
#include "data/GediLoader.h"
#include "processing/Calculator.h"
#include <QDialog>

class QListWidget;
class QProgressBar;
class QLabel;

namespace ui {
class WaveformWidget;

class WaveformCalculatorDialog : public QDialog
{
    Q_OBJECT
public:
    WaveformCalculatorDialog(data::GediLoader* loader, const data::WaveformResult* current,
                             QWidget* parent = nullptr);

private:
    void refreshChain();
    void preview();
    void runBatch();
    void saveChain();
    void loadChain();

    data::GediLoader* m_loader = nullptr;
    data::WaveformResult m_current;
    proc::ProcessChain m_chain;
    QListWidget* m_modules = nullptr;
    QListWidget* m_chainList = nullptr;
    WaveformWidget* m_wave = nullptr;
    QProgressBar* m_prog = nullptr;
    QLabel* m_status = nullptr;
};

} // namespace ui
