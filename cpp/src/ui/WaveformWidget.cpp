#include "ui/WaveformWidget.h"
#include "core/I18n.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace ui {

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(280, 280);
    m_msg = i18n::t("hint_wave");
}

void WaveformWidget::clearMessage(const QString& msg)
{
    m_msg = msg;
    m_x.clear();
    m_y.clear();
    m_y2.clear();
    m_specX.clear();
    m_specY.clear();
    m_hasSpec = false;
    m_compare = false;
    m_title = i18n::t("panel_wave");
    update();
}

void WaveformWidget::showSpectrum(const std::vector<double>& x, const std::vector<double>& y,
                                  const QString& title)
{
    m_specX = x;
    m_specY = y;
    m_hasSpec = !y.empty();
    if (!title.isEmpty())
        m_title = title;
    update();
}

void WaveformWidget::showResult(const data::WaveformResult& r)
{
    m_msg.clear();
    m_compare = false;
    m_y2.clear();
    m_hasSpec = false;
    m_specX.clear();
    m_specY.clear();
    m_rh = r.hasRh();
    if (r.hasWaveform()) {
        m_y = r.waveform;
        m_x = r.heights;
        if (m_x.size() != m_y.size()) {
            m_x.resize(m_y.size());
            for (size_t i = 0; i < m_y.size(); ++i)
                m_x[i] = double(i);
        }
    } else if (r.hasRh()) {
        m_y = r.rh;
        m_x.resize(m_y.size());
        for (size_t i = 0; i < m_y.size(); ++i)
            m_x[i] = double(i);
    } else {
        clearMessage(i18n::t("hint_no_wave"));
        return;
    }
    m_title = QStringLiteral("%1 · %2 · %3").arg(data::productName(r.product), r.beam).arg(r.shotNumber);
    m_label = i18n::t("axis_intensity");
    update();
}

void WaveformWidget::showProcessed(const data::WaveformResult& r, const std::vector<double>& y,
                                   const QString& label)
{
    showResult(r);
    m_y2 = y;
    m_label = label;
    update();
}

void WaveformWidget::showCompare(const std::vector<double>& before, const std::vector<double>& after,
                                 bool rhMode)
{
    m_msg.clear();
    m_compare = true;
    m_rh = rhMode;
    m_y = before;
    m_y2 = after;
    m_x.resize(before.size());
    for (size_t i = 0; i < before.size(); ++i)
        m_x[i] = double(i);
    m_title = QStringLiteral("Before / After");
    update();
}

void WaveformWidget::paintProfile(QPainter& p, const QRectF& rc, const std::vector<double>& x,
                                  const std::vector<double>& y, const QColor& col,
                                  const QString& label)
{
    if (y.empty())
        return;
    // x-axis intensity = value; y-axis = index/height
    double vmin = 1e9, vmax = -1e9;
    for (double v : y) {
        if (std::isfinite(v)) {
            vmin = std::min(vmin, v);
            vmax = std::max(vmax, v);
        }
    }
    if (vmax <= vmin)
        vmax = vmin + 1;
    double y0 = 0, y1 = double(y.size() - 1);
    QPainterPath path;
    for (size_t i = 0; i < y.size(); ++i) {
        double u = (y[i] - vmin) / (vmax - vmin);
        double v = 1.0 - (double(i) - y0) / std::max(1.0, y1 - y0);
        QPointF pt(rc.left() + u * rc.width(), rc.top() + v * rc.height());
        if (i == 0)
            path.moveTo(pt);
        else
            path.lineTo(pt);
    }
    p.setPen(QPen(col, 2));
    p.drawPath(path);
    p.setPen(QColor("#5B6B7C"));
    p.drawText(rc.adjusted(4, 4, -4, -4), Qt::AlignTop | Qt::AlignRight, label);
}

void WaveformWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#FBFCFE"));
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor("#0B3D91"));
    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.drawText(rect().adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignHCenter, m_title);
    f.setBold(false);
    f.setPointSizeF(8);
    p.setFont(f);
    p.setPen(QColor("#5B6B7C"));
    // axis hint at the BOTTOM (gray)
    const QString hint = QStringLiteral("Y: %1 ↑    X: %2 →")
                             .arg(i18n::t("axis_height"), i18n::t("axis_intensity"));
    p.drawText(rect().adjusted(8, 4, -8, -8), Qt::AlignBottom | Qt::AlignHCenter, hint);

    if (!m_msg.isEmpty()) {
        p.setPen(QColor("#5B6B7C"));
        p.drawText(rect(), Qt::AlignCenter, m_msg);
        return;
    }

    // main profile (top) + optional spectrum (bottom)
    int bottomPad = 36;
    QRectF full = rect().adjusted(40, 36, -16, -bottomPad);
    QRectF rMain = m_hasSpec ? QRectF(full.left(), full.top(), full.width(), full.height() * 0.58)
                             : full;
    QRectF rSpec = QRectF(full.left(), full.bottom() - full.height() * 0.32,
                          full.width(), full.height() * 0.32);

    p.setPen(QColor("#D5DEE7"));
    p.drawRect(rMain);
    if (m_compare) {
        QRectF r1(rMain.left(), rMain.top(), rMain.width() * 0.48, rMain.height());
        QRectF r2(rMain.left() + rMain.width() * 0.52, rMain.top(), rMain.width() * 0.48, rMain.height());
        paintProfile(p, r1, m_x, m_y, QColor("#5B6B7C"), QStringLiteral("Before"));
        paintProfile(p, r2, m_x, m_y2, QColor("#0B3D91"), QStringLiteral("After"));
    } else {
        if (m_y2.size() == m_y.size())
            paintProfile(p, rMain, m_x, m_y, QColor("#5B6B7C"), QStringLiteral("raw"));
        paintProfile(p, rMain, m_x, m_y2.empty() ? m_y : m_y2, QColor("#0B3D91"), m_label);
    }

    if (m_hasSpec && m_specY.size() > 1) {
        p.setPen(QColor("#D5DEE7"));
        p.drawRect(rSpec);
        std::vector<double> xs = m_specX;
        if (xs.size() != m_specY.size()) {
            xs.resize(m_specY.size());
            for (size_t i = 0; i < xs.size(); ++i)
                xs[i] = double(i);
        }
        paintProfile(p, rSpec, xs, m_specY, QColor("#1AA3C8"), QStringLiteral("FFT / Spectrum"));
    }
}

} // namespace ui
