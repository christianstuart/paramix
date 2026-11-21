#include <gtest/gtest.h>

#include "library/export/dlglibraryexport.h"
#ifdef __REKORDBOX__
#include "library/export/rekordbox/rekordboxexportrequest.h"
#endif
#include "library/trackset/crate/crateid.h"
#include "test/mixxxtest.h"

namespace mixxx {
namespace {

class RekordboxExportUiTest : public MixxxTest {};

#ifdef __REKORDBOX__
TEST_F(RekordboxExportUiTest, OffersAndSelectsTheRekordboxTarget) {
    DlgLibraryExport dialog(nullptr, config(), nullptr);

    EXPECT_TRUE(dialog.isRekordboxTargetAvailable());
    EXPECT_EQ(LibraryExportTarget::EnginePrime, dialog.selectedTarget());

    dialog.selectRekordboxTarget();
    EXPECT_EQ(LibraryExportTarget::Rekordbox, dialog.selectedTarget());
}
#endif

#ifdef __REKORDBOX__
TEST_F(RekordboxExportUiTest, BuildsTheDeviceRequestFromTheSelection) {
    const QDir deviceRoot(QStringLiteral("/tmp/paramix-rekordbox-device"));
    auto pRequest = createRekordboxExportRequest(
            deviceRoot, {CrateId{QVariant(3)}}, {7, 11});

    ASSERT_TRUE(pRequest);
    EXPECT_EQ(deviceRoot.absolutePath(), pRequest->deviceRoot.absolutePath());
    EXPECT_TRUE(pRequest->deviceName.isEmpty());
    ASSERT_EQ(1, pRequest->crateIdsToExport.size());
    EXPECT_TRUE(pRequest->crateIdsToExport.contains(CrateId{QVariant(3)}));
    ASSERT_EQ(2, pRequest->playlistIdsToExport.size());
    EXPECT_TRUE(pRequest->playlistIdsToExport.contains(7));
    EXPECT_TRUE(pRequest->playlistIdsToExport.contains(11));
}
#endif

} // namespace
} // namespace mixxx
