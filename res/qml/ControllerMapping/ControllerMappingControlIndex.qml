import QtQuick 2.15
import Mixxx 1.0 as Mixxx

/// Snapshots the learnable control inventory into a plain JavaScript array.
///
/// The C++ inventory is `Mixxx.ControllerLearning.availableControls`, a
/// `ControlPickerModel` that reaches QML through a `QAbstractListModel*`
/// property. Roles are only readable from a delegate, so this component runs a
/// one-shot hidden Repeater over the model and copies every row into `entries`.
/// The Repeater (and its item delegates) is destroyed on the next event loop
/// pass; only the small JS objects remain. The search list then filters the
/// array synchronously instead of rebuilding a delegate per row.
///
/// `ready` becomes true once the snapshot is complete.
Item {
    id: root

    property var entries: []
    property bool ready: false

    visible: false

    Component.onCompleted: {
        root.ready = true;
        // Release the item delegates once their roles have been harvested.
        Qt.callLater(function () {
            harvest.destroy();
        });    }

    Repeater {
        id: harvest

        model: Mixxx.ControllerLearning.availableControls

        delegate: Item {
            required property string category
            required property string description
            required property string group
            required property string key
            required property string title

            Component.onCompleted: root.entries.push({
                "group": group,
                "key": key,
                "title": title,
                "description": description,
                "category": category,
                "configKey": group + "," + key,
                "search": (title + " " + description + " " + category + " " + group + " " + key).toLowerCase()
            })
        }
    }
}
