#include "qml/qmllibrarysourcetree.h"

#include <qalgorithms.h>
#include <qvariant.h>

#include <QAbstractListModel>
#include <QVariant>

#include "library/library.h"
#include "library/librarytablemodel.h"
#include "moc_qmllibrarysourcetree.cpp"
#include "qml_owned_ptr.h"
#include "qmllibraryproxy.h"
#include "qmllibrarytracklistmodel.h"
#include "qmlsidebarmodelproxy.h"

namespace mixxx {
namespace qml {

QmlLibrarySourceTree::QmlLibrarySourceTree(QQuickItem* parent)
        : QQuickItem(parent),
          m_model(new QmlSidebarModelProxy(this)) {
}
QmlLibrarySourceTree::~QmlLibrarySourceTree() = default;

Q_INVOKABLE QmlLibraryTrackListModel* QmlLibrarySourceTree::allTracks() const {
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return nullptr;
    }
    return make_qml_owned<QmlLibraryTrackListModel>(
            m_defaultColumns, pLibrary->trackTableModel());
}

void QmlLibrarySourceTree::componentComplete() {
    m_model->setColumns(m_defaultColumns);
    m_model->update();
}

} // namespace qml
} // namespace mixxx
