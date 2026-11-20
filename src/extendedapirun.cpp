#include "extendedapirun.h"

#include "actions.h"
#include "arrangementedit.h"
#include "ccli.h"
#include "chords.h"
#include "extendedapijson.h"
#include "httpapifiles.h"
#include "labelfile.h"
#include "presentationedit.h"
#include "proconvert.h"
#include "prodocument.h"
#include "sessionlog.h"
#include "slideedit.h"
#include "songselect.h"
#include "themefile.h"
#include "timeline.h"
#include "workspacefiles.h"

#include "template.pb.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace {

using Answer = ExtendedApi::Answer;
namespace json = extendedapijson;

Answer bad(const QString &why)
{
    return {400, json::error(why)};
}

Answer missing(const QString &why)
{
    return {404, json::error(why)};
}

Answer inTheWay(const QString &why)
{
    return {409, json::error(why)};
}

Answer failed(const QString &why)
{
    return {500, json::error(why)};
}

// How long a copy kept of a file stands for the changes that follow it, and how many of
// one file's are kept
constexpr int kKeptForSeconds = 600;
constexpr int kCopiesKept = 20;

// A presentation being changed: a copy of its file in a folder of its own, which every
// step reads and writes, and which takes the file's place at the end if all went well
// (see the header). The copy has the file's own name, so that what a step says of it
// ("Song.pro no longer has that slide") is said of the presentation.
class Work
{
public:
    explicit Work(const QString &file)
        : m_file(file), m_folder(QDir::tempPath() + QStringLiteral("/SimplePresenter-api-XXXXXX"))
    {
        if (!m_folder.isValid()) {
            m_error = QStringLiteral("A folder to work in could not be made: %1").arg(m_folder.errorString());
            return;
        }
        m_copy = m_folder.filePath(QFileInfo(file).fileName());
        if (QFileInfo::exists(file) && !QFile::copy(file, m_copy))
            m_error = QStringLiteral("%1 could not be read").arg(QFileInfo(file).fileName());
    }

    QString error() const { return m_error; }
    // The copy, for what changes a presentation by its file
    QString copy() const { return m_copy; }

    // A change made to the presentation as the file has it: read, changed, and written
    // back unless the change left it as it was
    QString change(const std::function<QString(rv::data::Presentation *)> &apply)
    {
        rv::data::Presentation presentation;
        QString error;
        if (!proconvert::readPresentation(m_copy, &presentation, &error))
            return error;
        const std::string before = presentation.SerializeAsString();
        error = apply(&presentation);
        if (!error.isEmpty() || presentation.SerializeAsString() == before)
            return error;
        return proconvert::writePresentation(m_copy, presentation);
    }

    // Whether the copy is no longer what the file is
    bool changed() const
    {
        QFile was(m_file);
        QFile now(m_copy);
        if (!now.open(QIODevice::ReadOnly))
            return false;
        if (!was.open(QIODevice::ReadOnly))
            return true;
        return was.readAll() != now.readAll();
    }

    // The copy put in the file's place, in one step
    QString finish()
    {
        QFile now(m_copy);
        if (!now.open(QIODevice::ReadOnly))
            return QStringLiteral("%1 could not be read back").arg(QFileInfo(m_file).fileName());
        const QByteArray bytes = now.readAll();
        QSaveFile target(m_file);
        if (!target.open(QIODevice::WriteOnly) || target.write(bytes) != bytes.size() || !target.commit())
            return QStringLiteral("Cannot write %1: %2").arg(QFileInfo(m_file).fileName(), target.errorString());
        SessionLog::write("saved", m_file);
        return {};
    }

private:
    QString m_file;
    QTemporaryDir m_folder;
    QString m_copy;
    QString m_error;
    // (What is saved on the way is the copy, which the log has no use for: it is told
    // when the file itself is, in finish())
    proconvert::UnloggedSaves m_quiet;
};

// The body of a request as a map: an empty one for no body. False for what is not a
// JSON object.
bool bodyOf(const QByteArray &body, QVariantMap *map)
{
    if (body.trimmed().isEmpty())
        return true;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;
    *map = document.object().toVariantMap();
    return true;
}

// A name of the body that the operation does not take, or "" for none. (An element
// and an action take properties by name beside their own: those are looked at by what
// knows them.)
QString unknownOf(const extendedapi::Operation &operation, const QVariantMap &body)
{
    QStringList known;
    for (const extendedapi::Field &field : operation.fields) {
        if (field.where == extendedapi::Field::Body) {
            if (field.name == "properties")
                return {};
            known.append(QString::fromLatin1(field.name));
        }
    }
    for (auto it = body.constBegin(); it != body.constEnd(); ++it) {
        if (!known.contains(it.key())) {
            return known.isEmpty() ? QStringLiteral("%1 takes nothing in its body, and was given `%2`").arg(QString::fromLatin1(operation.name), it.key())
                                   : QStringLiteral("%1 takes no `%2`: it takes %3").arg(QString::fromLatin1(operation.name), it.key(), known.join(QStringLiteral(", ")));
        }
    }
    return {};
}

// An id made of a name, for what ProPresenter's files give none (a theme, as the HTTP
// API's libraries: httpapijson.cpp)
QString idOf(const QString &name)
{
    static const QUuid space = QUuid::fromString(QStringLiteral("5D1A0E00-51DE-4A0D-8000-0000000000A2"));
    return QUuid::createUuidV5(space, name).toString(QUuid::WithoutBraces).toUpper();
}

// Which of a theme's slides is meant, by its id, its name or its number: its place, or -1
int themeSlideIndex(const rv::data::Template::Document &theme, const QString &which)
{
    for (int i = 0; i < theme.slides_size(); ++i) {
        if (QString::fromStdString(theme.slides(i).base_slide().uuid().string()) == which)
            return i;
    }
    for (int i = 0; i < theme.slides_size(); ++i) {
        if (!which.trimmed().isEmpty() && QString::fromStdString(theme.slides(i).name()).trimmed().compare(which.trimmed(), Qt::CaseInsensitive) == 0)
            return i;
    }
    bool isNumber = false;
    const int number = which.toInt(&isNumber);
    return isNumber && number >= 0 && number < theme.slides_size() ? number : -1;
}

// A slide's action as it is asked for, as actions::build takes one: the things of a
// `properties` map beside the rest, and the names turned to the app's
QVariantMap actionOf(const QVariantMap &asked, QString *error)
{
    QVariantMap sent = asked;
    const QVariant inner = sent.take(QStringLiteral("properties"));
    if (inner.typeId() == QMetaType::QVariantMap) {
        const QVariantMap properties = inner.toMap();
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            if (!sent.contains(it.key()))
                sent.insert(it.key(), it.value());
        }
    }
    QVariantMap made = json::named(sent);
    static const QStringList kinds {QStringLiteral("timer"), QStringLiteral("clear"), QStringLiteral("clearGroup"), QStringLiteral("stage"),
                                    QStringLiteral("prop"), QStringLiteral("look"), QStringLiteral("macro"), QStringLiteral("message"),
                                    QStringLiteral("audio"), QStringLiteral("media"), QStringLiteral("mediaPlaylist"),
                                    QStringLiteral("audioPlaylist"), QStringLiteral("videoInput"), QStringLiteral("communication")};
    if (!kinds.contains(made.value(QStringLiteral("kind")).toString())) {
        *error = QStringLiteral("An action's `kind` is one of timer, clear, clear_group, stage, prop, look, macro, message, audio, media, "
                                "media_playlist, audio_playlist, video_input and communication");
        return {};
    }
    // A timer's action by its word, as well as by ProPresenter's number
    if (made.value(QStringLiteral("kind")).toString() == QLatin1String("timer") && made.value(QStringLiteral("action")).typeId() == QMetaType::QString) {
        static const QStringList words {QStringLiteral("start"), QStringLiteral("stop"), QStringLiteral("reset"), QStringLiteral("reset_and_start"),
                                        QStringLiteral("stop_and_reset"), QStringLiteral("increment")};
        const int action = int(words.indexOf(made.value(QStringLiteral("action")).toString().trimmed().toLower()));
        if (action < 0) {
            *error = QStringLiteral("A timer action's `action` is one of start, stop, reset, reset_and_start, stop_and_reset and increment");
            return {};
        }
        made.insert(QStringLiteral("action"), action);
    }
    // A layer to clear by its word
    if (made.value(QStringLiteral("kind")).toString() == QLatin1String("clear") && made.value(QStringLiteral("layer")).typeId() == QMetaType::QString) {
        static const QHash<QString, int> layers {{QStringLiteral("all"), 0}, {QStringLiteral("audio"), 1}, {QStringLiteral("media"), 2},
                                                 {QStringLiteral("props"), 4}, {QStringLiteral("slide"), 5}, {QStringLiteral("audio_effects"), 8}};
        const QString layer = made.value(QStringLiteral("layer")).toString().trimmed().toLower();
        if (layer == QLatin1String("announcements")) {
            made.insert(QStringLiteral("announcements"), true);
            made.insert(QStringLiteral("layer"), 0);
        } else if (layers.contains(layer)) {
            made.insert(QStringLiteral("layer"), layers.value(layer));
        } else {
            *error = QStringLiteral("A clear action's `layer` is one of all, audio, media, props, slide, audio_effects and announcements");
            return {};
        }
    }
    return made;
}

// A format for many slides' text as it is asked for, as textformat takes one
QVariantMap manyFormat(const QVariantMap &asked, QString *error)
{
    static const QStringList known {
        QStringLiteral("family"), QStringLiteral("style"), QStringLiteral("color"), QStringLiteral("capitalization"), QStringLiteral("size"),
        QStringLiteral("sizeBy"), QStringLiteral("strokeColor"), QStringLiteral("strokeWidth"), QStringLiteral("strokeOn"),
        QStringLiteral("strokeWidthBy"), QStringLiteral("alignment"), QStringLiteral("verticalAlignment"), QStringLiteral("textShadowEnabled"),
        QStringLiteral("textShadowRadius"), QStringLiteral("textShadowOpacity"), QStringLiteral("textShadowSolidColor"),
        QStringLiteral("textShadowAngle"), QStringLiteral("textShadowOffset"), QStringLiteral("textShadowRadiusBy"),
        QStringLiteral("textShadowOpacityBy"), QStringLiteral("textShadowAngleBy"), QStringLiteral("textShadowOffsetBy"),
    };
    QVariantMap made;
    for (auto it = asked.constBegin(); it != asked.constEnd(); ++it) {
        const QString key = json::camel(it.key());
        if (!known.contains(key)) {
            QStringList names;
            for (const QString &each : known)
                names.append(json::snake(each));
            *error = QStringLiteral("A format for many slides has no `%1`: it has %2").arg(it.key(), names.join(QStringLiteral(", ")));
            return {};
        }
        QVariant value = it.value();
        if (key.endsWith(QLatin1String("Color"))) {
            QColor color;
            if (!json::colorOf(value, &color)) {
                *error = QStringLiteral("`%1` is not a colour").arg(it.key());
                return {};
            }
            value = color;
        } else if (key == QLatin1String("alignment") || key == QLatin1String("verticalAlignment") || key == QLatin1String("capitalization")) {
            const int number = key == QLatin1String("alignment") ? json::alignmentOf(value)
                               : key == QLatin1String("verticalAlignment") ? json::verticalOf(value) : json::capitalsOf(value);
            if (number < 0) {
                *error = QStringLiteral("`%1` is not one of the words it can be").arg(it.key());
                return {};
            }
            value = number;
        }
        made.insert(key, value);
    }
    return made;
}

// A media file as it is asked for: where it is, or for a path that does not begin at
// the top of the disk, that path under the workspace's Media folder
QString mediaFile(const QString &asked, const QString &mediaFolder)
{
    if (asked.trimmed().isEmpty() || QDir::isAbsolutePath(asked))
        return asked;
    return QDir::cleanPath(QDir(mediaFolder).absoluteFilePath(asked));
}

// What was sent with every media file in it named in full: an element's `file` and
// `fill_media_path`, a slide's `media` (a path, or its `file`), wherever in it they are
QVariant withMedia(const QVariant &sent, const QString &mediaFolder, const QString &key = QString())
{
    if (sent.typeId() == QMetaType::QVariantMap) {
        QVariantMap made;
        const QVariantMap map = sent.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            made.insert(it.key(), withMedia(it.value(), mediaFolder, it.key()));
        return made;
    }
    if (sent.typeId() == QMetaType::QVariantList) {
        QVariantList made;
        for (const QVariant &each : sent.toList())
            made.append(withMedia(each, mediaFolder));
        return made;
    }
    if (sent.typeId() == QMetaType::QString
        && (key == QLatin1String("file") || key == QLatin1String("fill_media_path") || key == QLatin1String("media")))
        return mediaFile(sent.toString(), mediaFolder);
    return sent;
}

// A song's words as the groups create_presentation takes: each part a group, its lines
// cut into slides of so many
QVariantList lyricGroups(const songselect::Song &song, int linesPerSlide)
{
    QVariantList groups;
    for (const chords::Section &section : song.sections) {
        QVariantList slides;
        for (const QList<chords::Line> &lines : chords::slidesOf(section, linesPerSlide)) {
            QStringList words;
            for (const chords::Line &line : lines)
                words.append(line.text);
            slides.append(words.join(u'\n'));
        }
        groups.append(QVariantMap {{QStringLiteral("name"), section.name}, {QStringLiteral("slides"), slides}});
    }
    return groups;
}

}

ExtendedApi::ExtendedApi(QObject *parent)
    : QObject(parent)
{
}

void ExtendedApi::setCatalog(Catalog *catalog)
{
    if (m_catalog == catalog)
        return;
    m_catalog = catalog;
    emit catalogChanged();
}

void ExtendedApi::setGroups(const QVariantList &groups)
{
    if (m_groups == groups)
        return;
    m_groups = groups;
    emit groupsChanged();
}

void ExtendedApi::setBusy(const QStringList &busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

QObject *ExtendedApi::search() const
{
    return m_search;
}

void ExtendedApi::setSearch(QObject *search)
{
    if (m_search == search)
        return;
    m_search = search;
    emit searchChanged();
}

QString ExtendedApi::root() const
{
    return m_catalog ? m_catalog->workspacePath() : QString();
}

bool ExtendedApi::isBusy(const QString &file) const
{
    return m_busy.contains(file) || m_busy.contains(QFileInfo(file).canonicalFilePath());
}

namespace {

// What a file is, in a few bytes: for knowing whether it is still what it was
QByteArray fingerprint(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha1);
}

}

void ExtendedApi::keep(const QString &file)
{
    if (!QFileInfo::exists(file))
        return;
    if (!m_changing.contains(file))
        m_changing.append(file);
    const QDateTime now = QDateTime::currentDateTime();
    Kept kept = m_kept.value(file);
    // As the API left it, and a copy kept of it lately: that copy stands for this
    // change too. Otherwise (somebody has changed it since, or it has been a while,
    // or this is the first change to it) a copy as it is now.
    if (kept.when.isValid() && kept.when.secsTo(now) < kKeptForSeconds && kept.left == fingerprint(file))
        return;
    workspace::keepCopy(file, root(), QStringLiteral("the API"), kCopiesKept);
    kept.when = now;
    m_kept.insert(file, kept);
}

QString ExtendedApi::reference(const QString &topic)
{
    QFile file(QStringLiteral(":/docs/features/extended-api.md"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QString text = QString::fromUtf8(file.readAll());
    // (From its first heading: what is before is the repository's way back to the README)
    const qsizetype start = text.indexOf(QLatin1String("\n# "));
    if (start >= 0)
        text = text.mid(start + 1);
    if (topic.trimmed().isEmpty())
        return text;
    // The part under the heading that names the topic: "## Elements" for "elements",
    // up to the next heading of its rank
    const QStringList lines = text.split(u'\n');
    QStringList part;
    bool inside = false;
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("## "))) {
            if (inside)
                break;
            inside = line.mid(3).trimmed().toLower().startsWith(topic.trimmed().toLower());
        }
        if (inside)
            part.append(line);
    }
    return part.join(u'\n');
}

// ---- finding things

QString ExtendedApi::libraryPath(const QString &name) const
{
    for (const QVariant &each : m_catalog->libraries()) {
        const QVariantMap library = each.toMap();
        if (library.value(QStringLiteral("name")).toString() == name)
            return library.value(QStringLiteral("path")).toString();
    }
    for (const QVariant &each : m_catalog->libraries()) {
        const QVariantMap library = each.toMap();
        if (library.value(QStringLiteral("name")).toString().compare(name.trimmed(), Qt::CaseInsensitive) == 0)
            return library.value(QStringLiteral("path")).toString();
    }
    return {};
}

QString ExtendedApi::libraryOf(const QString &file) const
{
    return QFileInfo(file).dir().dirName();
}

ExtendedApi::Found ExtendedApi::presentation(const QString &which, const QString &library) const
{
    Found found;
    if (which.trimmed().isEmpty()) {
        found.error = QStringLiteral("Which presentation is not said");
        found.status = 400;
        return found;
    }
    QStringList folders;
    if (!library.isEmpty()) {
        const QString folder = libraryPath(library);
        if (folder.isEmpty()) {
            found.error = QStringLiteral("The workspace has no library “%1”").arg(library);
            return found;
        }
        folders.append(folder);
    } else {
        for (const QVariant &each : m_catalog->libraries())
            folders.append(each.toMap().value(QStringLiteral("path")).toString());
    }
    QList<QFileInfo> files;
    for (const QString &folder : folders)
        files += workspace::sortedEntries(folder, {QStringLiteral("*.pro")}, QDir::Files);

    // By its uuid, which a file has near its start (and which is asked of every file
    // only for what could be one)
    if (!QUuid::fromString(which.trimmed()).isNull()) {
        for (const QFileInfo &file : files) {
            if (httpapifiles::presentationUuid(file.absoluteFilePath()).compare(which.trimmed(), Qt::CaseInsensitive) == 0) {
                found.path = file.absoluteFilePath();
                found.library = file.dir().dirName();
                return found;
            }
        }
    }
    // By its name: as it is written, or failing that whatever its capitals
    QList<QFileInfo> named;
    for (const Qt::CaseSensitivity capitals : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
        for (const QFileInfo &file : files) {
            if (file.completeBaseName().compare(which.trimmed(), capitals) == 0)
                named.append(file);
        }
        if (!named.isEmpty())
            break;
    }
    if (named.isEmpty()) {
        found.error = QStringLiteral("There is no presentation “%1”%2").arg(which, library.isEmpty() ? QString() : QStringLiteral(" in the library “%1”").arg(library));
        return found;
    }
    if (named.size() > 1) {
        QStringList libraries;
        for (const QFileInfo &file : named)
            libraries.append(file.dir().dirName());
        found.error = QStringLiteral("There is a presentation “%1” in each of the libraries %2: say which with `library`, or name it by its uuid")
                          .arg(which, libraries.join(QStringLiteral(", ")));
        found.status = 409;
        return found;
    }
    found.path = named.first().absoluteFilePath();
    found.library = named.first().dir().dirName();
    return found;
}

ExtendedApi::Found ExtendedApi::presentationOf(const QVariantMap &params, const QVariantMap &query) const
{
    return presentation(params.value(QStringLiteral("presentation")).toString(), query.value(QStringLiteral("library")).toString());
}

ExtendedApi::ThemeFound ExtendedApi::theme(const QString &which) const
{
    ThemeFound found;
    const QStringList places = themefile::places(root());
    for (const QString &place : places) {
        if (place == which || idOf(place).compare(which.trimmed(), Qt::CaseInsensitive) == 0) {
            found.place = place;
            return found;
        }
    }
    QStringList named;
    for (const QString &place : places) {
        if (place.section(u'/', -1).compare(which.trimmed(), Qt::CaseInsensitive) == 0)
            named.append(place);
    }
    if (named.size() == 1) {
        found.place = named.first();
    } else if (named.isEmpty()) {
        found.error = QStringLiteral("The workspace has no theme “%1”").arg(which);
    } else {
        found.error = QStringLiteral("More than one theme is called “%1”: %2. Name it by its place or its id.").arg(which, named.join(QStringLiteral(", ")));
    }
    return found;
}

QString ExtendedApi::playlist(const QString &which, bool *folder) const
{
    const QVariantList nodes = m_catalog->playlists();
    const auto found = [&](const QVariantMap &node) {
        if (folder)
            *folder = node.value(QStringLiteral("folder")).toBool();
        return node.value(QStringLiteral("path")).toString();
    };
    for (const QVariant &each : nodes) {
        if (each.toMap().value(QStringLiteral("path")).toString().compare(which.trimmed(), Qt::CaseInsensitive) == 0)
            return found(each.toMap());
    }
    for (const Qt::CaseSensitivity capitals : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
        for (const QVariant &each : nodes) {
            if (each.toMap().value(QStringLiteral("name")).toString().compare(which.trimmed(), capitals) == 0)
                return found(each.toMap());
        }
    }
    return {};
}

QString ExtendedApi::playlistItem(const QString &playlist, const QString &which) const
{
    const QVariantList rows = m_catalog->playlistItems(playlist);
    for (const QVariant &each : rows) {
        if (each.toMap().value(QStringLiteral("path")).toString().compare(which.trimmed(), Qt::CaseInsensitive) == 0)
            return each.toMap().value(QStringLiteral("path")).toString();
    }
    bool isNumber = false;
    const int number = which.toInt(&isNumber);
    return isNumber && number >= 0 && number < rows.size() ? rows.at(number).toMap().value(QStringLiteral("path")).toString() : QString();
}

// ---- what is answered

QVariantMap ExtendedApi::described(const QString &path, bool full, QString *error) const
{
    rv::data::Presentation presentation;
    if (!proconvert::readPresentation(path, &presentation, error))
        return {};
    QVariantMap document = m_catalog->openArranged(path, QString());
    QStringList names;
    QString selected;
    ProDocument::arrangementsOf(path, &names, &selected);
    document.insert(QStringLiteral("selectedArrangement"), selected);
    return json::presentation(document, libraryOf(path), presentationedit::groupCues(presentation), full ? json::Full : json::Outline);
}

namespace {

// A presentation in a few words, for what a change to one is answered with
QVariantMap summaryOf(const QVariantMap &document, const QString &library, const rv::data::Presentation &presentation)
{
    return json::presentation(document, library, presentationedit::groupCues(presentation), json::Summary);
}

}

namespace {

// What a change to a presentation as a whole is answered with, once it is made: the
// presentation in a few words
QVariantMap summarised(const Catalog *catalog, const QString &path, const QString &library)
{
    rv::data::Presentation presentation;
    QString error;
    proconvert::readPresentation(path, &presentation, &error);
    QVariantMap document = catalog->openArranged(path, QString());
    QStringList names;
    QString selected;
    ProDocument::arrangementsOf(path, &names, &selected);
    document.insert(QStringLiteral("selectedArrangement"), selected);
    return summaryOf(document, library, presentation);
}

}

QVariantList ExtendedApi::themeList() const
{
    QVariantList themes;
    for (const QString &place : themefile::places(root())) {
        rv::data::Template::Document theme;
        QVariantList slides;
        if (themefile::read(root(), place, &theme, nullptr)) {
            for (const rv::data::Template::Slide &slide : theme.slides()) {
                slides.append(QVariantMap {{QStringLiteral("id"), QString::fromStdString(slide.base_slide().uuid().string())},
                                           {QStringLiteral("name"), QString::fromStdString(slide.name())}});
            }
        }
        themes.append(QVariantMap {{QStringLiteral("id"), idOf(place)}, {QStringLiteral("name"), place.section(u'/', -1)},
                                   {QStringLiteral("place"), place}, {QStringLiteral("slides"), slides}});
    }
    return themes;
}

QVariantMap ExtendedApi::describedTheme(const QString &place, QString *error) const
{
    rv::data::Template::Document theme;
    if (!themefile::read(root(), place, &theme, error))
        return {};
    QVariantList slides;
    for (int i = 0; i < theme.slides_size(); ++i) {
        const QString name = QString::fromStdString(theme.slides(i).name());
        QVariantMap map = proconvert::toSlideMap(theme.slides(i).base_slide(), name);
        map.insert(QStringLiteral("id"), QString::fromStdString(theme.slides(i).base_slide().uuid().string()));
        slides.append(json::themeSlide(map, name, i));
    }
    return {{QStringLiteral("id"), idOf(place)}, {QStringLiteral("name"), place.section(u'/', -1)}, {QStringLiteral("place"), place},
            {QStringLiteral("slides"), slides}};
}

QVariantMap ExtendedApi::describedPlaylist(const QString &id) const
{
    QVariantList items;
    int index = 0;
    for (const QVariant &each : m_catalog->playlistItems(id)) {
        const QVariantMap row = each.toMap();
        const QString file = row.value(QStringLiteral("file")).toString();
        items.append(json::playlistItem(row, index++, file.isEmpty() ? QString() : httpapifiles::presentationUuid(file),
                                        file.isEmpty() ? QString() : libraryOf(file)));
    }
    QString name;
    for (const QVariant &each : m_catalog->playlists()) {
        if (each.toMap().value(QStringLiteral("path")).toString() == id)
            name = each.toMap().value(QStringLiteral("name")).toString();
    }
    return {{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("items"), items}};
}

// ---- answering

ExtendedApi::Answer ExtendedApi::answer(const extendedapi::Operation &operation, bool theme, const QVariantMap &params,
                                        const QVariantMap &query, const QByteArray &body)
{
    QVariantMap map;
    if (!bodyOf(body, &map))
        return bad(QStringLiteral("The body is not a JSON object"));
    return answer(operation, theme, params, query, map);
}

ExtendedApi::Answer ExtendedApi::answer(const extendedapi::Operation &operation, bool theme, const QVariantMap &params,
                                        const QVariantMap &query, const QVariantMap &body)
{
    if (!m_catalog || root().isEmpty())
        return {503, json::error(QStringLiteral("No workspace is open"))};
    const QString unknown = unknownOf(operation, body);
    if (!unknown.isEmpty())
        return bad(unknown);
    for (const extendedapi::Field &field : operation.fields) {
        if (!field.required || field.where == extendedapi::Field::Path)
            continue;
        const QVariantMap &from = field.where == extendedapi::Field::Query ? query : body;
        if (!from.contains(QString::fromLatin1(field.name)))
            return bad(QStringLiteral("%1 needs `%2`: %3").arg(QString::fromLatin1(operation.name), QString::fromLatin1(field.name),
                                                               QString::fromUtf8(field.description)));
    }
    // (A media file named from the workspace's Media folder is that file)
    m_changing.clear();
    const Answer made = run(operation, theme, params, query,
                            operation.kind == extendedapi::Operation::Read ? body : withMedia(body, m_catalog->mediaDirectory()).toMap());
    // What each file it changed is now, for the next request to know whether it has
    // been changed by anything else since (see keep())
    for (const QString &file : std::as_const(m_changing)) {
        if (m_kept.contains(file))
            m_kept[file].left = fingerprint(file);
    }
    m_changing.clear();
    if (operation.kind != extendedapi::Operation::Read && made.status >= 200 && made.status < 300)
        emit did(QString::fromLatin1(operation.name));
    return made;
}

ExtendedApi::Answer ExtendedApi::run(const extendedapi::Operation &operation, bool theme, const QVariantMap &params,
                                     const QVariantMap &query, const QVariantMap &body)
{
    const QByteArray &name = operation.name;
    const auto part = [&params](const char *key) { return params.value(QLatin1String(key)).toString(); };

    if (name == "get_workspace")
        return workspace();
    if (name == "get_reference") {
        const QString text = reference(query.value(QStringLiteral("topic")).toString());
        return text.isEmpty() ? missing(QStringLiteral("The reference has no such part")) : Answer {200, QVariantMap {{QStringLiteral("reference"), text}}};
    }
    if (name == "get_openapi")
        return {200, extendedapi::openApi(QCoreApplication::applicationVersion())};
    if (name == "list_libraries")
        return libraries();
    if (name == "create_library")
        return createLibrary(body);
    if (name == "list_presentations")
        return listPresentations(part("library"));
    if (name == "search_presentations")
        return searchPresentations(query);
    if (name == "create_presentation")
        return createPresentation(part("library"), body);
    if (name == "list_themes")
        return {200, themeList()};
    if (name == "create_theme")
        return createTheme(body);
    if (name == "list_playlists") {
        QHash<QString, int> counts;
        for (const QVariant &each : m_catalog->playlists())
            counts.insert(each.toMap().value(QStringLiteral("path")).toString(), int(m_catalog->playlistItems(each.toMap().value(QStringLiteral("path")).toString()).size()));
        return {200, json::playlistTree(m_catalog->playlists(), counts)};
    }
    if (name == "create_playlist")
        return createPlaylist(body);
    if (name == "list_media")
        return listMedia(query);
    if (name == "list_action_targets")
        return actionTargets();

    // What is of a theme
    if (theme || operation.pattern.startsWith("theme/")) {
        const ThemeFound found = this->theme(theme ? part("theme") : part("theme"));
        if (!found)
            return missing(found.error);
        if (name == "get_theme")
            return getTheme(found.place, QString());
        if (name == "get_slide")
            return getTheme(found.place, part("slide"));
        if (name == "delete_theme")
            return deleteTheme(found.place);
        if (name == "add_theme_slide" || name == "update_theme_slide" || name == "delete_theme_slide")
            return changeThemeSlide(found.place, part("slide"), body, name);
        return changeThemeElement(found.place, part("slide"), part("element"), body, name);
    }

    // What is of a playlist
    if (operation.pattern.startsWith("playlist/")) {
        bool folder = false;
        const QString id = playlist(part("playlist"), &folder);
        if (id.isEmpty())
            return missing(QStringLiteral("The workspace has no playlist “%1”").arg(part("playlist")));
        if (name == "update_playlist")
            return updatePlaylist(id, body);
        if (name == "delete_playlist")
            return deletePlaylist(id);
        if (folder)
            return bad(QStringLiteral("“%1” is a folder of playlists, and has no rows of its own").arg(part("playlist")));
        if (name == "get_playlist")
            return {200, describedPlaylist(id)};
        if (name == "set_playlist_items" || name == "add_playlist_items")
            return setPlaylistItems(id, body, name == "set_playlist_items");
        return updatePlaylistItem(id, part("item"), body, name == "delete_playlist_item");
    }

    // What is of a presentation
    const Found found = presentationOf(params, query);
    if (!found)
        return {found.status, json::error(found.error)};
    if (name == "get_presentation") {
        const QString detail = query.value(QStringLiteral("detail")).toString();
        if (!detail.isEmpty() && detail != QLatin1String("outline") && detail != QLatin1String("full"))
            return bad(QStringLiteral("`detail` is \"outline\" or \"full\""));
        QString error;
        const QVariantMap made = described(found.path, detail == QLatin1String("full"), &error);
        return error.isEmpty() ? Answer {200, made} : failed(error);
    }
    if (name == "get_slide")
        return getSlide(found, part("slide"));
    // (Everything from here on changes the presentation.)
    if (isBusy(found.path))
        return inTheWay(QStringLiteral("“%1” is open in the editor at the computer, which would write over a change made now: close the editor first")
                            .arg(QFileInfo(found.path).completeBaseName()));
    if (name == "update_presentation")
        return updatePresentation(found, body);
    if (name == "delete_presentation")
        return deletePresentation(found);
    if (name == "duplicate_presentation")
        return duplicatePresentation(found, body);
    if (name == "apply_theme")
        return applyTheme(found, body);
    if (name == "format_text")
        return formatText(found, body);
    if (name == "update_group" || name == "delete_group")
        return changeGroup(found, part("group"), body, name == "delete_group");
    if (name == "set_arrangement" || name == "delete_arrangement")
        return changeArrangement(found, part("arrangement"), body, name == "delete_arrangement");
    if (name == "update_timeline")
        return updateTimeline(found, body);
    if (name == "add_slides")
        return addSlides(found, body);
    if (name == "update_slide")
        return updateSlide(found, part("slide"), body);
    if (name == "delete_slide")
        return deleteSlide(found, part("slide"));
    if (name == "duplicate_slide")
        return duplicateSlide(found, part("slide"), body);
    if (name == "add_element" || name == "update_element" || name == "delete_element")
        return changeElement(found, part("slide"), part("element"), body, name);
    if (name == "add_action" || name == "update_action" || name == "delete_action")
        return changeAction(found, part("slide"), part("action"), body, name);
    return {404, json::error(QStringLiteral("There is no such operation"))};
}

// ---- the workspace, its libraries

ExtendedApi::Answer ExtendedApi::workspace() const
{
    QVariantList libraries;
    for (const QVariant &each : m_catalog->libraries()) {
        const QVariantMap library = each.toMap();
        libraries.append(QVariantMap {
            {QStringLiteral("name"), library.value(QStringLiteral("name"))},
            {QStringLiteral("presentations"), QDir(library.value(QStringLiteral("path")).toString()).entryList({QStringLiteral("*.pro")}, QDir::Files).size()},
        });
    }
    QHash<QString, int> counts;
    for (const QVariant &each : m_catalog->playlists())
        counts.insert(each.toMap().value(QStringLiteral("path")).toString(), int(m_catalog->playlistItems(each.toMap().value(QStringLiteral("path")).toString()).size()));
    QVariantList groups;
    for (const QVariant &each : m_groups)
        groups.append(QVariantMap {{QStringLiteral("name"), each.toMap().value(QStringLiteral("name"))}, {QStringLiteral("color"), each.toMap().value(QStringLiteral("color"))}});
    return {200, QVariantMap {
        {QStringLiteral("name"), m_catalog->workspaceName()},
        {QStringLiteral("folder"), root()},
        {QStringLiteral("media_folder"), m_catalog->mediaDirectory()},
        {QStringLiteral("libraries"), libraries},
        {QStringLiteral("playlists"), json::playlistTree(m_catalog->playlists(), counts)},
        {QStringLiteral("themes"), themeList()},
        {QStringLiteral("groups"), groups},
        {QStringLiteral("app"), QStringLiteral("SimplePresenter %1").arg(QCoreApplication::applicationVersion())},
    }};
}

ExtendedApi::Answer ExtendedApi::libraries() const
{
    return {200, workspace().data.toMap().value(QStringLiteral("libraries"))};
}

ExtendedApi::Answer ExtendedApi::createLibrary(const QVariantMap &body)
{
    // (A name another library has is in the way, where one that is no name is wrong:
    // looked at here, the catalogue's own refusal being one message for both)
    const QString wanted = body.value(QStringLiteral("name")).toString().simplified();
    if (!wanted.isEmpty() && !wanted.contains(u'/') && !wanted.startsWith(u'.') && !m_catalog->librariesDirectory().isEmpty()
        && QDir(m_catalog->librariesDirectory()).exists(wanted))
        return inTheWay(QStringLiteral("There is a library named %1 already").arg(wanted));
    const QVariantMap made = m_catalog->createLibrary(wanted);
    if (!made.value(QStringLiteral("error")).toString().isEmpty())
        return bad(made.value(QStringLiteral("error")).toString());
    return {201, QVariantMap {{QStringLiteral("name"), QFileInfo(made.value(QStringLiteral("path")).toString()).fileName()}, {QStringLiteral("presentations"), 0}}};
}

ExtendedApi::Answer ExtendedApi::listPresentations(const QString &library) const
{
    const QString folder = libraryPath(library);
    if (folder.isEmpty())
        return missing(QStringLiteral("The workspace has no library “%1”").arg(library));
    QVariantList presentations;
    for (const QFileInfo &file : workspace::sortedEntries(folder, {QStringLiteral("*.pro")}, QDir::Files)) {
        presentations.append(QVariantMap {{QStringLiteral("uuid"), httpapifiles::presentationUuid(file.absoluteFilePath())},
                                          {QStringLiteral("name"), file.completeBaseName()}});
    }
    return {200, QVariantMap {{QStringLiteral("name"), QFileInfo(folder).fileName()}, {QStringLiteral("presentations"), presentations}}};
}

ExtendedApi::Answer ExtendedApi::searchPresentations(const QVariantMap &query) const
{
    const QString typed = query.value(QStringLiteral("q")).toString();
    bool isNumber = false;
    int limit = query.value(QStringLiteral("limit")).toInt(&isNumber);
    if (!isNumber || limit <= 0)
        limit = 20;
    limit = qMin(limit, 200);
    QVariantList found;
    if (m_search)
        QMetaObject::invokeMethod(m_search, "find", Q_RETURN_ARG(QVariantList, found), Q_ARG(QString, typed), Q_ARG(int, limit));
    QVariantList made;
    for (const QVariant &each : found) {
        const QVariantMap hit = each.toMap();
        const QString path = hit.value(QStringLiteral("path")).toString();
        made.append(QVariantMap {
            {QStringLiteral("uuid"), httpapifiles::presentationUuid(path)},
            {QStringLiteral("name"), hit.value(QStringLiteral("name"))},
            {QStringLiteral("library"), hit.value(QStringLiteral("library"))},
            {QStringLiteral("by_name"), hit.value(QStringLiteral("byName")).toBool()},
            {QStringLiteral("line"), hit.value(QStringLiteral("line"))},
        });
    }
    return {200, made};
}

// ---- presentations

QString ExtendedApi::cueExtras(const QString &copy, const QString &cueId, const QVariantMap &wanted) const
{
    if (wanted.contains(QStringLiteral("media"))) {
        const QVariant asked = wanted.value(QStringLiteral("media"));
        if (asked.isNull() || !asked.isValid()) {
            const QString error = ProDocument::removeCueMedia(copy, cueId);
            if (!error.isEmpty())
                return error;
        } else {
            const QVariantMap media = asked.typeId() == QMetaType::QString ? QVariantMap {{QStringLiteral("file"), asked}} : asked.toMap();
            if (asked.typeId() != QMetaType::QString && asked.typeId() != QMetaType::QVariantMap)
                return QStringLiteral("A slide's `media` is a file's path, or {file, foreground, playback, loop_count, loop_seconds, settings}, or null for none");
            static const QStringList known {QStringLiteral("file"), QStringLiteral("foreground"), QStringLiteral("playback"), QStringLiteral("loop_count"),
                                            QStringLiteral("loop_seconds"), QStringLiteral("settings"), QStringLiteral("name"), QStringLiteral("missing"),
                                            QStringLiteral("video")};
            for (auto it = media.constBegin(); it != media.constEnd(); ++it) {
                if (!known.contains(it.key()))
                    return QStringLiteral("A slide's `media` has no `%1`: it has file, foreground, playback, loop_count, loop_seconds and settings").arg(it.key());
            }
            const QString file = media.value(QStringLiteral("file")).toString();
            // The media the cue has now, to know what is being asked: another file, or
            // the same one played another way
            QString has;
            QString hasFile;
            ProDocument::readCueMedia(copy, cueId, [&has, &hasFile](const rv::data::Action &action) {
                has = workspace::fileNameOf(action.media().element().url());
                hasFile = workspace::findMediaFile(action.media().element().url());
            });
            // (Another file: one of another name, or of the same name in another place.
            // Where the one it has cannot be found on this computer there is only its
            // name to go by.)
            const bool another = !file.isEmpty()
                                 && (has.isEmpty() || QFileInfo(file).fileName() != has
                                     || (!hasFile.isEmpty() && QFileInfo(hasFile).canonicalFilePath() != QFileInfo(file).canonicalFilePath()));
            if (file.isEmpty() && has.isEmpty())
                return QStringLiteral("A slide's `media` needs `file`: a picture's or a video's path");
            if (another) {
                if (!workspace::isMedia(file) || !QFileInfo(file).isFile())
                    return QStringLiteral("%1 is not a picture or a video that is on this computer").arg(file);
                // (The window gives a slide its media's name as its label, which is for
                // a file dropped on it; here the label is left as it was.)
                // (A cue that shows no slide, only media, has no label: nothing to put back.)
                labelfile::Label label;
                bool labelled = false;
                rv::data::Presentation presentation;
                QString error;
                if (!proconvert::readPresentation(copy, &presentation, &error))
                    return error;
                if (rv::data::Cue *cue = presentationedit::cue(&presentation, cueId)) {
                    label = labelfile::of(*cue);
                    labelled = presentationedit::slideOf(cue) != nullptr;
                }
                error = ProDocument::setCueMedia(copy, cueId, file, media.value(QStringLiteral("foreground")).toBool(), root());
                if (error.isEmpty() && labelled)
                    error = ProDocument::setCueLabel(copy, cueId, label.text, label.color);
                if (!error.isEmpty())
                    return error;
            }
            if (media.contains(QStringLiteral("foreground"))) {
                const QString error = ProDocument::setCueMediaForeground(copy, cueId, media.value(QStringLiteral("foreground")).toBool());
                if (!error.isEmpty())
                    return error;
            }
            if (media.contains(QStringLiteral("playback"))) {
                const int playback = json::playbackOf(media.value(QStringLiteral("playback")));
                if (playback < 0)
                    return QStringLiteral("A video's `playback` is one of \"stop\", \"loop\", \"loop_count\" and \"loop_seconds\"");
                const QString error = ProDocument::setCueMediaPlayback(copy, cueId, playback, qMax(1, media.value(QStringLiteral("loop_count")).toInt()),
                                                                       qMax(0.0, media.value(QStringLiteral("loop_seconds")).toDouble()));
                if (!error.isEmpty())
                    return error;
            }
            if (media.contains(QStringLiteral("settings"))) {
                const QString error = ProDocument::setCueMediaSettings(copy, cueId, json::named(media.value(QStringLiteral("settings")).toMap()));
                if (!error.isEmpty())
                    return error;
            }
        }
    }
    if (wanted.contains(QStringLiteral("go_to_next"))) {
        const QVariant asked = wanted.value(QStringLiteral("go_to_next"));
        const QVariantMap timer = asked.toMap();
        if (!asked.isNull() && asked.isValid() && (asked.typeId() != QMetaType::QVariantMap || !timer.contains(QStringLiteral("seconds"))))
            return QStringLiteral("A slide's `go_to_next` is {seconds, loop}, or null for none");
        const bool on = asked.typeId() == QMetaType::QVariantMap;
        const QString error = ProDocument::setCueGoToNext(copy, cueId, on, qMax(0.0, timer.value(QStringLiteral("seconds")).toDouble()),
                                                          timer.value(QStringLiteral("loop")).toBool());
        if (!error.isEmpty())
            return error;
    }
    if (wanted.contains(QStringLiteral("actions"))) {
        if (wanted.value(QStringLiteral("actions")).typeId() != QMetaType::QVariantList)
            return QStringLiteral("A slide's `actions` is a list of actions");
        for (const QVariant &each : wanted.value(QStringLiteral("actions")).toList()) {
            QString error;
            const QVariantMap action = actionOf(each.toMap(), &error);
            if (error.isEmpty())
                error = ProDocument::addCueActions(copy, {cueId}, action);
            if (!error.isEmpty())
                return error;
        }
    }
    return {};
}

namespace {

// The theme slide a request names, read: `theme` by what found it, `which` its slide
// (the first for ""). An error message, empty on success.
QString themeSlideOf(const QString &root, const QString &place, const QString &which, rv::data::Template::Document *theme, int *index)
{
    QString error;
    if (!themefile::read(root, place, theme, &error))
        return error;
    *index = which.trimmed().isEmpty() ? 0 : themeSlideIndex(*theme, which);
    if (*index < 0 || *index >= theme->slides_size())
        return QStringLiteral("The theme “%1” has no slide “%2”").arg(place, which);
    return {};
}

}

ExtendedApi::Answer ExtendedApi::createPresentation(const QString &library, const QVariantMap &body)
{
    const QString folder = libraryPath(library);
    if (folder.isEmpty())
        return missing(QStringLiteral("The workspace has no library “%1”").arg(library));
    QVariantMap wanted = body;
    // A song's words: its parts as groups, and what it says of itself at its foot
    if (wanted.contains(QStringLiteral("lyrics")) && !wanted.contains(QStringLiteral("groups")) && !wanted.contains(QStringLiteral("slides"))) {
        const songselect::Song song = songselect::parse(wanted.value(QStringLiteral("lyrics")).toString());
        if (song.sections.isEmpty())
            return bad(QStringLiteral("`lyrics` has no words to make a song of: each part begins with a line that names it (\"Verse 1\", \"Chorus\"), and parts are parted by an empty line"));
        bool isNumber = false;
        int lines = wanted.value(QStringLiteral("lines_per_slide")).toInt(&isNumber);
        if (!isNumber || lines < 1)
            lines = 2;
        wanted.insert(QStringLiteral("groups"), lyricGroups(song, lines));
        if (!wanted.contains(QStringLiteral("copyright")) && (!song.author.isEmpty() || !song.publisher.isEmpty() || song.year > 0 || !song.songNumber.isEmpty())) {
            wanted.insert(QStringLiteral("copyright"), QVariantMap {
                {QStringLiteral("title"), song.title.isEmpty() ? wanted.value(QStringLiteral("name")) : QVariant(song.title)},
                {QStringLiteral("author"), song.author}, {QStringLiteral("publisher"), song.publisher}, {QStringLiteral("year"), song.year},
                {QStringLiteral("song_number"), song.songNumber.toInt()}, {QStringLiteral("display"), true},
            });
        }
    }
    // The theme first, so that nothing is made if it is not to be found
    rv::data::Template::Document theme;
    int themeSlide = -1;
    if (wanted.contains(QStringLiteral("theme"))) {
        const ThemeFound found = this->theme(wanted.value(QStringLiteral("theme")).toString());
        if (!found)
            return missing(found.error);
        const QString error = themeSlideOf(root(), found.place, wanted.value(QStringLiteral("theme_slide")).toString(), &theme, &themeSlide);
        if (!error.isEmpty())
            return missing(error);
    }

    QString unique;
    const QString path = proconvert::presentationFileFor(folder, wanted.value(QStringLiteral("name")).toString(), QStringLiteral("New Presentation"), &unique);
    rv::data::Presentation presentation = proconvert::newPresentation(unique);
    presentation.mutable_last_modified_date()->set_seconds(QDateTime::currentSecsSinceEpoch());
    presentationedit::Context context {m_groups, QSizeF(1920, 1080)};
    if (wanted.contains(QStringLiteral("width")) || wanted.contains(QStringLiteral("height"))) {
        const double width = wanted.value(QStringLiteral("width"), 1920).toDouble();
        const double height = wanted.value(QStringLiteral("height"), 1080).toDouble();
        if (width < 16 || height < 16 || width > 16384 || height > 16384)
            return bad(QStringLiteral("`width` and `height` are a slide's size, each from 16 to 16384"));
        context.size = QSizeF(width, height);
    }
    QStringList cues;
    QVariantList descriptions;
    QString error = presentationedit::build(&presentation, wanted, context, &cues, &descriptions);
    if (!error.isEmpty())
        return bad(error);

    Work work(path);
    if (!work.error().isEmpty())
        return failed(work.error());
    error = proconvert::writePresentation(work.copy(), presentation);
    for (qsizetype i = 0; error.isEmpty() && i < cues.size(); ++i)
        error = cueExtras(work.copy(), cues.at(i), descriptions.value(i).toMap());
    if (error.isEmpty() && themeSlide >= 0) {
        int dressed = 0;
        error = ProDocument::dressCues(work.copy(), cues, theme.slides(themeSlide).base_slide(), themefile::elementIds(root()), &dressed);
    }
    if (!error.isEmpty())
        return bad(error);
    if (QFileInfo::exists(path))
        return inTheWay(QStringLiteral("A presentation called “%1” was made in “%2” in the meantime").arg(unique, library));
    error = work.finish();
    if (!error.isEmpty())
        return failed(error);
    m_catalog->rescan();
    emit presentationChanged(path);
    return {201, summarised(m_catalog, path, libraryOf(path))};
}

ExtendedApi::Answer ExtendedApi::updatePresentation(const Found &found, const QVariantMap &body)
{
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    QString error;
    // A new name is looked at before anything is changed, so that a request is not half
    // carried out and then refused: one that is no name, or that another presentation
    // of the library has
    if (body.contains(QStringLiteral("name"))) {
        const QString wanted = proconvert::presentationName(body.value(QStringLiteral("name")).toString());
        if (wanted.isEmpty())
            return bad(QStringLiteral("That is not a name a presentation can have"));
        const QFileInfo was(found.path);
        if (wanted != was.completeBaseName() && QFileInfo::exists(was.dir().filePath(wanted + QStringLiteral(".pro"))))
            return inTheWay(QStringLiteral("There is already a presentation called “%1” in %2").arg(wanted, was.dir().dirName()));
    }
    const bool hasDestination = body.contains(QStringLiteral("destination"));
    const QString destination = body.value(QStringLiteral("destination")).toString();
    if (hasDestination) {
        if (destination != QLatin1String("presentation") && destination != QLatin1String("announcements"))
            return bad(QStringLiteral("`destination` is \"presentation\" or \"announcements\""));
        error = ProDocument::setDestination(work.copy(), destination == QLatin1String("announcements") ? destination : QString());
    }
    if (error.isEmpty() && body.contains(QStringLiteral("slide_show"))) {
        bool isNumber = false;
        const double seconds = body.value(QStringLiteral("slide_show")).toDouble(&isNumber);
        if (!isNumber || seconds < 0)
            return bad(QStringLiteral("`slide_show` is a number of seconds, 0 for none"));
        error = ProDocument::setSlideShow(work.copy(), seconds);
    }
    if (error.isEmpty() && body.contains(QStringLiteral("key")) && chords::keyNumber(body.value(QStringLiteral("key")).toString()) < 0)
        return bad(QStringLiteral("“%1” is not a key: one is written as \"G\", \"F#m\", \"Bb\"").arg(body.value(QStringLiteral("key")).toString()));
    if (error.isEmpty()) {
        error = work.change([&](rv::data::Presentation *presentation) -> QString {
            if (body.contains(QStringLiteral("key"))) {
                // The key its chords are written in: where it is another than it has
                // (or it has none), it is the key it is shown in as well, as when one is
                // chosen in the editor (ProDocument::setOriginalKey). The key it has
                // already, sent again, leaves the key it is shown in alone.
                using Scale = rv::data::MusicKeyScale;
                const QString key = body.value(QStringLiteral("key")).toString();
                const auto wantedKey = Scale::MusicKey(chords::keyNumber(key));
                const auto wantedScale = chords::keyIsMinor(key) ? Scale::MUSIC_SCALE_MINOR : Scale::MUSIC_SCALE_MAJOR;
                if (!presentation->has_music() || presentation->music().original().music_key() != wantedKey
                    || presentation->music().original().music_scale() != wantedScale) {
                    for (Scale *target : {presentation->mutable_music()->mutable_original(), presentation->mutable_music()->mutable_user()}) {
                        target->set_music_key(wantedKey);
                        target->set_music_scale(wantedScale);
                    }
                }
            }
            if (body.contains(QStringLiteral("arrangement"))) {
                const QString failed = presentationedit::selectArrangement(presentation, body.value(QStringLiteral("arrangement")).toString());
                if (!failed.isEmpty())
                    return failed;
            }
            if (body.contains(QStringLiteral("shown_key"))) {
                const QString key = body.value(QStringLiteral("shown_key")).toString();
                if (chords::keyNumber(key) < 0)
                    return QStringLiteral("“%1” is not a key: one is written as \"G\", \"F#m\", \"Bb\"").arg(key);
                // (A presentation that names no key cannot be shown in another: a file
                // with only this part in it would read as a song in A flat. As the editor.)
                if (!presentation->has_music())
                    return QStringLiteral("The presentation names no key its chords are written in, so it cannot be shown in another: give it `key` first");
                using Scale = rv::data::MusicKeyScale;
                Scale *user = presentation->mutable_music()->mutable_user();
                const auto wantedKey = Scale::MusicKey(chords::keyNumber(key));
                const auto wantedScale = chords::keyIsMinor(key) ? Scale::MUSIC_SCALE_MINOR : Scale::MUSIC_SCALE_MAJOR;
                if (user->music_key() != wantedKey || user->music_scale() != wantedScale) {
                    user->set_music_key(wantedKey);
                    user->set_music_scale(wantedScale);
                }
            }
            if (body.contains(QStringLiteral("copyright"))) {
                if (body.value(QStringLiteral("copyright")).typeId() != QMetaType::QVariantMap)
                    return QStringLiteral("`copyright` is an object: {title, author, artist, publisher, year, song_number, display}");
                ccli::change(presentation->mutable_ccli(), json::named(body.value(QStringLiteral("copyright")).toMap()));
            }
            return {};
        });
    }
    if (!error.isEmpty())
        return bad(error);
    QString path = found.path;
    if (work.changed()) {
        keep(found.path);
        error = work.finish();
        if (!error.isEmpty())
            return failed(error);
        // (Where it is sent is said on each of its rows of a playlist too, as
        // ProPresenter has it: see Catalog::setDestination)
        if (hasDestination)
            PlaylistFile::setRowsDestination(root(), found.path, destination == QLatin1String("announcements") ? destination : QString());
        m_catalog->rescan();
        emit presentationChanged(found.path);
    }
    if (body.contains(QStringLiteral("name"))) {
        emit renaming(true);
        const QVariantMap renamed = m_catalog->renamePresentation(found.path, body.value(QStringLiteral("name")).toString());
        path = renamed.value(QStringLiteral("path")).toString();
        if (path != found.path && !path.isEmpty())
            emit presentationRenamed(found.path, path);
        emit renaming(false);
        // (The name was looked at above: what goes wrong here is the file's doing)
        if (!renamed.value(QStringLiteral("error")).toString().isEmpty())
            return failed(renamed.value(QStringLiteral("error")).toString());
    }
    m_catalog->rescan();
    return {200, summarised(m_catalog, path, libraryOf(path))};
}

ExtendedApi::Answer ExtendedApi::deletePresentation(const Found &found)
{
    const QVariantMap gone = m_catalog->deletePresentation(found.path);
    if (!gone.value(QStringLiteral("error")).toString().isEmpty())
        return failed(gone.value(QStringLiteral("error")).toString());
    return {200, QVariantMap {{QStringLiteral("deleted"), QFileInfo(found.path).completeBaseName()}, {QStringLiteral("library"), found.library},
                              {QStringLiteral("trash"), gone.value(QStringLiteral("trash"))}}};
}

ExtendedApi::Answer ExtendedApi::duplicatePresentation(const Found &found, const QVariantMap &body)
{
    const QString library = body.value(QStringLiteral("to_library")).toString();
    const QString folder = library.isEmpty() ? QFileInfo(found.path).absolutePath() : libraryPath(library);
    if (folder.isEmpty())
        return missing(QStringLiteral("The workspace has no library “%1”").arg(library));
    rv::data::Presentation presentation;
    QString error;
    if (!proconvert::readPresentation(found.path, &presentation, &error))
        return failed(error);
    QString unique;
    const QString wanted = body.value(QStringLiteral("name")).toString().trimmed();
    const QString path = proconvert::presentationFileFor(folder, wanted.isEmpty() ? QFileInfo(found.path).completeBaseName() : wanted,
                                                         QStringLiteral("New Presentation"), &unique);
    // A presentation of its own: its id, and the name ProPresenter keeps the same as
    // the file's. Its slides keep their ids, which are theirs within a presentation.
    presentation.mutable_uuid()->set_string(workspace::newUuid());
    presentation.set_name(unique.toStdString());
    error = proconvert::writePresentation(path, presentation);
    if (!error.isEmpty())
        return failed(error);
    m_catalog->rescan();
    return {201, summarised(m_catalog, path, libraryOf(path))};
}

namespace {

// The cues a request names (`slides`: ids or numbers), or all of them where `slides`
// is left out. An error message, empty on success; `*none` is set where it is that a
// slide named is not there. (A list with nothing in it is refused, and not taken for
// all of them: what is done to the cues is done to every one where none is named.)
QString cuesOf(const QString &file, const QVariant &asked, QStringList *cues, bool *none)
{
    *none = false;
    rv::data::Presentation presentation;
    QString error;
    if (!proconvert::readPresentation(file, &presentation, &error))
        return error;
    if (!asked.isValid() || asked.isNull()) {
        *cues = presentationedit::cueOrder(presentation);
        return {};
    }
    if (asked.typeId() != QMetaType::QVariantList && asked.typeId() != QMetaType::QStringList)
        return QStringLiteral("`slides` is a list of slides, each by its id or its number");
    if (asked.toList().isEmpty())
        return QStringLiteral("`slides` names no slides: leave it out for every slide of the presentation");
    for (const QVariant &each : asked.toList()) {
        const QString id = presentationedit::cueId(presentation, each.toString());
        if (id.isEmpty()) {
            *none = true;
            return QStringLiteral("The presentation has no slide “%1”").arg(each.toString());
        }
        cues->append(id);
    }
    return {};
}

}

ExtendedApi::Answer ExtendedApi::applyTheme(const Found &found, const QVariantMap &body)
{
    const ThemeFound themed = theme(body.value(QStringLiteral("theme")).toString());
    if (!themed)
        return missing(themed.error);
    rv::data::Template::Document theme;
    int themeSlide = -1;
    QString error = themeSlideOf(root(), themed.place, body.value(QStringLiteral("theme_slide")).toString(), &theme, &themeSlide);
    if (!error.isEmpty())
        return missing(error);
    QStringList cues;
    bool none = false;
    error = cuesOf(found.path, body.value(QStringLiteral("slides")), &cues, &none);
    if (!error.isEmpty())
        return none ? missing(error) : bad(error);
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    int dressed = 0;
    error = ProDocument::dressCues(work.copy(), cues, theme.slides(themeSlide).base_slide(), themefile::elementIds(root()), &dressed);
    if (!error.isEmpty())
        return bad(error);
    if (work.changed()) {
        keep(found.path);
        error = work.finish();
        if (!error.isEmpty())
            return failed(error);
        emit presentationChanged(found.path);
    }
    return {200, QVariantMap {{QStringLiteral("dressed"), dressed}, {QStringLiteral("theme"), themed.place},
                              {QStringLiteral("theme_slide"), QString::fromStdString(theme.slides(themeSlide).name())}}};
}

ExtendedApi::Answer ExtendedApi::formatText(const Found &found, const QVariantMap &body)
{
    if (body.value(QStringLiteral("format")).typeId() != QMetaType::QVariantMap)
        return bad(QStringLiteral("`format` is an object of what is to change: {size, color, family, …}"));
    QString error;
    const QVariantMap format = manyFormat(body.value(QStringLiteral("format")).toMap(), &error);
    if (!error.isEmpty())
        return bad(error);
    QStringList cues;
    bool none = false;
    error = cuesOf(found.path, body.value(QStringLiteral("slides")), &cues, &none);
    if (!error.isEmpty())
        return none ? missing(error) : bad(error);
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    int formatted = 0;
    error = ProDocument::formatCuesText(work.copy(), cues, format, &formatted);
    if (!error.isEmpty())
        return bad(error);
    if (work.changed()) {
        keep(found.path);
        error = work.finish();
        if (!error.isEmpty())
            return failed(error);
        emit presentationChanged(found.path);
    }
    return {200, QVariantMap {{QStringLiteral("text_boxes_changed"), formatted}}};
}

ExtendedApi::Answer ExtendedApi::changeGroup(const Found &found, const QString &group, const QVariantMap &body, bool remove)
{
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    bool none = false;
    QString error = work.change([&](rv::data::Presentation *presentation) -> QString {
        const int index = presentationedit::groupIndex(*presentation, group);
        if (index < 0) {
            none = true;
            return QStringLiteral("The presentation has no group “%1”").arg(group);
        }
        if (remove) {
            presentationedit::removeGroup(presentation, index);
            return {};
        }
        const std::string id = presentation->cue_groups(index).group().uuid().string();
        const QString failed = presentationedit::changeGroup(presentation, index, body, {m_groups, QSizeF(1920, 1080)});
        if (!failed.isEmpty())
            return failed;
        if (body.contains(QStringLiteral("before")) || body.contains(QStringLiteral("after"))) {
            const bool after = !body.contains(QStringLiteral("before"));
            const QString beside = body.value(after ? QStringLiteral("after") : QStringLiteral("before")).toString();
            const int target = presentationedit::groupIndex(*presentation, beside);
            if (target < 0) {
                none = true;
                return QStringLiteral("The presentation has no group “%1”").arg(beside);
            }
            return presentationedit::moveGroup(presentation, presentationedit::groupIndex(*presentation, QString::fromStdString(id)), target, after);
        }
        return {};
    });
    if (!error.isEmpty())
        return none ? missing(error) : bad(error);
    if (work.changed()) {
        keep(found.path);
        error = work.finish();
        if (!error.isEmpty())
            return failed(error);
        m_catalog->rescan();
        emit presentationChanged(found.path);
    }
    return {200, summarised(m_catalog, found.path, found.library)};
}

ExtendedApi::Answer ExtendedApi::changeArrangement(const Found &found, const QString &arrangement, const QVariantMap &body, bool remove)
{
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    bool none = false;
    QString error = work.change([&](rv::data::Presentation *presentation) -> QString {
        if (remove) {
            const int index = presentationedit::arrangementIndex(*presentation, arrangement);
            if (index < 0) {
                none = true;
                return QStringLiteral("The presentation has no arrangement “%1”").arg(arrangement);
            }
            // (Its id as a text of its own: the one in the arrangement goes with it)
            const std::string id = presentation->arrangements(index).uuid().string();
            arrangementedit::remove(presentation, id);
            return {};
        }
        // (A group it is to have that the presentation has not is one that is not there)
        for (const QVariant &each : body.value(QStringLiteral("groups")).toList())
            none = none || presentationedit::groupIndex(*presentation, each.toString()) < 0;
        QString id;
        return presentationedit::setArrangement(presentation, arrangement, body, &id);
    });
    if (!error.isEmpty())
        return none ? missing(error) : bad(error);
    if (work.changed()) {
        keep(found.path);
        error = work.finish();
        if (!error.isEmpty())
            return failed(error);
        m_catalog->rescan();
        emit presentationChanged(found.path);
    }
    return {200, summarised(m_catalog, found.path, found.library)};
}

ExtendedApi::Answer ExtendedApi::updateTimeline(const Found &found, const QVariantMap &body)
{
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    QString error;
    // The places asked for are looked at before anything is done: each a whole number
    // that is a cue of the timeline as it is. (One that is no number would otherwise
    // read as 0, and the first cue be moved or taken off for it.)
    int count = 0;
    {
        rv::data::Presentation presentation;
        if (!proconvert::readPresentation(work.copy(), &presentation, &error))
            return failed(error);
        count = int(timeline::describe(presentation, [](const rv::data::Action &) { return QVariantMap(); })
                        .value(QStringLiteral("cues")).toList().size());
    }
    bool none = false;
    const auto placeOf = [&](const QVariant &asked, int *place) -> QString {
        bool isNumber = false;
        const double number = asked.typeId() == QMetaType::QString || asked.typeId() == QMetaType::Bool ? -1 : asked.toDouble(&isNumber);
        if (!isNumber || number < 0 || number != std::floor(number))
            return QStringLiteral("A timeline cue's `place` is its number among the timeline's cues, from 0");
        if (number >= count) {
            none = true;
            return QStringLiteral("The timeline has no cue %1: it has %2").arg(qlonglong(number)).arg(count);
        }
        *place = int(number);
        return {};
    };
    for (const char *key : {"move", "remove", "add"}) {
        if (body.contains(QLatin1String(key)) && body.value(QLatin1String(key)).typeId() != QMetaType::QVariantList)
            return bad(QStringLiteral("`%1` is a list").arg(QLatin1String(key)));
    }
    QList<QPair<int, double>> moves;
    for (const QVariant &each : body.value(QStringLiteral("move")).toList()) {
        const QVariantMap move = each.toMap();
        bool isTime = false;
        const double time = move.value(QStringLiteral("time")).toDouble(&isTime);
        if (!move.contains(QStringLiteral("place")) || !isTime || move.value(QStringLiteral("time")).typeId() == QMetaType::QString)
            return bad(QStringLiteral("Each of `move` is {place, time}"));
        int place = 0;
        error = placeOf(move.value(QStringLiteral("place")), &place);
        if (!error.isEmpty())
            return none ? missing(error) : bad(error);
        moves.append({place, qMax(0.0, time)});
    }
    QList<int> places;
    for (const QVariant &each : body.value(QStringLiteral("remove")).toList()) {
        int place = 0;
        error = placeOf(each, &place);
        if (!error.isEmpty())
            return none ? missing(error) : bad(error);
        places.append(place);
    }
    // Moved first, a cue's place being what it was while none is taken off; then
    // taken off, the last first, so that each place still means what it meant
    for (const QPair<int, double> &move : std::as_const(moves)) {
        error = ProDocument::moveTimelineCue(work.copy(), move.first, move.second);
        if (!error.isEmpty())
            return bad(error);
    }
    std::sort(places.begin(), places.end(), std::greater<int>());
    places.erase(std::unique(places.begin(), places.end()), places.end());
    for (const int place : std::as_const(places)) {
        error = ProDocument::removeTimelineCue(work.copy(), place);
        if (!error.isEmpty())
            return bad(error);
    }
    if (body.contains(QStringLiteral("duration"))) {
        error = ProDocument::setTimelineDuration(work.copy(), body.value(QStringLiteral("duration")).toDouble());
        if (!error.isEmpty())
            return bad(error);
    }
    if (body.contains(QStringLiteral("loop"))) {
        error = ProDocument::setTimelineLoop(work.copy(), body.value(QStringLiteral("loop")).toBool());
        if (!error.isEmpty())
            return bad(error);
    }
    // Whether it follows timecode, and from what timecode: either alone leaves the other
    // as the file has it
    if (body.contains(QStringLiteral("timecode")) || body.contains(QStringLiteral("timecode_start"))) {
        rv::data::Presentation presentation;
        if (!proconvert::readPresentation(work.copy(), &presentation, &error))
            return failed(error);
        const double start = body.contains(QStringLiteral("timecode_start")) ? body.value(QStringLiteral("timecode_start")).toDouble()
                                                                              : presentation.timeline().timecode_offset();
        if (start < 0 || start >= 24 * 3600)
            return bad(QStringLiteral("timecode_start is seconds from 00:00:00;00, under a day"));
        error = ProDocument::setTimelineTimecode(work.copy(), body.contains(QStringLiteral("timecode")) ? body.value(QStringLiteral("timecode")).toBool()
                                                                                                         : presentation.timeline().timecode_enable(), start);
        if (!error.isEmpty())
            return bad(error);
    }
    if (body.contains(QStringLiteral("add"))) {
        rv::data::Presentation presentation;
        if (!proconvert::readPresentation(work.copy(), &presentation, &error))
            return failed(error);
        QVariantList cues;
        for (const QVariant &each : body.value(QStringLiteral("add")).toList()) {
            const QVariantMap cue = each.toMap();
            if (!cue.contains(QStringLiteral("time")) || (cue.contains(QStringLiteral("slide")) == cue.contains(QStringLiteral("action"))))
                return bad(QStringLiteral("Each of `add` is {time, slide} or {time, action, name}"));
            QVariantMap made {{QStringLiteral("time"), qMax(0.0, cue.value(QStringLiteral("time")).toDouble())}};
            if (cue.contains(QStringLiteral("slide"))) {
                const QString id = presentationedit::cueId(presentation, cue.value(QStringLiteral("slide")).toString());
                if (id.isEmpty())
                    return bad(QStringLiteral("The presentation has no slide “%1”").arg(cue.value(QStringLiteral("slide")).toString()));
                made.insert(QStringLiteral("slideId"), id);
            } else {
                const QVariantMap action = actionOf(cue.value(QStringLiteral("action")).toMap(), &error);
                if (!error.isEmpty())
                    return bad(error);
                made.insert(QStringLiteral("wanted"), action);
                made.insert(QStringLiteral("name"), cue.value(QStringLiteral("name")).toString());
            }
            cues.append(made);
        }
        error = ProDocument::addTimelineCues(work.copy(), cues);
        if (!error.isEmpty())
            return bad(error);
    }
    if (work.changed()) {
        keep(found.path);
        error = work.finish();
        if (!error.isEmpty())
            return failed(error);
        emit presentationChanged(found.path);
    }
    return {200, summarised(m_catalog, found.path, found.library).value(QStringLiteral("timeline"))};
}

// ---- slides

namespace {

// A slide of a presentation as it now is, as the API has it; an empty map if it is not there
QVariantMap slideNow(const Catalog *catalog, const QString &path, const QString &cueId, bool full)
{
    const QVariantMap document = catalog->openArranged(path, QString());
    int index = 0;
    QString last;
    for (const QVariant &each : document.value(QStringLiteral("slides")).toList()) {
        const QVariantMap map = each.toMap();
        const QString id = map.value(QStringLiteral("id")).toString();
        if (id == cueId)
            return json::slide(map, index, full);
        if (id != last)
            ++index;
        last = id;
    }
    return {};
}

}

ExtendedApi::Answer ExtendedApi::addSlides(const Found &found, const QVariantMap &body)
{
    if (body.value(QStringLiteral("slides")).typeId() != QMetaType::QVariantList)
        return bad(QStringLiteral("`slides` is a list of slides"));
    const QVariantList slides = body.value(QStringLiteral("slides")).toList();
    if (slides.isEmpty())
        return bad(QStringLiteral("`slides` has no slides in it"));
    rv::data::Template::Document theme;
    int themeSlide = -1;
    if (body.contains(QStringLiteral("theme"))) {
        const ThemeFound themed = this->theme(body.value(QStringLiteral("theme")).toString());
        if (!themed)
            return missing(themed.error);
        const QString error = themeSlideOf(root(), themed.place, body.value(QStringLiteral("theme_slide")).toString(), &theme, &themeSlide);
        if (!error.isEmpty())
            return missing(error);
    }
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    QStringList cues;
    bool none = false;
    QString error = work.change([&](rv::data::Presentation *presentation) -> QString {
        presentationedit::Where where;
        const presentationedit::Context context {m_groups, QSizeF(1920, 1080)};
        if (body.contains(QStringLiteral("after")) || body.contains(QStringLiteral("before"))) {
            const bool after = body.contains(QStringLiteral("after"));
            const QString asked = body.value(after ? QStringLiteral("after") : QStringLiteral("before")).toString();
            const QString id = presentationedit::cueId(*presentation, asked);
            if (id.isEmpty()) {
                none = true;
                return QStringLiteral("The presentation has no slide “%1”").arg(asked);
            }
            (after ? where.after : where.before) = id;
        } else if (body.contains(QStringLiteral("group"))) {
            // The group named, or a new one of that name, last
            const QString asked = body.value(QStringLiteral("group")).toString();
            int group = presentationedit::groupIndex(*presentation, asked);
            if (group < 0)
                group = presentationedit::addGroup(presentation, asked, QString(), context);
            where.group = QString::fromStdString(presentation->cue_groups(group).group().uuid().string());
        }
        return presentationedit::addSlides(presentation, slides, where, context, &cues);
    });
    for (qsizetype i = 0; error.isEmpty() && i < cues.size(); ++i)
        error = cueExtras(work.copy(), cues.at(i), presentationedit::described(slides.at(i)));
    if (error.isEmpty() && themeSlide >= 0) {
        int dressed = 0;
        error = ProDocument::dressCues(work.copy(), cues, theme.slides(themeSlide).base_slide(), themefile::elementIds(root()), &dressed);
    }
    if (!error.isEmpty())
        return none ? missing(error) : bad(error);
    keep(found.path);
    error = work.finish();
    if (!error.isEmpty())
        return failed(error);
    m_catalog->rescan();
    emit presentationChanged(found.path);
    QVariantList made;
    for (const QString &cue : std::as_const(cues))
        made.append(slideNow(m_catalog, found.path, cue, false));
    return {201, QVariantMap {{QStringLiteral("slides"), made}}};
}

ExtendedApi::Answer ExtendedApi::getSlide(const Found &found, const QString &slide) const
{
    rv::data::Presentation presentation;
    QString error;
    if (!proconvert::readPresentation(found.path, &presentation, &error))
        return failed(error);
    const QString cue = presentationedit::cueId(presentation, slide);
    if (cue.isEmpty())
        return missing(QStringLiteral("The presentation has no slide “%1”").arg(slide));
    const QVariantMap made = slideNow(m_catalog, found.path, cue, true);
    return made.isEmpty() ? missing(QStringLiteral("That cue shows no slide")) : Answer {200, made};
}

ExtendedApi::Answer ExtendedApi::updateSlide(const Found &found, const QString &slide, const QVariantMap &body)
{
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    QString cue;
    bool none = false;
    QString error = work.change([&](rv::data::Presentation *presentation) -> QString {
        cue = presentationedit::cueId(*presentation, slide);
        if (cue.isEmpty()) {
            none = true;
            return QStringLiteral("The presentation has no slide “%1”").arg(slide);
        }
        const QString failed = presentationedit::changeSlide(presentation, cue, body, {m_groups, QSizeF(1920, 1080)});
        if (!failed.isEmpty())
            return failed;
        // Moved, if it is to be
        presentationedit::Where where;
        for (const char *key : {"after", "before"}) {
            if (!body.contains(QLatin1String(key)))
                continue;
            const QString beside = presentationedit::cueId(*presentation, body.value(QLatin1String(key)).toString());
            if (beside.isEmpty()) {
                none = true;
                return QStringLiteral("The presentation has no slide “%1”").arg(body.value(QLatin1String(key)).toString());
            }
            (qstrcmp(key, "after") == 0 ? where.after : where.before) = beside;
            return presentationedit::moveCue(presentation, cue, where);
        }
        if (body.contains(QStringLiteral("to_group"))) {
            const int group = presentationedit::groupIndex(*presentation, body.value(QStringLiteral("to_group")).toString());
            if (group < 0) {
                none = true;
                return QStringLiteral("The presentation has no group “%1”").arg(body.value(QStringLiteral("to_group")).toString());
            }
            where.group = QString::fromStdString(presentation->cue_groups(group).group().uuid().string());
            return presentationedit::moveCue(presentation, cue, where);
        }
        return {};
    });
    if (error.isEmpty())
        error = cueExtras(work.copy(), cue, body);
    if (!error.isEmpty())
        return none ? missing(error) : bad(error);
    if (work.changed()) {
        keep(found.path);
        error = work.finish();
        if (!error.isEmpty())
            return failed(error);
        m_catalog->rescan();
        emit presentationChanged(found.path);
    }
    return {200, slideNow(m_catalog, found.path, cue, false)};
}

ExtendedApi::Answer ExtendedApi::deleteSlide(const Found &found, const QString &slide)
{
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    QString cue;
    QString error = work.change([&](rv::data::Presentation *presentation) -> QString {
        cue = presentationedit::cueId(*presentation, slide);
        if (cue.isEmpty())
            return QStringLiteral("The presentation has no slide “%1”").arg(slide);
        return presentationedit::removeCue(presentation, cue);
    });
    if (!error.isEmpty())
        return missing(error);
    keep(found.path);
    error = work.finish();
    if (!error.isEmpty())
        return failed(error);
    m_catalog->rescan();
    emit presentationChanged(found.path);
    return {200, QVariantMap {{QStringLiteral("deleted"), cue}}};
}

ExtendedApi::Answer ExtendedApi::duplicateSlide(const Found &found, const QString &slide, const QVariantMap &body)
{
    rv::data::Presentation presentation;
    QString error;
    if (!proconvert::readPresentation(found.path, &presentation, &error))
        return failed(error);
    const QString cue = presentationedit::cueId(presentation, slide);
    if (cue.isEmpty())
        return missing(QStringLiteral("The presentation has no slide “%1”").arg(slide));
    const QByteArray copied = ProDocument::copyCue(found.path, cue, &error);
    if (!error.isEmpty())
        return failed(error);

    // Into the same presentation, straight after it; or into another, after the slide
    // of that one that is named, or at its end
    Found target = found;
    QString after = cue;
    if (body.contains(QStringLiteral("to"))) {
        target = this->presentation(body.value(QStringLiteral("to")).toString(), QString());
        if (!target)
            return {target.status, json::error(target.error)};
        if (target.path != found.path) {
            after.clear();
            if (isBusy(target.path))
                return inTheWay(QStringLiteral("“%1” is open in the editor at the computer: close the editor first").arg(QFileInfo(target.path).completeBaseName()));
        }
    }
    if (body.contains(QStringLiteral("after"))) {
        rv::data::Presentation other;
        if (!proconvert::readPresentation(target.path, &other, &error))
            return failed(error);
        after = presentationedit::cueId(other, body.value(QStringLiteral("after")).toString());
        if (after.isEmpty())
            return bad(QStringLiteral("“%1” has no slide “%2”").arg(QFileInfo(target.path).completeBaseName(), body.value(QStringLiteral("after")).toString()));
    }
    Work work(target.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    QString made;
    error = ProDocument::pasteCue(work.copy(), after, true, copied, &made);
    if (!error.isEmpty())
        return bad(error);
    keep(target.path);
    error = work.finish();
    if (!error.isEmpty())
        return failed(error);
    m_catalog->rescan();
    emit presentationChanged(target.path);
    return {201, slideNow(m_catalog, target.path, made, false)};
}

// ---- elements

namespace {

// One of the three things asked of a slide's elements, done to the slide as the file
// has it. `*id` is the element's, to answer with; `*status` what an error is to be said
// with.
QString elementChange(rv::data::Slide *slide, const QString &element, const QVariantMap &body, const QByteArray &operation, QString *id, int *status)
{
    *status = 400;
    if (operation == "add_element") {
        const json::ElementChanges wanted = json::elementChanges(body, {});
        if (!wanted.error.isEmpty())
            return wanted.error;
        return slideedit::add(slide, wanted, id);
    }
    const int index = slideedit::indexOf(*slide, element);
    if (index < 0) {
        *status = 404;
        return QStringLiteral("The slide has no element “%1”").arg(element);
    }
    *id = QString::fromStdString(slide->elements(index).element().uuid().string());
    if (operation == "delete_element") {
        slideedit::remove(slide, index);
        return {};
    }
    // (The element as it is on its slide, with what is worked out of the slide as a
    // whole, such as whether it shows: what it was shown with, to be set against)
    json::ElementChanges changes = json::elementChanges(body, proconvert::toSlideMap(*slide, QString()).value(QStringLiteral("elements")).toList().value(index).toMap());
    if (!changes.error.isEmpty())
        return changes.error;
    if (!changes.kind.isEmpty() || !changes.file.isEmpty())
        return QStringLiteral("What an element is cannot be changed: `kind` and `file` are for add_element (a picture is changed with `fill_media_path`)");
    // (Where it is already is no move)
    if (changes.order == index)
        changes.order = -1;
    return slideedit::change(slide, index, changes);
}

// An element of a slide as the API has it, by its id; an empty map if it is not there
QVariantMap elementNow(const rv::data::Slide &slide, const QString &id)
{
    const QVariantMap map = proconvert::toSlideMap(slide, QString());
    int order = 0;
    for (const QVariant &each : map.value(QStringLiteral("elements")).toList()) {
        if (each.toMap().value(QStringLiteral("id")).toString() == id)
            return json::element(each.toMap(), order);
        ++order;
    }
    return {};
}

}

ExtendedApi::Answer ExtendedApi::changeElement(const Found &found, const QString &slide, const QString &element, const QVariantMap &body,
                                               const QByteArray &operation)
{
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    QString id;
    int status = 400;
    QVariantMap now;
    QString error = work.change([&](rv::data::Presentation *presentation) -> QString {
        const QString cue = presentationedit::cueId(*presentation, slide);
        rv::data::Cue *target = cue.isEmpty() ? nullptr : presentationedit::cue(presentation, cue);
        rv::data::PresentationSlide *shown = target ? presentationedit::slideOf(target) : nullptr;
        if (!shown) {
            status = 404;
            return QStringLiteral("The presentation has no slide “%1”").arg(slide);
        }
        const QString failed = elementChange(shown->mutable_base_slide(), element, body, operation, &id, &status);
        if (failed.isEmpty())
            now = elementNow(shown->base_slide(), id);
        return failed;
    });
    if (!error.isEmpty())
        return {status, json::error(error)};
    if (work.changed()) {
        keep(found.path);
        error = work.finish();
        if (!error.isEmpty())
            return failed(error);
        emit presentationChanged(found.path);
    }
    if (operation == "delete_element")
        return {200, QVariantMap {{QStringLiteral("deleted"), id}}};
    return {operation == "add_element" ? 201 : 200, now};
}

ExtendedApi::Answer ExtendedApi::changeThemeElement(const QString &place, const QString &slide, const QString &element, const QVariantMap &body,
                                                    const QByteArray &operation)
{
    const QString file = themefile::folder(root()) + u'/' + place + QStringLiteral("/Theme");
    if (isBusy(file))
        return inTheWay(QStringLiteral("The theme “%1” is open in the editor at the computer: close the editor first").arg(place));
    rv::data::Template::Document theme;
    QString error;
    if (!themefile::read(root(), place, &theme, &error))
        return failed(error);
    const int index = themeSlideIndex(theme, slide);
    if (index < 0)
        return missing(QStringLiteral("The theme “%1” has no slide “%2”").arg(place, slide));
    const std::string before = theme.SerializeAsString();
    QString id;
    int status = 400;
    error = elementChange(theme.mutable_slides(index)->mutable_base_slide(), element, body, operation, &id, &status);
    if (!error.isEmpty())
        return {status, json::error(error)};
    if (theme.SerializeAsString() != before) {
        keep(file);
        error = themefile::write(root(), place, theme);
        if (!error.isEmpty())
            return failed(error);
        emit themesChanged();
    }
    if (operation == "delete_element")
        return {200, QVariantMap {{QStringLiteral("deleted"), id}}};
    return {operation == "add_element" ? 201 : 200, elementNow(theme.slides(index).base_slide(), id)};
}

// ---- a slide's actions

ExtendedApi::Answer ExtendedApi::changeAction(const Found &found, const QString &slide, const QString &action, const QVariantMap &body,
                                              const QByteArray &operation)
{
    rv::data::Presentation presentation;
    QString error;
    if (!proconvert::readPresentation(found.path, &presentation, &error))
        return failed(error);
    const QString cue = presentationedit::cueId(presentation, slide);
    if (cue.isEmpty())
        return missing(QStringLiteral("The presentation has no slide “%1”").arg(slide));
    if (operation != "add_action") {
        bool has = false;
        if (const rv::data::Cue *of = presentationedit::cue(&presentation, cue)) {
            for (const rv::data::Action &each : of->actions())
                has = has || (QString::fromStdString(each.uuid().string()) == action && actions::listed(each));
        }
        if (!has)
            return missing(QStringLiteral("That slide has no action “%1”: get_slide lists those it has, each with its id").arg(action));
    }
    Work work(found.path);
    if (!work.error().isEmpty())
        return failed(work.error());
    if (operation == "delete_action") {
        error = ProDocument::removeCueAction(work.copy(), cue, action);
    } else {
        const QVariantMap wanted = actionOf(body, &error);
        if (error.isEmpty())
            error = operation == "add_action" ? ProDocument::addCueActions(work.copy(), {cue}, wanted)
                                              : ProDocument::changeCueAction(work.copy(), cue, action, wanted);
    }
    if (!error.isEmpty())
        return bad(error);
    if (work.changed()) {
        keep(found.path);
        error = work.finish();
        if (!error.isEmpty())
            return failed(error);
        emit presentationChanged(found.path);
    }
    return {operation == "add_action" ? 201 : 200,
            QVariantMap {{QStringLiteral("slide"), cue}, {QStringLiteral("actions"), slideNow(m_catalog, found.path, cue, false).value(QStringLiteral("actions"))}}};
}

// ---- themes

ExtendedApi::Answer ExtendedApi::getTheme(const QString &place, const QString &slide) const
{
    QString error;
    const QVariantMap theme = describedTheme(place, &error);
    if (!error.isEmpty())
        return failed(error);
    if (slide.isEmpty())
        return {200, theme};
    rv::data::Template::Document document;
    themefile::read(root(), place, &document, nullptr);
    const int index = themeSlideIndex(document, slide);
    if (index < 0)
        return missing(QStringLiteral("The theme “%1” has no slide “%2”").arg(place, slide));
    return {200, theme.value(QStringLiteral("slides")).toList().value(index)};
}

namespace {

// A theme's slide made as it is described: { name, background, width, height, elements },
// or with nothing said of what is on it, the slide a new theme has here
QString describedThemeSlide(rv::data::Template::Document *theme, const QVariantMap &wanted)
{
    const QString name = wanted.value(QStringLiteral("name")).toString().trimmed();
    rv::data::Template::Slide made;
    themefile::fillSlide(&made, name.isEmpty() ? themefile::freeSlideName(*theme) : name);
    rv::data::Slide *slide = made.mutable_base_slide();
    if (wanted.contains(QStringLiteral("width")) || wanted.contains(QStringLiteral("height"))) {
        const double width = wanted.value(QStringLiteral("width"), 1920).toDouble();
        const double height = wanted.value(QStringLiteral("height"), 1080).toDouble();
        if (width < 16 || height < 16 || width > 16384 || height > 16384)
            return QStringLiteral("`width` and `height` are a slide's size, each from 16 to 16384");
        slide->mutable_size()->set_width(width);
        slide->mutable_size()->set_height(height);
    }
    if (wanted.contains(QStringLiteral("elements"))) {
        if (wanted.value(QStringLiteral("elements")).typeId() != QMetaType::QVariantList)
            return QStringLiteral("A slide's `elements` is a list of elements");
        // What is described, in place of the text box a new slide starts with
        slide->clear_elements();
        for (const QVariant &each : wanted.value(QStringLiteral("elements")).toList()) {
            const json::ElementChanges element = json::elementChanges(each.toMap(), {});
            if (!element.error.isEmpty())
                return element.error;
            QString id;
            const QString error = slideedit::add(slide, element, &id);
            if (!error.isEmpty())
                return error;
        }
    }
    if (wanted.contains(QStringLiteral("background")) && !slideedit::setBackground(slide, wanted.value(QStringLiteral("background")).toString()))
        return QStringLiteral("`background` is not a colour");
    *theme->add_slides() = made;
    return {};
}

}

ExtendedApi::Answer ExtendedApi::createTheme(const QVariantMap &body)
{
    const QString name = body.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty() || name.contains(u'/') || name.startsWith(u'.'))
        return bad(QStringLiteral("A theme needs a name, without a slash in it"));
    if (QFileInfo::exists(themefile::folder(root()) + u'/' + name))
        return inTheWay(QStringLiteral("There is a theme or a folder of themes called “%1” already").arg(name));
    rv::data::Template::Document theme;
    QVariantList slides = body.value(QStringLiteral("slides")).toList();
    if (body.contains(QStringLiteral("slides")) && body.value(QStringLiteral("slides")).typeId() != QMetaType::QVariantList)
        return bad(QStringLiteral("`slides` is a list of slides, each {name, background, elements}"));
    if (slides.isEmpty())
        slides.append(QVariantMap());
    for (const QVariant &each : std::as_const(slides)) {
        const QString error = describedThemeSlide(&theme, each.toMap());
        if (!error.isEmpty())
            return bad(error);
    }
    const QString error = themefile::write(root(), name, theme);
    if (!error.isEmpty())
        return failed(error);
    // (ProPresenter keeps a theme's pictures in a folder beside its file.)
    QDir().mkpath(themefile::folder(root()) + u'/' + name + QStringLiteral("/Assets"));
    emit themesChanged();
    QString unread;
    return {201, describedTheme(name, &unread)};
}

ExtendedApi::Answer ExtendedApi::deleteTheme(const QString &place)
{
    const QString folder = themefile::folder(root()) + u'/' + place;
    if (isBusy(folder + QStringLiteral("/Theme")))
        return inTheWay(QStringLiteral("The theme “%1” is open in the editor at the computer: close the editor first").arg(place));
    // (A place is one the listing gave, a folder's name or folders' names: never a way
    // out of the themes' folder. A name may well have dots in it, "Waiting...".)
    const QStringList parts = place.split(u'/');
    if (place.isEmpty() || parts.contains(QStringLiteral("..")) || parts.contains(QStringLiteral(".")) || parts.contains(QString())
        || !QFileInfo::exists(folder + QStringLiteral("/Theme")))
        return missing(QStringLiteral("That theme is not in the workspace any more"));
    // To the Trash, where it can be put back from, and not deleted outright: what asks
    // may be far from whoever made the theme. (The window's own removes it for good,
    // after asking the person in front of it.)
    QFile theme(folder);
    if (!theme.moveToTrash())
        return failed(QStringLiteral("The theme “%1” could not be moved to the Trash, and has been left where it is: %2").arg(place, theme.errorString()));
    SessionLog::write("trash", QStringLiteral("%1 moved to the Trash, as %2").arg(folder, theme.fileName()));
    emit themesChanged();
    return {200, QVariantMap {{QStringLiteral("deleted"), place}, {QStringLiteral("trash"), theme.fileName()}}};
}

ExtendedApi::Answer ExtendedApi::changeThemeSlide(const QString &place, const QString &slide, const QVariantMap &body, const QByteArray &operation)
{
    const QString file = themefile::folder(root()) + u'/' + place + QStringLiteral("/Theme");
    if (isBusy(file))
        return inTheWay(QStringLiteral("The theme “%1” is open in the editor at the computer: close the editor first").arg(place));
    rv::data::Template::Document theme;
    QString error;
    if (!themefile::read(root(), place, &theme, &error))
        return failed(error);
    const std::string before = theme.SerializeAsString();
    int index = -1;
    if (operation == "add_theme_slide") {
        if (body.contains(QStringLiteral("copy_of"))) {
            const int source = themeSlideIndex(theme, body.value(QStringLiteral("copy_of")).toString());
            if (source < 0)
                return missing(QStringLiteral("The theme “%1” has no slide “%2”").arg(place, body.value(QStringLiteral("copy_of")).toString()));
            // The slide over again, as itself: ids of its own throughout, so that what
            // marks a thing as a theme's (its id) marks each slide's things apart
            rv::data::Template::Slide copy = theme.slides(source);
            QByteArray bytes = QByteArray::fromStdString(copy.SerializeAsString());
            QList<std::string> ids {copy.base_slide().uuid().string()};
            for (const rv::data::Slide::Element &element : copy.base_slide().elements())
                ids.append(element.element().uuid().string());
            for (const std::string &id : ids) {
                const std::string fresh = workspace::newUuid();
                if (!id.empty() && id.size() == fresh.size())
                    bytes.replace(QByteArray::fromStdString(id), QByteArray::fromStdString(fresh));
            }
            if (!copy.ParseFromArray(bytes.constData(), int(bytes.size())))
                return failed(QStringLiteral("The copied slide could not be read back"));
            const QString name = body.value(QStringLiteral("name")).toString().trimmed();
            copy.set_name((name.isEmpty() ? themefile::freeSlideName(theme) : name).toStdString());
            if (body.contains(QStringLiteral("background"))
                && !slideedit::setBackground(copy.mutable_base_slide(), body.value(QStringLiteral("background")).toString()))
                return bad(QStringLiteral("`background` is not a colour"));
            *theme.add_slides() = copy;
        } else {
            error = describedThemeSlide(&theme, body);
            if (!error.isEmpty())
                return bad(error);
        }
        index = theme.slides_size() - 1;
    } else {
        index = themeSlideIndex(theme, slide);
        if (index < 0)
            return missing(QStringLiteral("The theme “%1” has no slide “%2”").arg(place, slide));
        if (operation == "delete_theme_slide") {
            if (theme.slides_size() <= 1)
                return bad(QStringLiteral("A theme's last slide cannot be removed: remove the theme"));
            theme.mutable_slides()->DeleteSubrange(index, 1);
        } else {
            if (body.contains(QStringLiteral("name"))) {
                const QString name = body.value(QStringLiteral("name")).toString().trimmed();
                if (name.isEmpty())
                    return bad(QStringLiteral("A theme slide needs a name"));
                if (theme.slides(index).name() != name.toStdString())
                    theme.mutable_slides(index)->set_name(name.toStdString());
            }
            if (body.contains(QStringLiteral("background"))
                && !slideedit::setBackground(theme.mutable_slides(index)->mutable_base_slide(), body.value(QStringLiteral("background")).toString()))
                return bad(QStringLiteral("`background` is not a colour"));
        }
    }
    if (theme.SerializeAsString() != before) {
        keep(file);
        error = themefile::write(root(), place, theme);
        if (!error.isEmpty())
            return failed(error);
        emit themesChanged();
    }
    if (operation == "delete_theme_slide")
        return {200, QVariantMap {{QStringLiteral("deleted"), slide}}};
    QString unread;
    return {operation == "add_theme_slide" ? 201 : 200, describedTheme(place, &unread).value(QStringLiteral("slides")).toList().value(index)};
}

// ---- playlists

QString ExtendedApi::wantedItems(const QVariantList &items, QList<PlaylistFile::Wanted> *wanted, int *status) const
{
    *status = 400;
    for (const QVariant &each : items) {
        const QVariantMap item = each.toMap();
        PlaylistFile::Wanted made;
        if (item.contains(QStringLiteral("header"))) {
            made.kind = PlaylistFile::Wanted::Header;
            made.name = item.value(QStringLiteral("header")).toString();
            if (item.contains(QStringLiteral("color"))) {
                QColor color;
                const QString text = item.value(QStringLiteral("color")).toString();
                if (!text.trimmed().isEmpty() && !json::colorOf(text, &color))
                    return QStringLiteral("A heading's `color` is not a colour");
                made.color = text.trimmed().isEmpty() ? QStringLiteral("") : color.name(QColor::HexRgb);
            }
        } else if (item.contains(QStringLiteral("presentation"))) {
            const Found found = presentation(item.value(QStringLiteral("presentation")).toString(), item.value(QStringLiteral("library")).toString());
            if (!found) {
                *status = found.status;
                return found.error;
            }
            made.kind = PlaylistFile::Wanted::Presentation;
            made.file = found.path;
            if (item.contains(QStringLiteral("arrangement"))) {
                made.ownArrangement = false;
                made.arrangementName = item.value(QStringLiteral("arrangement")).toString();
                if (!made.arrangementName.isEmpty()) {
                    made.arrangementId = ProDocument::arrangementId(found.path, made.arrangementName);
                    if (made.arrangementId.isEmpty()) {
                        *status = 404;
                        return QStringLiteral("“%1” has no arrangement “%2”").arg(QFileInfo(found.path).completeBaseName(), made.arrangementName);
                    }
                }
            }
        } else if (item.contains(QStringLiteral("id"))) {
            made.kind = PlaylistFile::Wanted::Row;
            made.id = item.value(QStringLiteral("id")).toString();
        } else {
            return QStringLiteral("Each of `items` is {presentation, library, arrangement} for a presentation, {header, color} for a heading, or {id} for a row the playlist has");
        }
        wanted->append(made);
    }
    return {};
}

ExtendedApi::Answer ExtendedApi::createPlaylist(const QVariantMap &body)
{
    const QString name = body.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
        return bad(QStringLiteral("A playlist needs a name"));
    const bool folder = body.value(QStringLiteral("folder")).toBool();
    QString parent;
    if (body.contains(QStringLiteral("parent"))) {
        bool isFolder = false;
        parent = playlist(body.value(QStringLiteral("parent")).toString(), &isFolder);
        if (parent.isEmpty() || !isFolder)
            return missing(QStringLiteral("The workspace has no folder of playlists “%1”").arg(body.value(QStringLiteral("parent")).toString()));
    }
    QList<PlaylistFile::Wanted> wanted;
    if (body.contains(QStringLiteral("items"))) {
        if (folder)
            return bad(QStringLiteral("A folder of playlists has no items of its own"));
        if (body.value(QStringLiteral("items")).typeId() != QMetaType::QVariantList)
            return bad(QStringLiteral("`items` is a list of items"));
        int status = 400;
        const QString error = wantedItems(body.value(QStringLiteral("items")).toList(), &wanted, &status);
        if (!error.isEmpty())
            return {status, json::error(error)};
        // (A playlist that is yet to be made has no rows to keep)
        for (const PlaylistFile::Wanted &each : std::as_const(wanted)) {
            if (each.kind == PlaylistFile::Wanted::Row)
                return bad(QStringLiteral("A new playlist has no row %1 to keep: each of its `items` is {presentation} or {header}").arg(each.id));
        }
    }
    keep(root() + QStringLiteral("/Playlists/Library"));
    const QVariantMap made = folder ? m_catalog->createPlaylistFolder(name, parent) : m_catalog->createPlaylist(name, parent);
    if (!made.value(QStringLiteral("error")).toString().isEmpty())
        return failed(made.value(QStringLiteral("error")).toString());
    const QString id = made.value(QStringLiteral("id")).toString();
    if (!wanted.isEmpty()) {
        const QString error = PlaylistFile::setItems(root(), id, wanted, nullptr);
        if (!error.isEmpty()) {
            // Not left there empty, for a request that is answered as not carried out
            m_catalog->removePlaylist(id);
            return failed(error);
        }
        m_catalog->rescan();
    }
    QVariantMap described = describedPlaylist(id);
    described.insert(QStringLiteral("folder"), folder);
    return {201, described};
}

ExtendedApi::Answer ExtendedApi::updatePlaylist(const QString &id, const QVariantMap &body)
{
    // All of it is looked at before any of it is done, and it is moved before it is
    // renamed (a move can still be refused: a folder into itself), so that a request is
    // not half carried out and then answered as refused
    const QString name = body.value(QStringLiteral("name")).toString().trimmed();
    if (body.contains(QStringLiteral("name")) && name.isEmpty())
        return bad(QStringLiteral("A playlist needs a name"));
    QString target;
    QString how;
    for (const char *key : {"before", "after", "into"}) {
        if (!body.contains(QLatin1String(key)))
            continue;
        bool folder = false;
        target = playlist(body.value(QLatin1String(key)).toString(), &folder);
        if (target.isEmpty())
            return missing(QStringLiteral("The workspace has no playlist “%1”").arg(body.value(QLatin1String(key)).toString()));
        if (qstrcmp(key, "into") == 0 && !folder)
            return bad(QStringLiteral("“%1” is not a folder of playlists").arg(body.value(QLatin1String(key)).toString()));
        how = qstrcmp(key, "into") == 0 ? QStringLiteral("onto") : QString::fromLatin1(key);
        break;
    }
    keep(root() + QStringLiteral("/Playlists/Library"));
    if (!target.isEmpty()) {
        const QString error = m_catalog->movePlaylistNode(id, target, how);
        if (!error.isEmpty())
            return bad(error);
    }
    if (body.contains(QStringLiteral("name"))) {
        const QString error = m_catalog->renamePlaylist(id, name);
        if (!error.isEmpty())
            return failed(error);
    }
    return {200, describedPlaylist(id)};
}

ExtendedApi::Answer ExtendedApi::deletePlaylist(const QString &id)
{
    const QString name = describedPlaylist(id).value(QStringLiteral("name")).toString();
    keep(root() + QStringLiteral("/Playlists/Library"));
    const QString error = m_catalog->removePlaylist(id);
    if (!error.isEmpty())
        return failed(error);
    return {200, QVariantMap {{QStringLiteral("deleted"), name}}};
}

ExtendedApi::Answer ExtendedApi::setPlaylistItems(const QString &id, const QVariantMap &body, bool replace)
{
    if (body.value(QStringLiteral("items")).typeId() != QMetaType::QVariantList)
        return bad(QStringLiteral("`items` is a list of items"));
    QList<PlaylistFile::Wanted> wanted;
    int status = 400;
    QString error = wantedItems(body.value(QStringLiteral("items")).toList(), &wanted, &status);
    if (!error.isEmpty())
        return {status, json::error(error)};
    keep(root() + QStringLiteral("/Playlists/Library"));
    if (replace) {
        error = PlaylistFile::setItems(root(), id, wanted, nullptr);
    } else {
        // New rows, under the row named or at the end; the rows there are stay as they are
        QString after;
        if (body.contains(QStringLiteral("after"))) {
            after = playlistItem(id, body.value(QStringLiteral("after")).toString());
            if (after.isEmpty())
                return missing(QStringLiteral("The playlist has no row “%1”").arg(body.value(QStringLiteral("after")).toString()));
        }
        error = PlaylistFile::insertItems(root(), id, wanted, after, nullptr);
    }
    if (!error.isEmpty())
        return bad(error);
    m_catalog->rescan();
    return {replace ? 200 : 201, describedPlaylist(id)};
}

ExtendedApi::Answer ExtendedApi::updatePlaylistItem(const QString &playlist, const QString &item, const QVariantMap &body, bool remove)
{
    const QString id = playlistItem(playlist, item);
    if (id.isEmpty())
        return missing(QStringLiteral("The playlist has no row “%1”").arg(item));
    QVariantMap row;
    for (const QVariant &each : m_catalog->playlistItems(playlist)) {
        if (each.toMap().value(QStringLiteral("path")).toString() == id)
            row = each.toMap();
    }
    keep(root() + QStringLiteral("/Playlists/Library"));
    if (remove) {
        const QString error = m_catalog->removePlaylistItem(id);
        if (!error.isEmpty())
            return failed(error);
        return {200, describedPlaylist(playlist)};
    }
    // All of it is looked at before any of it is done, so that a request is not half
    // carried out and then answered as refused
    const QString kind = row.value(QStringLiteral("kind")).toString();
    const QString file = row.value(QStringLiteral("file")).toString();
    const QString arrangement = body.value(QStringLiteral("arrangement")).toString();
    if (body.contains(QStringLiteral("arrangement"))) {
        if (kind != QLatin1String("presentation") || row.value(QStringLiteral("missing")).toBool())
            return bad(QStringLiteral("Only a row that is a presentation, whose file is here, has an arrangement"));
        if (!arrangement.isEmpty() && ProDocument::arrangementId(file, arrangement).isEmpty())
            return missing(QStringLiteral("“%1” has no arrangement “%2”").arg(QFileInfo(file).completeBaseName(), arrangement));
    }
    const bool heading = body.contains(QStringLiteral("name")) || body.contains(QStringLiteral("color"));
    const QString name = body.value(QStringLiteral("name")).toString();
    QString color;
    if (heading) {
        if (kind != QLatin1String("header"))
            return bad(QStringLiteral("Only a heading has words and a colour of its own: a presentation's row is called what the presentation is"));
        if (body.contains(QStringLiteral("color"))) {
            QColor read;
            const QString text = body.value(QStringLiteral("color")).toString();
            if (!text.trimmed().isEmpty() && !json::colorOf(text, &read))
                return bad(QStringLiteral("`color` is not a colour"));
            color = text.trimmed().isEmpty() ? QString() : read.name(QColor::HexRgb);
        }
    }
    QString target;
    bool after = false;
    for (const char *key : {"before", "after"}) {
        if (!body.contains(QLatin1String(key)))
            continue;
        target = playlistItem(playlist, body.value(QLatin1String(key)).toString());
        if (target.isEmpty())
            return missing(QStringLiteral("The playlist has no row “%1”").arg(body.value(QLatin1String(key)).toString()));
        after = qstrcmp(key, "after") == 0;
        break;
    }
    if (body.contains(QStringLiteral("arrangement"))) {
        const QString error = m_catalog->setPlaylistItemArrangement(id, file, arrangement);
        if (!error.isEmpty())
            return failed(error);
    }
    if (heading) {
        const QString error = PlaylistFile::setHeader(root(), id, body.contains(QStringLiteral("name")) ? &name : nullptr,
                                                      body.contains(QStringLiteral("color")) ? &color : nullptr);
        if (!error.isEmpty())
            return failed(error);
        m_catalog->rescan();
    }
    if (!target.isEmpty()) {
        const QString error = m_catalog->movePlaylistItem(id, target, after);
        if (!error.isEmpty())
            return failed(error);
    }
    return {200, describedPlaylist(playlist)};
}

// ---- media, and what a slide's actions can work

ExtendedApi::Answer ExtendedApi::listMedia(const QVariantMap &query) const
{
    const QDir media(m_catalog->mediaDirectory());
    const QString asked = query.value(QStringLiteral("folder")).toString();
    // (Only what is under Media: a folder asked for by a way out of it is not looked in)
    const QString folder = QDir::cleanPath(media.absoluteFilePath(asked));
    if (!asked.isEmpty() && !(folder + u'/').startsWith(QDir::cleanPath(media.absolutePath()) + u'/'))
        return bad(QStringLiteral("`folder` is a folder under the workspace's Media"));
    if (!QFileInfo(folder).isDir())
        return missing(QStringLiteral("Media has no folder “%1”").arg(asked));
    QVariantList folders;
    for (const QFileInfo &each : workspace::sortedEntries(folder, {}, QDir::Dirs | QDir::NoDotAndDotDot))
        folders.append(media.relativeFilePath(each.absoluteFilePath()));
    QVariantList files;
    for (const QFileInfo &each : workspace::sortedEntries(folder, {}, QDir::Files)) {
        const QString path = each.absoluteFilePath();
        const bool sound = workspace::isAudio(path);
        if (!workspace::isMedia(path) && !sound)
            continue;
        files.append(QVariantMap {{QStringLiteral("name"), each.fileName()}, {QStringLiteral("file"), path},
                                  {QStringLiteral("kind"), workspace::isMedia(path) ? (workspace::isVideo(path) ? QStringLiteral("video") : QStringLiteral("image"))
                                                                                    : QStringLiteral("audio")}});
    }
    return {200, QVariantMap {{QStringLiteral("folder"), media.relativeFilePath(folder) == QLatin1String(".") ? QString() : media.relativeFilePath(folder)},
                              {QStringLiteral("folders"), folders}, {QStringLiteral("files"), files}}};
}

namespace {

// What one of ProPresenter's routes answers, as a plain list of { id, name } each:
// its things are known by an `id` of { uuid, name, index }, wherever in the answer they
// are
void gather(const QVariant &answer, QVariantList *found)
{
    if (answer.typeId() == QMetaType::QVariantList) {
        for (const QVariant &each : answer.toList())
            gather(each, found);
        return;
    }
    if (answer.typeId() != QMetaType::QVariantMap)
        return;
    const QVariantMap map = answer.toMap();
    // (Some are their ids themselves: a stage screen, a stage layout)
    const QVariantMap id = map.contains(QStringLiteral("uuid")) && map.contains(QStringLiteral("name")) ? map : map.value(QStringLiteral("id")).toMap();
    if (id.contains(QStringLiteral("uuid"))) {
        found->append(QVariantMap {{QStringLiteral("id"), id.value(QStringLiteral("uuid"))}, {QStringLiteral("name"), id.value(QStringLiteral("name"))}});
        return;
    }
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        gather(it.value(), found);
}

// A bin's playlists with their rows, each { id, name, items: [{ id, name }] }
QVariantList binOf(const QVariantList &nodes, const std::function<QVariantList(const QString &)> &rowsOf)
{
    QVariantList made;
    for (const QVariant &each : nodes) {
        const QVariantMap node = each.toMap();
        if (node.value(QStringLiteral("folder")).toBool())
            continue;
        QVariantList items;
        for (const QVariant &row : rowsOf(node.value(QStringLiteral("path")).toString()))
            items.append(QVariantMap {{QStringLiteral("id"), row.toMap().value(QStringLiteral("id"))}, {QStringLiteral("name"), row.toMap().value(QStringLiteral("name"))}});
        made.append(QVariantMap {{QStringLiteral("id"), node.value(QStringLiteral("path"))}, {QStringLiteral("name"), node.value(QStringLiteral("name"))},
                                 {QStringLiteral("items"), items}});
    }
    return made;
}

}

ExtendedApi::Answer ExtendedApi::actionTargets() const
{
    QVariantMap made {
        {QStringLiteral("media_playlists"), binOf(m_catalog->mediaPlaylists(), [this](const QString &id) { return m_catalog->mediaIn(id); })},
        {QStringLiteral("audio_playlists"), binOf(m_catalog->audioPlaylists(), [this](const QString &id) { return m_catalog->audioIn(id); })},
    };
    // What the show has, by ProPresenter's own routes, which the operator window answers
    static const QList<std::pair<const char *, const char *>> routes {
        {"timers", "timers"}, {"macros", "macros"}, {"looks", "looks"}, {"props", "props"}, {"messages", "messages"},
        {"stage_screens", "stage/screens"}, {"stage_layouts", "stage/layouts"}, {"clear_groups", "clear/groups"},
    };
    for (const auto &[key, route] : routes) {
        QVariantList found;
        if (m_askShow)
            gather(m_askShow(QString::fromLatin1(route)), &found);
        made.insert(QString::fromLatin1(key), found);
    }
    return {200, made};
}
