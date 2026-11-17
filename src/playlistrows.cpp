#include "playlistrows.h"

#include "playlist.pb.h"

#include <QColor>
#include <QHash>

namespace playlistrows {

namespace {

// A heading's colour as it is shown ("#rrggbb"), or "" for none: what a file means by
// no colour, or by one that is wholly see-through
QString shown(const rv::data::PlaylistItem::Header &header)
{
    if (!header.has_color() || header.color().alpha() <= 0)
        return {};
    const auto unit = [](float v) { return qBound(0.0f, v, 1.0f); };
    return QColor::fromRgbF(unit(header.color().red()), unit(header.color().green()), unit(header.color().blue())).name();
}

// A heading's colour written, or with "" taken out; false if the text is no colour
bool setColor(rv::data::PlaylistItem::Header *header, const QString &text)
{
    const QString wanted = text.trimmed();
    if (wanted.isEmpty()) {
        header->clear_color();
        return true;
    }
    const QColor color = QColor::fromString(wanted);
    if (!color.isValid() || !wanted.startsWith(u'#') || wanted.size() != 7)
        return false;
    rv::data::Color *made = header->mutable_color();
    made->set_red(color.redF());
    made->set_green(color.greenF());
    made->set_blue(color.blueF());
    made->set_alpha(1);
    return true;
}

// The colour given to a heading, only where it is another than it has
bool recolour(rv::data::PlaylistItem *item, const QString &color)
{
    rv::data::PlaylistItem::Header changed = item->header();
    if (!setColor(&changed, color))
        return false;
    if (shown(changed) != shown(item->header()))
        *item->mutable_header() = changed;
    return true;
}

}

bool makeHeader(rv::data::PlaylistItem *item, const QString &name, const QString &color, const std::string &id)
{
    item->mutable_uuid()->set_string(id);
    item->set_name(name.toStdString());
    return setColor(item->mutable_header(), color);
}

QString changeHeader(rv::data::PlaylistItem *item, const QString *name, const QString *color)
{
    if (!item->has_header())
        return QStringLiteral("That row is not a heading");
    if (color && !recolour(item, *color))
        return QStringLiteral("A heading's colour is written \"#rrggbb\"");
    if (name && item->name() != name->toStdString())
        item->set_name(name->toStdString());
    return {};
}

namespace {

// The arrangement a presentation's row is asked to show it in, put on the row where it
// is another than the row has
void arrange(rv::data::PlaylistItem *row, const Wanted &wanted)
{
    if (wanted.ownArrangement)
        return;
    rv::data::PlaylistItem::Presentation *presentation = row->mutable_presentation();
    if (wanted.arrangementName.isEmpty()) {
        if (presentation->has_arrangement() || !presentation->arrangement_name().empty()) {
            presentation->clear_arrangement();
            presentation->clear_arrangement_name();
        }
    } else if (presentation->arrangement().string() != wanted.arrangementId.toStdString()
               || presentation->arrangement_name() != wanted.arrangementName.toStdString()) {
        presentation->mutable_arrangement()->set_string(wanted.arrangementId.toStdString());
        presentation->set_arrangement_name(wanted.arrangementName.toStdString());
    }
}

}

QString insert(rv::data::Playlist *playlist, const QList<Wanted> &wanted, const QString &afterId,
               const std::function<rv::data::PlaylistItem(const QString &file)> &makeRow, const std::function<std::string()> &newId,
               QStringList *ids)
{
    // The rows first, so that nothing is added if one of them cannot be made
    QList<rv::data::PlaylistItem> rows;
    for (const Wanted &each : wanted) {
        rv::data::PlaylistItem row;
        if (each.kind == Wanted::Row)
            return QStringLiteral("A row the playlist has already is not one to add to it");
        if (each.kind == Wanted::Header) {
            if (!makeHeader(&row, each.name, each.color, newId()))
                return QStringLiteral("A heading's colour is written \"#rrggbb\"");
        } else {
            row = makeRow(each.file);
            arrange(&row, each);
        }
        rows.append(row);
    }
    auto *items = playlist->mutable_items()->mutable_items();
    int place = items->size();
    if (!afterId.isEmpty()) {
        place = -1;
        for (int i = 0; i < items->size(); ++i) {
            if (items->Get(i).uuid().string() == afterId.toStdString())
                place = i + 1;
        }
        if (place < 0)
            return QStringLiteral("The playlist has no row %1").arg(afterId);
    }
    QStringList made;
    for (const rv::data::PlaylistItem &row : std::as_const(rows)) {
        // Added at the end and walked back up to its place, as a row moved is
        *items->Add() = row;
        for (int i = items->size() - 1; i > place; --i)
            items->SwapElements(i, i - 1);
        ++place;
        made.append(QString::fromStdString(row.uuid().string()));
    }
    if (ids)
        *ids = made;
    return {};
}

QString set(rv::data::Playlist *playlist, const QList<Wanted> &wanted, const QStringList &files,
            const std::function<rv::data::PlaylistItem(const QString &file)> &makeRow, const std::function<std::string()> &newId,
            QStringList *ids)
{
    const google::protobuf::RepeatedPtrField<rv::data::PlaylistItem> had = playlist->items().items();
    QList<bool> taken(had.size(), false);
    const auto take = [&](const std::function<bool(int)> &fits) {
        for (int i = 0; i < had.size(); ++i) {
            if (!taken.at(i) && !had.Get(i).is_hidden() && fits(i)) {
                taken[i] = true;
                return i;
            }
        }
        return -1;
    };

    // Rows asked for by their ids first
    QList<int> byId(wanted.size(), -1);
    for (qsizetype i = 0; i < wanted.size(); ++i) {
        if (wanted.at(i).kind != Wanted::Row)
            continue;
        byId[i] = take([&](int at) { return had.Get(at).uuid().string() == wanted.at(i).id.toStdString(); });
        if (byId.at(i) < 0)
            return QStringLiteral("The playlist has no row %1").arg(wanted.at(i).id);
    }
    // The hidden rows, which nothing could have asked for, stay with the row each came
    // after: so a playlist given the rows it has, in the order it has them, is as it
    // was. Those that came before any other are first; one whose row is going goes last.
    QHash<int, QList<int>> following;
    int before = -1;
    for (int i = 0; i < had.size(); ++i) {
        if (had.Get(i).is_hidden())
            following[before].append(i);
        else
            before = i;
    }
    QList<rv::data::PlaylistItem> rows;
    QList<bool> placed(had.size(), false);
    const auto placeAfter = [&](int row) {
        for (const int hidden : following.value(row)) {
            rows.append(had.Get(hidden));
            placed[hidden] = true;
        }
    };
    placeAfter(-1);
    QStringList made;
    for (qsizetype i = 0; i < wanted.size(); ++i) {
        const Wanted &each = wanted.at(i);
        rv::data::PlaylistItem row;
        // Which of the rows there were it is, or -1 for a new one
        int from = -1;
        if (each.kind == Wanted::Row) {
            from = byId.at(i);
            row = had.Get(from);
        } else if (each.kind == Wanted::Header) {
            from = take([&](int index) { return had.Get(index).has_header() && had.Get(index).name() == each.name.toStdString(); });
            if (from >= 0) {
                row = had.Get(from);
            } else {
                row.mutable_uuid()->set_string(newId());
                row.set_name(each.name.toStdString());
                row.mutable_header();
            }
            if (!each.color.isNull() && !recolour(&row, each.color))
                return QStringLiteral("A heading's colour is written \"#rrggbb\"");
        } else {
            from = take([&](int index) {
                return had.Get(index).has_presentation() && !each.file.isEmpty() && files.value(index) == each.file;
            });
            row = from >= 0 ? had.Get(from) : makeRow(each.file);
            arrange(&row, each);
        }
        made.append(QString::fromStdString(row.uuid().string()));
        rows.append(row);
        if (from >= 0)
            placeAfter(from);
    }
    for (int i = 0; i < had.size(); ++i) {
        if (had.Get(i).is_hidden() && !placed.at(i))
            rows.append(had.Get(i));
    }

    auto *items = playlist->mutable_items()->mutable_items();
    items->Clear();
    for (const rv::data::PlaylistItem &row : rows)
        *items->Add() = row;
    if (ids)
        *ids = made;
    return {};
}

}
