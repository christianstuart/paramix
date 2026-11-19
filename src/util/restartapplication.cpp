#include "util/restartapplication.h"

#include <QCoreApplication>
#include <QProcess>
#include <QStringList>

namespace mixxx {

bool restartApplication() {
    QStringList args = QCoreApplication::arguments();
    if (!args.isEmpty()) {
        args.removeFirst();
    }

    QStringList filteredArgs;
    filteredArgs.reserve(args.size());
    for (const QString& arg : args) {
        if (arg == QStringLiteral("--new-ui") ||
                arg == QStringLiteral("--qml") ||
                arg == QStringLiteral("--allow-dangerous-data-corruption-risk")) {
            continue;
        }
        filteredArgs.append(arg);
    }

    return QProcess::startDetached(QCoreApplication::applicationFilePath(), filteredArgs);
}

} // namespace mixxx
