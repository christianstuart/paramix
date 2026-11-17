#pragma once

#include <QList>
#include <QString>
#include <QWidget>

namespace mixxx::preferences {

struct SearchMatch {
    QString label;
    QWidget* pWidget;
};

struct SearchPageResult {
    QString title;
    QWidget* pPage;
    QList<SearchMatch> matches;
};

/// Index of the visible setting labels on the preference pages.
///
/// Labels, group boxes and buttons are indexed by their visible text. Tooltips
/// and the items of a label's buddy combo box are searchable but are not shown
/// as result rows. Index a page again when its visibility changes.
class SearchIndex {
  public:
    void clear();
    void addPage(const QString& title, QWidget* pPage);

    /// Returns the pages that match the query, in insertion order. Every word
    /// must occur in the page title or in a control's searchable text.
    QList<SearchPageResult> search(const QString& query) const;

  private:
    struct Entry {
        QString label;
        QString haystack;
        QWidget* pWidget;
    };

    struct Page {
        QString title;
        QWidget* pPage;
        QList<Entry> entries;
    };

    QList<Page> m_pages;
};

} // namespace mixxx::preferences
