#pragma once
#include "data/GediLoader.h"
#include <QWidget>

class QLabel;
class QTableWidget;
class QTabWidget;

namespace ui {

class InfoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit InfoPanel(QWidget* parent = nullptr);
    void retranslate();
    void showFile(const data::GediFile* f);
    void showMessage(const QString& msg);
    void showMetrics(const data::WaveformResult& r, const QMap<QString, double>& m);

signals:
    void recomputeRequested();

private:
    QLabel* m_info = nullptr;
    QTableWidget* m_table = nullptr;
    QTabWidget* m_tabs = nullptr;
};

} // namespace ui
