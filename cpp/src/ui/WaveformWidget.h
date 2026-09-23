#pragma once
#include "data/GediLoader.h"
#include <QWidget>
#include <vector>

namespace ui {

class WaveformWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget* parent = nullptr);
    void showResult(const data::WaveformResult& r);
    void showProcessed(const data::WaveformResult& r, const std::vector<double>& y,
                       const QString& label);
    void showCompare(const std::vector<double>& before, const std::vector<double>& after,
                     bool rhMode);
    void showSpectrum(const std::vector<double>& x, const std::vector<double>& y,
                      const QString& title);
    void clearMessage(const QString& msg);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void paintProfile(QPainter& p, const QRectF& rc, const std::vector<double>& x,
                      const std::vector<double>& y, const QColor& col, const QString& label);

    std::vector<double> m_x, m_y, m_y2;
    std::vector<double> m_specX, m_specY;
    QString m_title, m_label;
    bool m_compare = false;
    bool m_rh = false;
    bool m_hasSpec = false;
    QString m_msg;
};

} // namespace ui
