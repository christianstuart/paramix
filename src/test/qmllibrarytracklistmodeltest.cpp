#include <gtest/gtest.h>

#include <QDebug>

#include "library/columncache.h"
#include "library/librarytablemodel.h"
#include "qml/qmllibrarytracklistcolumn.h"
#include "qml/qmllibrarytracklistmodel.h"
#include "qml/qmltrackproxy.h"
#include "test/librarytest.h"
#include "track/track.h"

namespace {

class QmlLibraryTrackListModelTest : public LibraryTest {
  protected:
    static mixxx::qml::QmlLibraryTrackListColumn makeTitleColumn() {
        return mixxx::qml::QmlLibraryTrackListColumn(nullptr,
                QStringLiteral("Title"),
                1,
                ColumnCache::COLUMN_LIBRARYTABLE_TITLE,
                120.0,
                0.0,
                nullptr,
                mixxx::qml::QmlLibraryTrackListColumn::Role::Title);
    }
};

TEST_F(QmlLibraryTrackListModelTest, ReusesTrackProxyAcrossCells) {
    const auto pTrack = getOrAddTrackByLocation(
            getTestDir().filePath(QStringLiteral("fixtures/beatgrid_128bpm.wav")));
    ASSERT_TRUE(pTrack);

    LibraryTableModel sourceModel(
            nullptr, trackCollectionManager(), "QmlLibraryTrackListModelTest");
    sourceModel.select();
    ASSERT_GT(sourceModel.rowCount(), 0);

    mixxx::qml::QmlLibraryTrackListColumn titleColumn = makeTitleColumn();
    QList<mixxx::qml::QmlLibraryTrackListColumn*> columns{&titleColumn};
    mixxx::qml::QmlLibraryTrackListModel model(columns, &sourceModel);
    ASSERT_GT(model.rowCount(), 0);

    const QModelIndex index = model.index(0, 0);
    const QVariant first = model.data(index, mixxx::qml::QmlLibraryTrackListModel::Track);
    const QVariant second = model.data(index, mixxx::qml::QmlLibraryTrackListModel::Track);
    mixxx::qml::QmlTrackProxy* pFirst =
            first.value<mixxx::qml::QmlTrackProxy*>();
    mixxx::qml::QmlTrackProxy* pSecond =
            second.value<mixxx::qml::QmlTrackProxy*>();
    ASSERT_NE(nullptr, pFirst);
    EXPECT_EQ(pFirst, pSecond);
    EXPECT_EQ(pFirst, model.getTrack(0));
}

TEST_F(QmlLibraryTrackListModelTest, RowRolesDoNotRequireATrackPerCell) {
    const auto pTrack = getOrAddTrackByLocation(
            getTestDir().filePath(QStringLiteral("fixtures/beatgrid_128bpm.wav")));
    ASSERT_TRUE(pTrack);

    LibraryTableModel sourceModel(
            nullptr, trackCollectionManager(), "QmlLibraryTrackListModelTest");
    sourceModel.select();
    ASSERT_GT(sourceModel.rowCount(), 0);

    mixxx::qml::QmlLibraryTrackListColumn titleColumn = makeTitleColumn();
    QList<mixxx::qml::QmlLibraryTrackListColumn*> columns{&titleColumn};
    mixxx::qml::QmlLibraryTrackListModel model(columns, &sourceModel);

    const QModelIndex index = model.index(0, 0);
    EXPECT_FALSE(model.data(index, mixxx::qml::QmlLibraryTrackListModel::HasStems)
                    .toBool());
    // The invokables are what the QML cover column uses. They must resolve the
    // row without requiring a QmlTrackProxy for every column cell.
    EXPECT_EQ(model.data(index, mixxx::qml::QmlLibraryTrackListModel::CoverArt)
                      .toUrl(),
            model.getCoverArtUrl(0));
    EXPECT_NO_THROW((void)model.getDecorationColor(0));

    // The cover cell binds to `revision`; dataChanged must bump it or the
    // invokable-based bindings would not re-evaluate after a colour/cover edit.
    const int revision = model.revision();
    model.dataChanged(index, index);
    EXPECT_EQ(revision + 1, model.revision());
}

} // namespace
