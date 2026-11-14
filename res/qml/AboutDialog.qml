import Mixxx 1.0 as Mixxx
import QtQuick.Dialogs

MessageDialog {
    buttons: MessageDialog.Ok
    informativeText: qsTr("%1\n%2").arg(Mixxx.Application.platform).arg("https://github.com/christianstuart/paramix")
    text: qsTr("%1 %2").arg(Mixxx.Application.applicationName).arg(Mixxx.Application.version)
    title: qsTr("About %1").arg(Mixxx.Application.applicationName)
}
