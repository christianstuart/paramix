#include <gtest/gtest.h>

#include <QDialog>
#include <memory>

#include "qml/qmldlgpreferencesproxy.h"

namespace {

TEST(QmlDlgPreferencesProxyTest, CreatesDialogOnFirstShowOnly) {
    auto pDialog = std::make_shared<QDialog>();
    pDialog->setAttribute(Qt::WA_DontShowOnScreen, true);

    int factoryCalls = 0;
    mixxx::qml::QmlDlgPreferencesProxy proxy([&]() {
        ++factoryCalls;
        return pDialog;
    });

    EXPECT_EQ(0, factoryCalls);
    proxy.show();
    EXPECT_EQ(1, factoryCalls);
    proxy.show();
    EXPECT_EQ(1, factoryCalls);
    pDialog->hide();
}

} // namespace
