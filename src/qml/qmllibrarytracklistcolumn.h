#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QVariant>

#include "library/columncache.h"
#include "qml/qml_owned_ptr.h"

namespace mixxx {
namespace qml {

class QmlLibraryTrackListColumn : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString key MEMBER m_key FINAL)
    Q_PROPERTY(QString label MEMBER m_label FINAL)
    Q_PROPERTY(int fillSpan MEMBER m_fillSpan FINAL)
    Q_PROPERTY(int columnIdx MEMBER m_columnIdx FINAL)
    Q_PROPERTY(double preferredWidth MEMBER m_preferredWidth FINAL)
    Q_PROPERTY(double autoHideWidth MEMBER m_autoHideWidth FINAL)
    Q_PROPERTY(bool hidden MEMBER m_hidden FINAL)
    Q_PROPERTY(QQmlComponent* delegate READ delegate WRITE setDelegate FINAL)
    Q_PROPERTY(Role role MEMBER m_role FINAL)
    QML_NAMED_ELEMENT(TrackListColumn)
  public:
    enum class SQLColumns {
        Album = ColumnCache::COLUMN_LIBRARYTABLE_ALBUM,
        Artist = ColumnCache::COLUMN_LIBRARYTABLE_ARTIST,
        Title = ColumnCache::COLUMN_LIBRARYTABLE_TITLE,
        Year = ColumnCache::COLUMN_LIBRARYTABLE_YEAR,
        Genre = ColumnCache::COLUMN_LIBRARYTABLE_GENRE,
        Bpm = ColumnCache::COLUMN_LIBRARYTABLE_BPM,
        Key = ColumnCache::COLUMN_LIBRARYTABLE_KEY,
        FileType = ColumnCache::COLUMN_LIBRARYTABLE_FILETYPE,
        Bitrate = ColumnCache::COLUMN_LIBRARYTABLE_BITRATE,
        AlbumArtist = ColumnCache::COLUMN_LIBRARYTABLE_ALBUMARTIST,
        Composer = ColumnCache::COLUMN_LIBRARYTABLE_COMPOSER,
        Grouping = ColumnCache::COLUMN_LIBRARYTABLE_GROUPING,
        TrackNumber = ColumnCache::COLUMN_LIBRARYTABLE_TRACKNUMBER,
        Comment = ColumnCache::COLUMN_LIBRARYTABLE_COMMENT,
        Duration = ColumnCache::COLUMN_LIBRARYTABLE_DURATION,
        DateAdded = ColumnCache::COLUMN_LIBRARYTABLE_DATETIMEADDED,
        TimesPlayed = ColumnCache::COLUMN_LIBRARYTABLE_TIMESPLAYED,
        LastPlayed = ColumnCache::COLUMN_LIBRARYTABLE_LAST_PLAYED_AT,
        ReplayGain = ColumnCache::COLUMN_LIBRARYTABLE_REPLAYGAIN,
    };
    Q_ENUM(SQLColumns)
    enum class Role {
        Location,
        Artist,
        Title,
        Cover,
    };
    Q_ENUM(Role)
    explicit QmlLibraryTrackListColumn(QObject* parent = nullptr)
            : QObject(parent) {
    }
    explicit QmlLibraryTrackListColumn(QObject* parent,
            const QString& key,
            const QString& label,
            int fillSpan,
            int columnIdx,
            double preferredWidth,
            double autoHideWidth,
            QQmlComponent* delegate,
            Role role,
            bool hidden = false);
    const QString& key() const {
        return m_key;
    }
    const QString& label() const {
        return m_label;
    }
    bool hidden() const {
        return m_hidden;
    }
    void setHidden(bool hidden) {
        m_hidden = hidden;
    }
    Role role() const {
        return m_role;
    }
    int fillSpan() const {
        return m_fillSpan;
    }
    int columnIdx() const {
        return m_columnIdx;
    }
    double preferredWidth() const {
        return m_preferredWidth;
    }
    double autoHideWidth() const {
        return m_autoHideWidth;
    }
    QQmlComponent* delegate() const {
        return m_pDelegate;
    }
    void setDelegate(QQmlComponent* delegate) {
        m_pDelegate = qml_owned_ptr(delegate);
    }

  private:
    QString m_key;
    QString m_label;
    Role m_role;
    int m_fillSpan{0};
    int m_columnIdx{-1};
    double m_preferredWidth{-1};
    double m_autoHideWidth{-1};
    bool m_hidden{false};
    qml_owned_ptr<QQmlComponent> m_pDelegate;
};
} // namespace qml
} // namespace mixxx
