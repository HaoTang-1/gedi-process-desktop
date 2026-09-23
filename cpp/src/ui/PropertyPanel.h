#pragma once
#include <QWidget>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QGroupBox;

namespace ui {

class PropertyPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PropertyPanel(QWidget* parent = nullptr);
    void retranslate();
    QString scalar() const;
    QString beamFilter() const;
    int pointSize() const;
    bool showCircles() const;
    bool qualityOnly() const;

signals:
    void displayChanged();

private:
    QGroupBox *g1 = nullptr, *g2 = nullptr, *g3 = nullptr;
    QComboBox *m_scalar = nullptr, *m_beam = nullptr;
    QSpinBox* m_size = nullptr;
    QCheckBox* m_circles = nullptr;
    QCheckBox* m_quality = nullptr;
};

} // namespace ui
