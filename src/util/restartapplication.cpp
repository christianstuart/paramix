#include "util/restartapplication.h"

#include <QCoreApplication>
#include <QProcess>
#include <QStringList>

namespace mixxx {

bool restartApplication(bool useQml) {
    QStringList args = QCoreApplication::arguments();
    if (!args.isEmpty()) {
        args.removeFirst();
    }

    QStringList filteredArgs;
    filteredArgs.reserve(args.size() + 2);
    for (const QString& arg : args) {
        if (arg == QStringLiteral("--new-ui") ||
                arg == QStringLiteral("--qml") ||
                arg == QStringLiteral("--allow-dangerous-data-corruption-risk")) {
            continue;
        }
        filteredArgs.append(arg);
    }

    if (useQml) {
        filteredArgs.append(QStringLiteral("--new-ui"));
        filteredArgs.append(QStringLiteral("--allow-dangerous-data-corruption-risk"));
    }

    return QProcess::startDetached(QCoreApplication::applicationFilePath(), filteredArgs);
}

} // namespace mixxx
