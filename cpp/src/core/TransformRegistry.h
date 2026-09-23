#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <QMap>
#include <functional>
#include <vector>

namespace core {

struct ParamSpec {
    QString name;
    QString label;
    QString kind; // float | int | choice
    double def = 0.0;
    double minV = -1e9;
    double maxV = 1e9;
    double step = 0.01;
    QStringList choices;
};

enum class ResultKind { Signal, Spectrum };

struct TransformResult {
    std::vector<double> y;
    QString label;
    ResultKind kind = ResultKind::Signal;
    std::vector<double> auxX;
    std::vector<double> auxY;      // 1D spectrum or flattened image
    QString auxLabel;
    QString auxXLabel;
    QString auxYLabel;
    int imageRows = 0;
    int imageCols = 0;
    std::vector<double> image;     // optional scalogram
};

using TransformFn = std::function<TransformResult(const std::vector<double>&, const QVariantMap&)>;

struct TransformSpec {
    QString id;
    QString name;
    QString category;
    ResultKind kind = ResultKind::Signal;
    QVector<ParamSpec> params;
    QString description;
    int menuOrder = 100;
    TransformFn fn;
};

class TransformRegistry
{
public:
    void add(TransformSpec s) { m_items[s.id] = std::move(s); }
    const TransformSpec* get(const QString& id) const
    {
        auto it = m_items.find(id);
        return it == m_items.end() ? nullptr : &it.value();
    }
    QList<TransformSpec> all() const;
    QMap<QString, QList<TransformSpec>> byCategory() const;

private:
    QMap<QString, TransformSpec> m_items;
};

TransformRegistry& transforms();

void registerBuiltinTransforms();

} // namespace core
