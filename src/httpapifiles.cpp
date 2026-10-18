#include "httpapifiles.h"

#include "httpapijson.h"
#include "proconvert.h"

#include <QColor>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QUuid>

namespace httpapifiles {

namespace {

// A colour from the file as ProPresenter gives it: its own numbers, which are kept as
// floats (0.4 is 0.3999999761581421), not by way of a QColor's eight bits
QVariantMap colorOf(const rv::data::Color &color)
{
    return {{"red", double(color.red())}, {"green", double(color.green())}, {"blue", double(color.blue())}, {"alpha", double(color.alpha())}};
}

QVariantMap slideOf(const rv::data::Cue &cue)
{
    QVariantMap made {{"enabled", cue.isenabled()}, {"notes", QString()}, {"text", QString()},
                      {"label", QString::fromStdString(cue.name())}, {"size", QVariantMap {{"width", 0}, {"height", 0}}}};
    // The words, notes and size of the cue's slide, if it has one (a cue may only play media)
    for (const rv::data::Action &action : cue.actions()) {
        if (!action.has_slide() || !action.slide().has_presentation())
            continue;
        const rv::data::Slide &slide = action.slide().presentation().base_slide();
        const QVariantMap map = proconvert::toSlideMap(slide, QString());
        made.insert("text", map.value("plainText").toString());
        made.insert("size", QVariantMap {{"width", qRound(slide.size().width())}, {"height", qRound(slide.size().height())}});
        QVariantMap notes;
        proconvert::addNotes(action.slide().presentation(), &notes);
        made.insert("notes", notes.value("notesText").toString());
        break;
    }
    return made;
}

QVariantMap read(const QString &path)
{
    rv::data::Presentation file;
    QString error;
    if (!proconvert::readPresentation(path, &file, &error))
        return {};
    QHash<std::string, const rv::data::Cue *> cues;
    for (const rv::data::Cue &cue : file.cues())
        cues.insert(cue.uuid().string(), &cue);
    QVariantList groups;
    QHash<std::string, int> cuesOf;
    for (const auto &group : file.cue_groups()) {
        QVariantList slides;
        for (const rv::data::UUID &cueId : group.cue_identifiers()) {
            if (const rv::data::Cue *cue = cues.value(cueId.string()))
                slides << slideOf(*cue);
        }
        cuesOf.insert(group.group().uuid().string(), int(slides.size()));
        groups << QVariantMap {{"name", QString::fromStdString(group.group().name())},
                               {"color", colorOf(group.group().color())},
                               {"slides", slides},
                               {"uuid", QString::fromStdString(group.group().uuid().string())}};
    }
    QVariantList arrangements;
    for (int i = 0; i < file.arrangements_size(); ++i) {
        const auto &arrangement = file.arrangements(i);
        QStringList ids;
        int total = 0;
        for (const rv::data::UUID &groupId : arrangement.group_identifiers()) {
            ids << QString::fromStdString(groupId.string());
            total += cuesOf.value(groupId.string());
        }
        arrangements << QVariantMap {{"id", httpapijson::id(QString::fromStdString(arrangement.uuid().string()),
                                                            QString::fromStdString(arrangement.name()), i)},
                                     {"groups", ids},
                                     {"total_cues", total}};
    }
    const QString name = file.name().empty() ? QFileInfo(path).completeBaseName() : QString::fromStdString(file.name());
    return {{"id", httpapijson::id(QString::fromStdString(file.uuid().string()), name, httpapijson::noIndex)},
            {"groups", groups},
            // (The app plays no timeline, so none is offered.)
            {"has_timeline", false},
            {"presentation_path", path},
            {"destination", file.content_destination() == rv::data::Action::CONTENT_DESTINATION_ANNOUNCEMENTS ? QStringLiteral("announcements")
                                                                                                              : QStringLiteral("presentation")},
            {"arrangements", arrangements},
            {"current_arrangement", file.has_selected_arrangement() ? QString::fromStdString(file.selected_arrangement().string()) : QString()},
            {"type", QStringLiteral("standard")},
            {"is_authorized", true},
            {"total_cues", file.cues_size()}};
}

}

namespace {

// What is kept of a file, for as long as it is unchanged
template<typename T>
struct ByFile
{
    struct Kept
    {
        QDateTime modified;
        qint64 size = -1;
        T made;
    };
    QHash<QString, Kept> kept;

    // The thing made of the file by `make`, made again only if the file has changed
    template<typename Make>
    T get(const QString &path, Make make)
    {
        // (A long service opens many: what was kept of them all is let go of now and then.)
        if (kept.size() > 2000)
            kept.clear();
        const QFileInfo info(path);
        if (!info.exists())
            return T();
        Kept &entry = kept[path];
        if (entry.modified != info.lastModified() || entry.size != info.size()) {
            entry.made = make(path);
            entry.modified = info.lastModified();
            entry.size = info.size();
        }
        return entry.made;
    }
};

// A varint at `at` in `bytes`, moving `at` past it; false if it runs off the end
bool varint(const QByteArray &bytes, qsizetype &at, quint64 *value)
{
    quint64 made = 0;
    for (int shift = 0; shift < 64 && at < bytes.size(); shift += 7) {
        const uchar byte = uchar(bytes[at++]);
        made |= quint64(byte & 0x7f) << shift;
        if (!(byte & 0x80)) {
            *value = made;
            return true;
        }
    }
    return false;
}

// The UUID of a presentation from the first bytes of its file: field 2 of the message, a
// UUID message whose field 1 is the string. "" if it is not within them.
QString uuidFromStart(const QByteArray &bytes)
{
    qsizetype at = 0;
    while (at < bytes.size()) {
        quint64 tag = 0;
        if (!varint(bytes, at, &tag))
            return {};
        const int field = int(tag >> 3);
        const int wire = int(tag & 7);
        if (wire == 0) {
            quint64 ignored = 0;
            if (!varint(bytes, at, &ignored))
                return {};
        } else if (wire == 2) {
            quint64 length = 0;
            if (!varint(bytes, at, &length) || at + qsizetype(length) > bytes.size())
                return {};
            if (field == 2) {
                const QByteArray inner = bytes.mid(at, qsizetype(length));
                qsizetype in = 0;
                quint64 innerTag = 0;
                quint64 innerLength = 0;
                if (varint(inner, in, &innerTag) && innerTag == ((1 << 3) | 2) && varint(inner, in, &innerLength)
                    && in + qsizetype(innerLength) <= inner.size())
                    return QString::fromUtf8(inner.mid(in, qsizetype(innerLength)));
                return {};
            }
            at += qsizetype(length);
        } else if (wire == 5) {
            at += 4;
        } else if (wire == 1) {
            at += 8;
        } else {
            return {};
        }
    }
    return {};
}

QString readUuid(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QString quick = uuidFromStart(file.read(16 * 1024));
    if (!quick.isEmpty())
        return quick;
    rv::data::Presentation whole;
    QString error;
    return proconvert::readPresentation(path, &whole, &error) ? QString::fromStdString(whole.uuid().string()) : QString();
}

// A presentation's arrangements, by name: their UUIDs (for a playlist's rows, which
// name the one they follow)
QHash<QString, QString> readArrangements(const QString &path)
{
    rv::data::Presentation file;
    QString error;
    QHash<QString, QString> made;
    if (!proconvert::readPresentation(path, &file, &error))
        return made;
    for (const auto &arrangement : file.arrangements())
        made.insert(QString::fromStdString(arrangement.name()), QString::fromStdString(arrangement.uuid().string()));
    return made;
}

ByFile<QString> uuids;
ByFile<QHash<QString, QString>> arrangementsOf;

}

QString presentationUuid(const QString &path)
{
    return uuids.get(path, readUuid);
}

QString findPresentation(const QString &uuid, const QStringList &files)
{
    const QUuid wanted = QUuid::fromString(uuid);
    if (wanted.isNull())
        return {};
    for (const QString &file : files) {
        if (QUuid::fromString(presentationUuid(file)) == wanted)
            return file;
    }
    return {};
}

QVariantMap library(const QVariantList &rows)
{
    QVariantList items;
    for (const QVariant &entry : rows) {
        const QVariantMap row = entry.toMap();
        items << httpapijson::id(presentationUuid(row.value("file").toString()), row.value("name").toString(), items.size());
    }
    return {{"update_type", QStringLiteral("all")}, {"items", items}};
}

QVariantMap playlistItem(const QVariantMap &row, int index)
{
    const QString kind = row.value("kind").toString();
    QVariantMap made {{"id", httpapijson::id(row.value("path").toString(), row.value("name").toString(), index)},
                      {"type", kind == QLatin1String("header") ? QStringLiteral("header")
                               : kind == QLatin1String("presentation") ? QStringLiteral("presentation")
                                                                        : QStringLiteral("placeholder")},
                      {"is_hidden", false},
                      {"is_pco", false},
                      {"destination", QStringLiteral("presentation")}};
    if (kind == QLatin1String("header")) {
        const QColor color(row.value("color").toString());
        made.insert("header_color", httpapijson::color(color.isValid() ? color : QColor(Qt::transparent)));
    } else if (kind == QLatin1String("presentation")) {
        const QString file = row.value("file").toString();
        const QString arrangement = row.value("arrangement").toString();
        made.insert("presentation_info", QVariantMap {{"presentation_uuid", row.value("missing").toBool() ? QString() : presentationUuid(file)},
                                                      {"arrangement_name", arrangement},
                                                      {"arrangement_uuid", arrangement.isEmpty() ? QString()
                                                                                                 : arrangementsOf.get(file, readArrangements).value(arrangement)}});
    }
    return made;
}

QVariantList playlistItems(const QVariantList &rows)
{
    QVariantList made;
    for (const QVariant &row : rows)
        made << playlistItem(row.toMap(), int(made.size()));
    return made;
}

QVariantMap presentation(const QString &path, qint64 listIndex)
{
    // Read again only when the file has changed since
    struct Kept
    {
        QDateTime modified;
        qint64 size = -1;
        QVariantMap made;
    };
    static QHash<QString, Kept> kept;
    // (A long service opens many: what was kept of them all is let go of now and then.)
    if (kept.size() > 200)
        kept.clear();
    const QFileInfo info(path);
    if (!info.exists())
        return {};
    Kept &entry = kept[path];
    if (entry.modified != info.lastModified() || entry.size != info.size()) {
        entry.made = read(path);
        entry.modified = info.lastModified();
        entry.size = info.size();
    }
    if (entry.made.isEmpty())
        return {};
    QVariantMap made = entry.made;
    QVariantMap ownId = made.value("id").toMap();
    ownId.insert("index", double(listIndex));
    made.insert("id", ownId);
    return {{"presentation", made}};
}

}
