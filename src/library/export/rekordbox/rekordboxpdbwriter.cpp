#include "library/export/rekordbox/rekordboxpdbwriter.h"

#include <algorithm>
#include <array>
#include <utility>

namespace {

constexpr int kPageHeaderLen = 0x28;
constexpr int kRowGroupLen = 0x24;
constexpr quint32 kNoPage = 0x03ff'ffff;
constexpr int kTrackFixedLen = 0x5e;
constexpr int kTrackStrings = 21;

void putLe16(QByteArray* out, int at, quint16 value) {
    (*out)[at] = static_cast<char>(value & 0xff);
    (*out)[at + 1] = static_cast<char>((value >> 8) & 0xff);
}

void putLe32(QByteArray* out, int at, quint32 value) {
    (*out)[at] = static_cast<char>(value & 0xff);
    (*out)[at + 1] = static_cast<char>((value >> 8) & 0xff);
    (*out)[at + 2] = static_cast<char>((value >> 16) & 0xff);
    (*out)[at + 3] = static_cast<char>((value >> 24) & 0xff);
}

void appendLe16(QByteArray* out, quint16 value) {
    out->append(static_cast<char>(value & 0xff));
    out->append(static_cast<char>((value >> 8) & 0xff));
}

void appendLe32(QByteArray* out, quint32 value) {
    out->append(static_cast<char>(value & 0xff));
    out->append(static_cast<char>((value >> 8) & 0xff));
    out->append(static_cast<char>((value >> 16) & 0xff));
    out->append(static_cast<char>((value >> 24) & 0xff));
}

bool isAscii(const QString& text) {
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i).unicode() > 0x7f) {
            return false;
        }
    }
    return true;
}

QByteArray shortAscii(const QString& text) {
    const QByteArray bytes = text.toUtf8();
    const int mangled = std::min(static_cast<int>((bytes.size() + 1) * 2 + 1), 0xff);
    QByteArray out;
    out.append(static_cast<char>(mangled));
    out.append(bytes);
    return out;
}

QByteArray longAscii(const QString& text) {
    const QByteArray bytes = text.toUtf8();
    const int length = 4 + static_cast<int>(bytes.size());
    QByteArray out;
    out.append(static_cast<char>(0x40));
    appendLe16(&out, static_cast<quint16>(std::min(length, 0xffff)));
    out.append('\0');
    out.append(bytes);
    return out;
}

QByteArray longUtf16le(const QString& text) {
    const int length = 4 + static_cast<int>(text.size() * 2);
    QByteArray out;
    out.append(static_cast<char>(0x90));
    appendLe16(&out, static_cast<quint16>(std::min(length, 0xffff)));
    out.append('\0');
    for (int i = 0; i < text.size(); ++i) {
        const quint16 unit = text.at(i).unicode();
        out.append(static_cast<char>(unit & 0xff));
        out.append(static_cast<char>((unit >> 8) & 0xff));
    }
    return out;
}

/// Appends a string the way rekordbox lays strings out in a row and returns
/// the row offset it starts at. Long strings start on a four-byte boundary.
int pushDeviceSqlString(QByteArray* row, const QString& text) {
    const QByteArray encoded = mixxx::rekordbox::PdbWriter::deviceSqlString(text);
    const quint8 flag = encoded.isEmpty() ? 0 : static_cast<quint8>(encoded.at(0));
    if (flag == 0x40 || flag == 0x90) {
        while (row->size() % 4 != 0) {
            row->append('\0');
        }
    }
    const int at = row->size();
    row->append(encoded);
    return at;
}

void setIndexShift(QByteArray* row, quint16 slot) {
    putLe16(row, 2, static_cast<quint16>(std::min<quint32>(slot * 32, 0xffff)));
}

QByteArray indexPage(int pageSize,
        quint32 pageIndex,
        quint32 pageType,
        quint32 nextPage,
        bool hasDataPage,
        quint32 firstDataPage) {
    QByteArray page(pageSize, '\0');
    putLe32(&page, 0x04, pageIndex);
    putLe32(&page, 0x08, pageType);
    putLe32(&page, 0x0c, nextPage);
    putLe32(&page, 0x10, 1);
    page[0x1b] = static_cast<char>(0x64);
    putLe16(&page, 0x20, 0x1fff);
    putLe16(&page, 0x22, 0x1fff);
    putLe16(&page, 0x24, 0x03ec);
    putLe32(&page, kPageHeaderLen, pageIndex);
    putLe32(&page, kPageHeaderLen + 4, hasDataPage ? firstDataPage : kNoPage);
    putLe32(&page, kPageHeaderLen + 8, kNoPage);
    int at = kPageHeaderLen + 18;
    for (int i = 0; i < 1004 && at + 4 <= page.size(); ++i) {
        page[at] = static_cast<char>(0xff);
        page[at + 1] = static_cast<char>(0x1f);
        page[at + 2] = static_cast<char>(0xf8);
        page[at + 3] = static_cast<char>(0xff);
        at += 4;
    }
    if (at + 2 <= page.size()) {
        page[at] = static_cast<char>(0xff);
        page[at + 1] = static_cast<char>(0x1f);
    }
    return page;
}

/// One table page in the DeviceSQL page index format.
class PageBuilder {
  public:
    PageBuilder(int pageSize, quint32 pageIndex, quint32 pageType, quint32 nextPage)
            : m_pageSize(pageSize),
              m_pageIndex(pageIndex),
              m_pageType(pageType),
              m_nextPage(nextPage) {
    }

    bool fits(int rowLength) const {
        return freeSpace() >= rowLength + 8;
    }

    quint16 pushRow(const QByteArray& row) {
        while (m_heap.size() % 4 != 0) {
            m_heap.append('\0');
        }
        const quint16 offset = static_cast<quint16>(
                std::min(static_cast<int>(m_heap.size()), 0xffff));
        m_heap.append(row);
        m_offsets.append(offset);
        return offset;
    }

    int rowCount() const {
        return m_offsets.size();
    }

    QByteArray finish(quint32 sequence) const {
        QByteArray page(m_pageSize, '\0');
        const int numRows = m_offsets.size();
        const bool lastRowMarked = numRows > 0xff;

        putLe32(&page, 0x04, m_pageIndex);
        putLe32(&page, 0x08, m_pageType);
        putLe32(&page, 0x0c, m_nextPage);
        putLe32(&page, 0x10, sequence);

        const quint32 rows = static_cast<quint32>(numRows);
        const quint32 packed = (rows & 0x1fff) | ((rows & 0x7ff) << 13);
        page[0x18] = static_cast<char>(packed & 0xff);
        page[0x19] = static_cast<char>((packed >> 8) & 0xff);
        page[0x1a] = static_cast<char>((packed >> 16) & 0xff);
        page[0x1b] = static_cast<char>(0x24); // ordinary data page

        const int used = ((m_heap.size() + 3) / 4) * 4;
        const int groups = std::max(1, (numRows + 15) / 16);
        const int freeSpace = std::max(0,
                m_pageSize - kPageHeaderLen - used - (2 * numRows + 4 * groups));
        putLe16(&page, 0x1c, static_cast<quint16>(std::min(freeSpace, 0xffff)));
        putLe16(&page, 0x1e, static_cast<quint16>(std::min(used, 0xffff)));
        if (lastRowMarked) {
            putLe16(&page, 0x20, 1);
            putLe16(&page, 0x22, static_cast<quint16>(std::max(numRows - 1, 0)));
        } else {
            putLe16(&page, 0x20, static_cast<quint16>(numRows));
        }

        const int heapLength = std::min(
                static_cast<int>(m_heap.size()), m_pageSize - kPageHeaderLen);
        if (heapLength > 0) {
            std::copy(m_heap.cbegin(),
                    m_heap.cbegin() + heapLength,
                    page.begin() + kPageHeaderLen);
        }

        const int indexGroups = (numRows + 15) / 16;
        for (int group = 0; group < indexGroups; ++group) {
            const int base = m_pageSize - group * kRowGroupLen;
            const int inGroup = group < indexGroups - 1
                    ? 16
                    : numRows - group * 16;
            quint16 present = 0;
            for (int row = 0; row < inGroup; ++row) {
                present = static_cast<quint16>(present | (1u << row));
                const quint16 offset = m_offsets.at(group * 16 + row);
                const int at = base - (6 + 2 * row);
                if (at + 2 <= page.size()) {
                    putLe16(&page, at, offset);
                }
            }
            const quint16 second = !lastRowMarked
                    ? present
                    : (group == indexGroups - 1
                                      ? static_cast<quint16>(1u << (inGroup - 1))
                                      : 0);
            if (base - 4 + 2 <= page.size()) {
                putLe16(&page, base - 4, present);
            }
            if (base - 2 + 2 <= page.size()) {
                putLe16(&page, base - 2, second);
            }
        }
        return page;
    }

  private:
    int freeSpace() const {
        const int groups = std::max(1, static_cast<int>((m_offsets.size() + 15) / 16));
        const int alignedHeap = ((m_heap.size() + 3) / 4) * 4;
        return std::max(0,
                m_pageSize - kPageHeaderLen - alignedHeap - groups * kRowGroupLen);
    }

    int m_pageSize;
    quint32 m_pageIndex;
    quint32 m_pageType;
    quint32 m_nextPage;
    QByteArray m_heap;
    QVector<quint16> m_offsets;
};

struct TablePages {
    quint32 type = 0;
    QVector<QByteArray> pages;
    bool hasRows = false;
};

struct TableEntry {
    quint32 type = 0;
    quint32 first = 0;
    quint32 last = 0;
    quint32 candidate = 0;
};

TablePages makeTable(quint32 type, const QVector<QByteArray>& rows, bool numbered) {
    TablePages table;
    table.type = type;
    table.hasRows = !rows.isEmpty();
    PageBuilder builder(mixxx::rekordbox::PdbWriter::kPageSize, 0, type, 0);
    for (QByteArray row : rows) {
        if (!builder.fits(row.size())) {
            table.pages.append(builder.finish(0));
            builder = PageBuilder(mixxx::rekordbox::PdbWriter::kPageSize, 0, type, 0);
        }
        if (numbered) {
            setIndexShift(&row, static_cast<quint16>(builder.rowCount()));
        }
        builder.pushRow(row);
    }
    table.pages.append(builder.finish(0));
    return table;
}

bool isZero(const QByteArray& bytes) {
    for (char byte : bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

QByteArray buildFile(QVector<TablePages> tables) {
    const int pageSize = mixxx::rekordbox::PdbWriter::kPageSize;
    QByteArray out(pageSize, '\0');
    quint32 nextIndex = 1;
    quint32 sequence = 1;
    QVector<TableEntry> entries;
    QVector<QByteArray> pages;

    struct Held {
        int entry = 0;
        QVector<QByteArray> rest;
        int firstPagePos = 0;
    };
    QVector<Held> held;

    for (TablePages& table : tables) {
        const quint32 indexAt = nextIndex++;
        if (table.hasRows) {
            const quint32 firstData = nextIndex++;
            pages.append(indexPage(pageSize, indexAt, table.type, firstData, true, firstData));
            QByteArray first = table.pages.takeFirst();
            putLe32(&first, 0x04, firstData);
            const int pos = pages.size();
            pages.append(first);
            held.append(Held{static_cast<int>(entries.size()), table.pages, pos});
            entries.append(TableEntry{table.type, firstData, 0xffffffff, 0xffffffff});
        } else {
            const quint32 candidate = nextIndex++;
            pages.append(indexPage(pageSize, indexAt, table.type, candidate, false, kNoPage));
            pages.append(QByteArray(pageSize, '\0'));
            entries.append(TableEntry{table.type, indexAt, indexAt, candidate});
        }
    }

    // A table's further pages and its empty candidate are written after every
    // table got its index page and first data page. `property` (19) is written
    // first, then the rest in table order, as rekordbox lays a file out.
    std::stable_sort(held.begin(), held.end(), [&entries](const Held& lhs, const Held& rhs) {
        const auto key = [&entries](const Held& heldTable) {
            const quint32 type = entries.at(heldTable.entry).type;
            return std::pair<int, quint32>(type == 19 ? 0 : 1, type);
        };
        return key(lhs) < key(rhs);
    });

    for (Held& heldTable : held) {
        TableEntry& entry = entries[heldTable.entry];
        int lastPos = heldTable.firstPagePos;
        quint32 lastIndex = entry.first;
        ++sequence;
        putLe32(&pages[lastPos], 0x10, sequence);

        for (QByteArray page : heldTable.rest) {
            const quint32 index = nextIndex++;
            putLe32(&page, 0x04, index);
            ++sequence;
            putLe32(&page, 0x10, sequence);
            putLe32(&pages[lastPos], 0x0c, index);
            pages.append(page);
            lastPos = pages.size() - 1;
            lastIndex = index;
        }

        const quint32 candidate = nextIndex++;
        pages.append(QByteArray(pageSize, '\0'));
        putLe32(&pages[lastPos], 0x0c, candidate);
        entry.last = lastIndex;
        entry.candidate = candidate;
    }

    // rekordbox's files stop at the last page with anything in it; the header
    // then names candidates past the end.
    while (!pages.isEmpty() && isZero(pages.last())) {
        pages.removeLast();
    }

    putLe32(&out, 0x04, static_cast<quint32>(pageSize));
    putLe32(&out, 0x08, static_cast<quint32>(tables.size()));
    putLe32(&out, 0x0c, nextIndex);
    putLe32(&out, 0x10, 1);
    putLe32(&out, 0x14, sequence + 1);
    for (int i = 0; i < entries.size(); ++i) {
        const TableEntry& entry = entries.at(i);
        const int at = 28 + i * 16;
        putLe32(&out, at, entry.type);
        putLe32(&out, at + 4, entry.candidate);
        putLe32(&out, at + 8, entry.first);
        putLe32(&out, at + 12, entry.last);
    }

    for (const QByteArray& page : pages) {
        out.append(page);
    }
    return out;
}

QVector<QByteArray> rowsFromHex(const char* const* rows, int count) {
    QVector<QByteArray> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        out.append(QByteArray::fromHex(QByteArray(rows[i])));
    }
    return out;
}

/// The `columns` browse names rekordbox writes: 27 rows, UTF-16 names wrapped
/// in the `0xfffa`/`0xfffb` markers. Taken from rbxport's `rbl-pdb`
/// `reference.rs`.
const char* const kColumnsHex[] = {
        "0100800090120000faff470045004e0052004500fbff0000",
        "0200810090140000faff410052005400490053005400fbff",
        "0300820090120000faff41004c00420055004d00fbff0000",
        "0400830090120000faff54005200410043004b00fbff0000",
        "05008500900e0000faff420050004d00fbff0000",
        "0600860090140000faff52004100540049004e004700fbff",
        "0700870090100000faff5900450041005200fbff",
        "0800880090160000faff520045004d004900580045005200fbff0000",
        "0900890090120000faff4c004100420045004c00fbff0000",
        "0a008a0090260000faff4f0052004900470049004e0041004c002000410052005400490053005400fbff0000",
        "0b008b00900e0000faff4b0045005900fbff0000",
        "0c008d00900e0000faff430055004500fbff0000",
        "0d008e0090120000faff43004f004c004f005200fbff0000",
        "0e00920090100000faff540049004d004500fbff",
        "0f00930090160000faff4200490054005200410054004500fbff0000",
        "10009400901a0000faff460049004c00450020004e0041004d004500fbff0000",
        "1100840090180000faff50004c00410059004c00490053005400fbff",
        "1200980090200000faff48004f00540020004300550045002000420041004e004b00fbff",
        "1300950090160000faff48004900530054004f0052005900fbff0000",
        "1400910090140000faff530045004100520043004800fbff",
        "1500960090180000faff43004f004d004d0045004e0054005300fbff",
        "16008c00901c0000faff4400410054004500200041004400440045004400fbff",
        "1700970090220000faff44004a00200050004c0041005900200043004f0055004e005400fbff0000",
        "1800900090140000faff46004f004c00440045005200fbff",
        "1900a10090160000faff440045004600410055004c005400fbff0000",
        "1a00a20090180000faff41004c00500048004100420045005400fbff",
        "1b00aa0090180000faff4d00410054004300480049004e004700fbff",
};

/// The default `categories` rows, page type 17.
const char* const kCategoriesHex[] = {
        "0100010063010000",
        "0500060005010000",
        "0600070063010000",
        "0700080063010000",
        "0800090063010000",
        "09000a0063010000",
        "0a000b0063010000",
        "0d000f0063010000",
        "0e00130004010000",
        "0f00140006010000",
        "1000150063010000",
        "1200170063010000",
        "0200020002000100",
        "0300030003000200",
        "0400040001000300",
        "0b000c0063000400",
        "1100050063000500",
        "1300160063000600",
        "1400120063000700",
        "1b001a0063020800",
        "1800110063000900",
        "16001b0063000a00",
};

/// The default `sorts` rows, page type 18.
const char* const kSortsHex[] = {
        "0100060001000000",
        "1500070003000000",
        "0e00080001000000",
        "0800090001000000",
        "09000a0001000000",
        "0a000b0001000000",
        "0f000d0001000000",
        "0d000f0001000000",
        "1700100001000000",
        "1600110001000000",
        "1900000000010000",
        "1a00010000020000",
        "0200020000030000",
        "0300030000040000",
        "0500040000050000",
        "0600050000060000",
        "0b000c0000070000",
};

constexpr std::array<const char*, 8> kDefaultColorNames = {
        "Pink", "Red", "Orange", "Yellow", "Green", "Aqua", "Blue", "Purple"};

} // namespace

namespace mixxx {
namespace rekordbox {

QByteArray PdbWriter::deviceSqlString(const QString& text) {
    if (!isAscii(text)) {
        return longUtf16le(text);
    }
    if (text.size() < 0x7e) {
        return shortAscii(text);
    }
    return longAscii(text);
}

QByteArray PdbWriter::trackRow(const PdbTrack& track) {
    QVector<QString> strings(kTrackStrings);
    strings[0] = track.isrc;
    strings[7] = track.hotCueAutoLoad ? QStringLiteral("ON") : QString();
    strings[10] = track.dateCreated;
    strings[11] = track.releaseDate;
    strings[12] = track.mixName;
    strings[14] = track.analyzePath;
    strings[15] = track.dateAdded;
    strings[16] = track.comment;
    strings[17] = track.title;
    strings[19] = track.filename;
    strings[20] = track.filePath;

    QByteArray row(kTrackFixedLen + kTrackStrings * 2, '\0');
    putLe16(&row, 0x00, 0x24);
    putLe16(&row, 0x02, 0);
    putLe32(&row, 0x04, track.contentLink);
    putLe32(&row, 0x08, track.sampleRate);
    putLe32(&row, 0x0c, 0); // composer_id
    putLe32(&row, 0x10, track.fileSize);
    putLe32(&row, 0x14, track.masterContentId);
    putLe32(&row, 0x18, track.masterDbId);
    putLe32(&row, 0x1c, track.artworkId);
    putLe32(&row, 0x20, track.keyId);
    putLe32(&row, 0x24, 0); // original_artist_id
    putLe32(&row, 0x28, track.labelId);
    putLe32(&row, 0x2c, 0); // remixer_id
    putLe32(&row, 0x30, track.bitrate);
    putLe32(&row, 0x34, track.trackNumber);
    putLe32(&row, 0x38, track.tempoX100);
    putLe32(&row, 0x3c, track.genreId);
    putLe32(&row, 0x40, track.albumId);
    putLe32(&row, 0x44, track.artistId);
    putLe32(&row, 0x48, track.id);
    putLe16(&row, 0x4c, track.discNumber);
    putLe16(&row, 0x4e, track.playCount);
    putLe16(&row, 0x50, track.year);
    putLe16(&row, 0x52, track.sampleDepth);
    putLe16(&row, 0x54, track.durationSec);
    putLe16(&row, 0x56, 0x29);
    row[0x58] = static_cast<char>(track.colorId);
    row[0x59] = static_cast<char>(track.rating);
    putLe16(&row, 0x5a, audioFileType(track.filename));
    putLe16(&row, 0x5c, 3);

    QVector<quint16> offsets(kTrackStrings);
    for (int slot = 0; slot < kTrackStrings; ++slot) {
        offsets[slot] = static_cast<quint16>(
                std::min(pushDeviceSqlString(&row, strings.at(slot)), 0xffff));
    }
    for (int slot = 0; slot < kTrackStrings; ++slot) {
        putLe16(&row, kTrackFixedLen + slot * 2, offsets.at(slot));
    }
    return row;
}

QByteArray PdbWriter::artistRow(const PdbArtist& artist) {
    QByteArray row(10, '\0');
    putLe16(&row, 0x00, 0x60);
    putLe32(&row, 0x04, artist.id);
    row[0x08] = static_cast<char>(0x03);
    const int at = pushDeviceSqlString(&row, artist.name);
    row[0x09] = static_cast<char>(std::min(at, 0xff));
    return row;
}

QByteArray PdbWriter::albumRow(const PdbAlbum& album) {
    QByteArray row(0x16, '\0');
    putLe16(&row, 0x00, 0x80);
    putLe32(&row, 0x08, album.artistId);
    putLe32(&row, 0x0c, album.id);
    row[0x14] = static_cast<char>(0x03);
    const int at = pushDeviceSqlString(&row, album.name);
    row[0x15] = static_cast<char>(std::min(at, 0xff));
    return row;
}

QByteArray PdbWriter::genreRow(const PdbGenre& genre) {
    QByteArray row;
    appendLe32(&row, genre.id);
    row.append(deviceSqlString(genre.name));
    return row;
}

QByteArray PdbWriter::keyRow(const PdbKey& key) {
    QByteArray row;
    appendLe32(&row, key.id);
    appendLe32(&row, key.id);
    row.append(deviceSqlString(key.name));
    return row;
}

QByteArray PdbWriter::colorRow(const PdbColor& color) {
    QByteArray row(8, '\0');
    row[4] = static_cast<char>(color.id & 0xff);
    putLe16(&row, 5, color.id);
    row.append(deviceSqlString(color.name));
    return row;
}

QByteArray PdbWriter::playlistRow(const PdbPlaylistNode& node) {
    QByteArray row;
    appendLe32(&row, node.parentId);
    appendLe32(&row, 0);
    appendLe32(&row, node.sortOrder);
    appendLe32(&row, node.id);
    appendLe32(&row, node.isFolder ? 1 : 0);
    row.append(deviceSqlString(node.name));
    return row;
}

QByteArray PdbWriter::playlistEntryRow(const PdbPlaylistEntry& entry) {
    QByteArray row;
    appendLe32(&row, entry.entryIndex);
    appendLe32(&row, entry.trackId);
    appendLe32(&row, entry.playlistId);
    return row;
}

QByteArray PdbWriter::propertyRow(const PdbProperty& property) {
    if (property.createdDate.size() != 10 ||
            !isAscii(property.createdDate) ||
            !isAscii(property.dbVersion)) {
        return {};
    }
    QByteArray row;
    row.append(static_cast<char>(0x80));
    row.append(static_cast<char>(0x02));
    appendLe16(&row, 0);
    appendLe32(&row, property.contents);
    row.append('\0');
    row.append(static_cast<char>(property.backgroundColor));
    row.append('\0');
    row.append('\0');
    row.append(shortAscii(property.createdDate));
    row.append(static_cast<char>(0x19));
    row.append(static_cast<char>(0x1e));
    row.append(shortAscii(property.dbVersion));
    row.append(deviceSqlString(property.deviceName));
    row.append(QByteArray(8, '\0'));
    while (row.size() % 4 != 0) {
        row.append('\0');
    }
    return row;
}

quint16 PdbWriter::audioFileType(const QString& filename) {
    const int dot = filename.lastIndexOf(QLatin1Char('.'));
    if (dot < 0) {
        return 0;
    }
    const QString extension = filename.mid(dot + 1).toLower();
    if (extension == QLatin1String("mp3")) {
        return 1;
    }
    if (extension == QLatin1String("m4a") ||
            extension == QLatin1String("mp4") ||
            extension == QLatin1String("aac")) {
        return 4;
    }
    if (extension == QLatin1String("flac")) {
        return 5;
    }
    if (extension == QLatin1String("wav")) {
        return 11;
    }
    if (extension == QLatin1String("aif") || extension == QLatin1String("aiff")) {
        return 12;
    }
    return 0;
}

QByteArray PdbWriter::write(const PdbLibrary& library) {
    QVector<TablePages> tables;
    tables.reserve(20);

    QVector<QByteArray> trackRows;
    trackRows.reserve(library.tracks.size());
    for (const PdbTrack& track : library.tracks) {
        trackRows.append(trackRow(track));
    }
    tables.append(makeTable(0, trackRows, true));

    QVector<QByteArray> genreRows;
    for (const PdbGenre& genre : library.genres) {
        genreRows.append(genreRow(genre));
    }
    tables.append(makeTable(1, genreRows, false));

    QVector<QByteArray> artistRows;
    for (const PdbArtist& artist : library.artists) {
        artistRows.append(artistRow(artist));
    }
    tables.append(makeTable(2, artistRows, true));

    QVector<QByteArray> albumRows;
    for (const PdbAlbum& album : library.albums) {
        albumRows.append(albumRow(album));
    }
    tables.append(makeTable(3, albumRows, true));

    // Labels are not written yet; the table is still part of the file.
    tables.append(makeTable(4, {}, false));

    QVector<QByteArray> keyRows;
    for (const PdbKey& key : library.keys) {
        keyRows.append(keyRow(key));
    }
    tables.append(makeTable(5, keyRows, false));

    QVector<QByteArray> colorRows;
    for (int i = 0; i < static_cast<int>(kDefaultColorNames.size()); ++i) {
        const quint16 id = static_cast<quint16>(i + 1);
        QString name = QString::fromLatin1(kDefaultColorNames.at(i));
        for (const PdbColor& color : library.colors) {
            if (color.id == id) {
                name = color.name;
                break;
            }
        }
        colorRows.append(colorRow(PdbColor{id, name}));
    }
    tables.append(makeTable(6, colorRows, false));

    QVector<QByteArray> playlistRows;
    for (const PdbPlaylistNode& node : library.playlists) {
        playlistRows.append(playlistRow(node));
    }
    tables.append(makeTable(7, playlistRows, false));

    QVector<QByteArray> entryRows;
    for (const PdbPlaylistEntry& entry : library.entries) {
        entryRows.append(playlistEntryRow(entry));
    }
    tables.append(makeTable(8, entryRows, false));

    for (quint32 type = 9; type <= 15; ++type) {
        tables.append(makeTable(type, {}, false));
    }

    tables.append(makeTable(16, rowsFromHex(kColumnsHex, 27), false));
    tables.append(makeTable(17, rowsFromHex(kCategoriesHex, 22), false));
    tables.append(makeTable(18, rowsFromHex(kSortsHex, 17), false));

    PdbProperty property = library.property;
    property.contents = static_cast<quint32>(library.tracks.size());
    QVector<QByteArray> propertyRows;
    const QByteArray propertyBytes = propertyRow(property);
    if (!propertyBytes.isEmpty()) {
        propertyRows.append(propertyBytes);
    }
    tables.append(makeTable(19, propertyRows, true));

    return buildFile(tables);
}

} // namespace rekordbox
} // namespace mixxx
