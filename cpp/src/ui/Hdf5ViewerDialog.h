#pragma once
#include "data/H5Dyn.h"
#include <QDialog>

class QTreeWidget;
class QTableWidget;
class QLabel;
class QTreeWidgetItem;

namespace ui {

class Hdf5ViewerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit Hdf5ViewerDialog(const QString& path, QWidget* parent = nullptr);
    ~Hdf5ViewerDialog() override;

private:
    void loadTree();
    void onSelect(QTreeWidgetItem* cur, QTreeWidgetItem* prev);
    void fillGroup(long long file, const QString& path, QTreeWidgetItem* parent);

    QString m_path;
    long long m_file = -1;
    QTreeWidget* m_tree = nullptr;
    QTableWidget* m_tbl = nullptr;
    QLabel* m_doc = nullptr;
};

} // namespace ui
