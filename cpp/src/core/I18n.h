#pragma once
#include <QString>
#include <QStringList>
#include <functional>

namespace i18n {

enum class Lang { Zh, En };

Lang lang();
void setLang(Lang l);
void setLangFromName(const QString& name); // "zh" | "en"
QString name();

// key lookup; falls back to key
QString t(const char* key);

void addListener(std::function<void(Lang)> fn);

} // namespace i18n
