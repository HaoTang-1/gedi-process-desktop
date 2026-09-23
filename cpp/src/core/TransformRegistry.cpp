#include "core/TransformRegistry.h"

#include <algorithm>

namespace core {

QList<TransformSpec> TransformRegistry::all() const
{
    QList<TransformSpec> out;
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it)
        out.push_back(it.value());
    std::sort(out.begin(), out.end(), [](const TransformSpec& a, const TransformSpec& b) {
        if (a.menuOrder != b.menuOrder)
            return a.menuOrder < b.menuOrder;
        return a.name < b.name;
    });
    return out;
}

QMap<QString, QList<TransformSpec>> TransformRegistry::byCategory() const
{
    QMap<QString, QList<TransformSpec>> out;
    for (const auto& s : all())
        out[s.category].push_back(s);
    return out;
}

TransformRegistry& transforms()
{
    static TransformRegistry r;
    return r;
}

} // namespace core
