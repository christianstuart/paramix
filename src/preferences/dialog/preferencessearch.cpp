#include "preferences/dialog/preferencessearch.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QSet>
#include <algorithm>

namespace mixxx::preferences {
namespace {

constexpr int kMaxSearchableLabelLength = 120;
constexpr int kMaxResultLabelLength = 60;

QString cleanText(const QString& text) {
    QString result = text;
    result.remove(QLatin1Char('&'));
    return result.simplified();
}

bool isSearchableText(const QString& text) {
    if (text.size() < 2 || text.size() > kMaxSearchableLabelLength) {
        return false;
    }
    return std::any_of(text.cbegin(), text.cend(), [](QChar c) {
        return c.isLetterOrNumber();
    });
}

QString resultLabel(const QString& text) {
    return text.size() > kMaxResultLabelLength
            ? text.left(kMaxResultLabelLength - 1) + QChar(0x2026)
            : text;
}

bool containsAllWords(const QString& text, const QStringList& words) {
    return std::all_of(words.cbegin(), words.cend(), [&text](const QString& word) {
        return text.contains(word);
    });
}

bool containsAnyWord(const QString& text, const QStringList& words) {
    return std::any_of(words.cbegin(), words.cend(), [&text](const QString& word) {
        return text.contains(word);
    });
}

} // namespace

void SearchIndex::clear() {
    m_pages.clear();
}

void SearchIndex::addPage(const QString& title, QWidget* pPage) {
    Page page{cleanText(title), pPage, {}};
    QSet<QString> seen;
    const auto widgets = pPage->findChildren<QWidget*>();
    for (QWidget* pWidget : widgets) {
        if (pWidget->isHidden()) {
            continue;
        }

        QString text;
        QString extraKeywords;
        QWidget* pTarget = pWidget;
        if (auto* pLabel = qobject_cast<QLabel*>(pWidget)) {
            text = pLabel->text();
            // Rich text is help content, not a setting name.
            if (text.contains(QLatin1Char('<'))) {
                continue;
            }
            if (QWidget* pBuddy = pLabel->buddy()) {
                pTarget = pBuddy;
                if (auto* pCombo = qobject_cast<QComboBox*>(pBuddy)) {
                    QStringList items;
                    for (int i = 0; i < pCombo->count(); ++i) {
                        items.append(pCombo->itemText(i));
                    }
                    extraKeywords = items.join(QLatin1Char(' '));
                }
            }
        } else if (auto* pGroupBox = qobject_cast<QGroupBox*>(pWidget)) {
            text = pGroupBox->title();
        } else if (auto* pButton = qobject_cast<QAbstractButton*>(pWidget)) {
            text = pButton->text();
        } else {
            continue;
        }

        text = cleanText(text);
        if (pTarget->isHidden() || !isSearchableText(text)) {
            continue;
        }

        const QString key = text + QChar(0x1f) +
                QString::number(reinterpret_cast<quintptr>(pTarget));
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);

        const QString haystack = cleanText(text + QLatin1Char(' ') + pWidget->toolTip() +
                QLatin1Char(' ') + pTarget->toolTip() + QLatin1Char(' ') +
                extraKeywords);
        page.entries.append({resultLabel(text), haystack.toCaseFolded(), pTarget});
    }
    m_pages.append(page);
}

QList<SearchPageResult> SearchIndex::search(const QString& query) const {
    QStringList words = query.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (QString& word : words) {
        word = word.toCaseFolded();
    }
    if (words.isEmpty()) {
        return {};
    }

    QList<SearchPageResult> results;
    for (const Page& page : m_pages) {
        const QString title = page.title.toCaseFolded();
        SearchPageResult result{page.title, page.pPage, {}};
        for (const Entry& entry : page.entries) {
            // Only list controls that match a word themselves; a page title
            // match alone keeps the result row but no child rows.
            const bool entryMatches = containsAnyWord(entry.haystack, words);
            if (entryMatches &&
                    containsAllWords(title + QLatin1Char(' ') + entry.haystack, words)) {
                result.matches.append({entry.label, entry.pWidget});
            }
        }
        if (containsAllWords(title, words) || !result.matches.isEmpty()) {
            results.append(result);
        }
    }
    return results;
}

} // namespace mixxx::preferences
