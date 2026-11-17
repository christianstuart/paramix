#include <gtest/gtest.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QScopedPointer>
#include <QVBoxLayout>

#include "preferences/dialog/preferencessearch.h"

namespace mixxx::preferences {
namespace {

class PreferencesSearchTest : public testing::Test {
  protected:
    void SetUp() override {
        m_pPage.reset(new QWidget);
        auto* pLayout = new QVBoxLayout(m_pPage.data());
        auto* pGroup = new QGroupBox(QStringLiteral("Routing"), m_pPage.data());
        pLayout->addWidget(pGroup);

        auto* pLabel = new QLabel(QStringLiteral("&Crossfader curve"), pGroup);
        m_pCombo = new QComboBox(pGroup);
        m_pCombo->addItem(QStringLiteral("Linear"));
        m_pCombo->addItem(QStringLiteral("Logarithmic"));
        pLabel->setBuddy(m_pCombo);

        m_pBufferCheckBox = new QCheckBox(
                QStringLiteral("Use buffer underrun protection"), pGroup);
        m_pBufferCheckBox->setToolTip(
                QStringLiteral("Prevents audio dropouts when the CPU is busy."));

        auto* pHidden = new QCheckBox(QStringLiteral("Hidden debug option"), pGroup);
        pHidden->hide();
        auto* pMarkup = new QLabel(
                QStringLiteral("<a href=\"x\">Manual</a>"), pGroup);
        Q_UNUSED(pMarkup);
        auto* pLongLabel = new QLabel(QString(130, QLatin1Char('x')), pGroup);
        Q_UNUSED(pLongLabel);

        m_index.addPage(QStringLiteral("Output"), m_pPage.data());
    }

    QString onlyMatchLabel(const QString& query) const {
        const auto results = m_index.search(query);
        if (results.size() != 1 || results[0].matches.size() != 1) {
            return QString();
        }
        return results[0].matches[0].label;
    }

    QScopedPointer<QWidget> m_pPage;
    QComboBox* m_pCombo = nullptr;
    QCheckBox* m_pBufferCheckBox = nullptr;
    SearchIndex m_index;
};

TEST_F(PreferencesSearchTest, FindsLabelAndBuddy) {
    const auto results = m_index.search(QStringLiteral("crossfader"));
    ASSERT_EQ(1, results.size());
    EXPECT_EQ(QStringLiteral("Output"), results[0].title);
    ASSERT_EQ(1, results[0].matches.size());
    EXPECT_EQ(QStringLiteral("Crossfader curve"), results[0].matches[0].label);
    EXPECT_EQ(m_pCombo, results[0].matches[0].pWidget);
}

TEST_F(PreferencesSearchTest, MatchesComboItemsThroughBuddy) {
    EXPECT_EQ(QStringLiteral("Crossfader curve"),
            onlyMatchLabel(QStringLiteral("logarithmic")));
}

TEST_F(PreferencesSearchTest, MatchesTooltips) {
    EXPECT_EQ(QStringLiteral("Use buffer underrun protection"),
            onlyMatchLabel(QStringLiteral("dropouts")));
}

TEST_F(PreferencesSearchTest, RequiresAllWords) {
    EXPECT_EQ(1, m_index.search(QStringLiteral("underrun protection")).size());
    EXPECT_TRUE(m_index.search(QStringLiteral("buffer crossfader")).isEmpty());
}

TEST_F(PreferencesSearchTest, SkipsHiddenMarkupAndLongText) {
    EXPECT_TRUE(m_index.search(QStringLiteral("hidden debug")).isEmpty());
    EXPECT_TRUE(m_index.search(QStringLiteral("manual")).isEmpty());
    EXPECT_TRUE(m_index.search(QStringLiteral("xxxxxxxx")).isEmpty());
}

TEST_F(PreferencesSearchTest, MatchesPageTitleOnly) {
    const auto results = m_index.search(QStringLiteral("output"));
    ASSERT_EQ(1, results.size());
    EXPECT_TRUE(results[0].matches.isEmpty());
}

} // namespace
} // namespace mixxx::preferences
