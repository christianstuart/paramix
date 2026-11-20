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

    return QProcess::startDetached(QCoreApplication::applicationFilePath(), args);
}

} // namespace mixxx
