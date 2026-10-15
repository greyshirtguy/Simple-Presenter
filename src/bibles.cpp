#include "bibles.h"

#include "bibleflow.h"
#include "linejoin.h"
#include "biblexml.h"
#include "prodocument.h"
#include "proconvert.h"
#include "sessionlog.h"
#include "textlayout.h"
#include "themefile.h"
#include "workspacefiles.h"

#include "presentation.pb.h"
#include "template.pb.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>
#include <QUuid>

namespace {

// The address asked, where a build with the test hook is pointed at a stand-in for
// api.bible and its FUMS (the scripted tests' own, on this computer): never in a build
// that is used.
QUrl asked(QUrl url)
{
#ifdef SIMPLEPRESENTER_TEST_HOOK
    const QUrl standIn(qEnvironmentVariable("SIMPLEPRESENTER_API_BIBLE_URL"));
    if (standIn.isValid() && !standIn.host().isEmpty() && url.host().endsWith(QLatin1String("api.bible"))) {
        url.setScheme(standIn.scheme());
        url.setHost(standIn.host());
        url.setPort(standIn.port());
    }
#endif
    return url;
}

// A presentation's file as it is, kept beside the editor's backups before slides are
// put into it. (As Themes keeps one before it dresses a presentation: themes.cpp.)
void keepCopy(const QString &path, const QString &workspace)
{
    const QFileInfo file(path);
    QString place = file.dir().dirName();
    const QString relative = QDir(workspace).relativeFilePath(file.absolutePath());
    if (!workspace.isEmpty() && !relative.startsWith(QLatin1String("..")))
        place = QFileInfo(workspace).fileName() + u'/' + relative;
    const QDir directory(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/Edit Backups/") + place);
    if (!directory.mkpath(QStringLiteral(".")))
        return;
    const QString target = directory.filePath(file.completeBaseName() + QDateTime::currentDateTime().toString(QStringLiteral(" yyyy-MM-dd HH.mm.ss"))
                                              + QStringLiteral(" before Bible.") + file.suffix());
    if (!QFile::exists(target))
        QFile::copy(path, target);
}

// A file name made of a Bible's name: letters, digits and a few marks
QString fileNameFor(const QString &name)
{
    QString kept;
    for (const QChar c : name) {
        if (c.isLetterOrNumber() || c == u' ' || c == u'-' || c == u'_' || c == u'(' || c == u')')
            kept += c;
    }
    kept = kept.simplified();
    return kept.isEmpty() ? QStringLiteral("Bible") : kept.left(80);
}

// The slide Bible slides are made of where no theme is chosen: black, with the words in
// white over most of it, named as ProPresenter's Bible themes name theirs
rv::data::Slide plainSlide()
{
    rv::data::Slide slide;
    slide.mutable_size()->set_width(1920);
    slide.mutable_size()->set_height(1080);
    slide.mutable_uuid()->set_string(workspace::newUuid());
    rv::data::Slide::Element element = proconvert::makeTextElement(slide, nullptr);
    rv::data::Graphics::Text *text = element.mutable_element()->mutable_text();
    TextRun format = proconvert::readText(*text).firstRun();
    format.applyFormat({{"size", 72.0}, {"color", QColor(Qt::white)}});
    proconvert::writeText(text, RichText::plain(QString(), format, Qt::AlignHCenter));
    const QString id = QString::fromStdString(element.element().uuid().string());
    *slide.add_elements() = element;
    proconvert::applyChanges(&slide, id, {{"name", QStringLiteral("Verse")}, {"x", 110.0}, {"y", 110.0}, {"width", 1700.0}, {"height", 860.0}});
    return slide;
}

// What ← Verse or Verse → says at the end or the start of a book
QString bookEdge(int delta, int book, const QString &bookName)
{
    const QString name = bookName.isEmpty() ? bible::englishName(book) : bookName;
    return delta > 0 ? QStringLiteral("That is the end of %1.").arg(name) : QStringLiteral("That is the start of %1.").arg(name);
}

// Where a text box of a slide is, by its id (textBoxes()); or by its name, the first of
// that name, which is how a choice was kept before boxes had ids here (0.95); -1
int textBoxAt(const rv::data::Slide &slide, const QString &key)
{
    if (key.isEmpty())
        return -1;
    for (int i = 0; i < slide.elements_size(); ++i) {
        const auto &element = slide.elements(i).element();
        if (element.has_text() && QString::fromStdString(element.uuid().string()) == key)
            return i;
    }
    for (int i = 0; i < slide.elements_size(); ++i) {
        const auto &element = slide.elements(i).element();
        if (element.has_text() && QString::fromStdString(element.name()) == key)
            return i;
    }
    return -1;
}

}

Bibles::Bibles(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_session(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    readIncluded();
    readIndex();
}

Bibles::~Bibles() = default;

bool Bibles::builtInKey() const
{
#ifdef SIMPLEPRESENTER_API_BIBLE_KEY
    return !QStringLiteral(SIMPLEPRESENTER_API_BIBLE_KEY).isEmpty();
#else
    return false;
#endif
}

QString Bibles::key() const
{
    if (!m_ownKey.trimmed().isEmpty())
        return m_ownKey.trimmed();
#ifdef SIMPLEPRESENTER_API_BIBLE_KEY
    return QStringLiteral(SIMPLEPRESENTER_API_BIBLE_KEY);
#else
    return {};
#endif
}

void Bibles::setOwnKey(const QString &key)
{
    if (key == m_ownKey)
        return;
    m_ownKey = key;
    emit keyChanged();
    // (The Bibles a key gives are another key's to give: asked again when next wanted.)
    m_api.clear();
    m_apiAsked = false;
    m_apiProblem.clear();
    emit translationsChanged();
}

void Bibles::setWorkspace(const QString &workspace)
{
    if (workspace == m_workspace)
        return;
    m_workspace = workspace;
    emit workspaceChanged();
}

QString Bibles::folder() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/Bibles");
}

QVariantList Bibles::translations() const
{
    QVariantList all;
    for (const Imported &each : m_included) {
        all.append(QVariantMap {{"id", QStringLiteral("included:") + each.file}, {"name", each.name}, {"abbreviation", each.abbreviation},
                                {"language", QStringLiteral("English")}, {"source", QStringLiteral("included")}, {"description", QString()}});
    }
    for (const Imported &each : m_imported) {
        all.append(QVariantMap {{"id", QStringLiteral("file:") + each.file}, {"name", each.name}, {"abbreviation", each.abbreviation},
                                {"language", QString()}, {"source", QStringLiteral("file")}, {"description", QString()}});
    }
    for (const apibible::Translation &each : m_api) {
        all.append(QVariantMap {{"id", QStringLiteral("api:") + each.id}, {"name", each.name}, {"abbreviation", each.abbreviation},
                                {"language", each.language}, {"source", QStringLiteral("api")}, {"description", each.description}});
    }
    return all;
}

// ---- Bibles that come with the app

QString Bibles::pathOf(const QString &id) const
{
    if (id.startsWith(QLatin1String("file:")))
        return folder() + u'/' + id.mid(5);
    if (id.startsWith(QLatin1String("included:")) && !m_includedFolder.isEmpty())
        return m_includedFolder + u'/' + id.mid(9);
    return {};
}

// Their folder is beside the program when it runs from where it was built (build/bibles),
// and in the data folder it was installed with when it runs from a package
// (/usr/bin/../share/simplepresenter/bibles). Only their list is read here, not the
// Bibles themselves, which are read when first looked in (withBible).
void Bibles::readIncluded()
{
    const QString program = QCoreApplication::applicationDirPath();
    for (const QString &candidate : {program + QStringLiteral("/bibles"), program + QStringLiteral("/../share/simplepresenter/bibles")}) {
        QFile list(candidate + QStringLiteral("/included.json"));
        if (!list.open(QIODevice::ReadOnly))
            continue;
        m_includedFolder = QDir(candidate).canonicalPath();
        for (const QJsonValue &value : QJsonDocument::fromJson(list.readAll()).array()) {
            const QJsonObject each = value.toObject();
            const QString file = each.value("file").toString();
            if (!file.isEmpty() && !file.contains(u'/') && QFileInfo::exists(m_includedFolder + u'/' + file))
                m_included.append({file, each.value("name").toString(), each.value("abbreviation").toString()});
        }
        return;
    }
}

// ---- Imported Bibles

void Bibles::readIndex()
{
    m_imported.clear();
    QFile file(folder() + QStringLiteral("/index.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    for (const QJsonValue &value : QJsonDocument::fromJson(file.readAll()).array()) {
        const QJsonObject entry = value.toObject();
        Imported each {entry.value("file").toString(), entry.value("name").toString(), entry.value("abbreviation").toString()};
        // (A file someone has taken away is no Bible any more.)
        if (!each.file.isEmpty() && QFileInfo::exists(folder() + u'/' + each.file))
            m_imported.append(each);
    }
}

void Bibles::writeIndex() const
{
    QJsonArray all;
    for (const Imported &each : m_imported)
        all.append(QJsonObject {{"file", each.file}, {"name", each.name}, {"abbreviation", each.abbreviation}});
    QDir().mkpath(folder());
    QFile file(folder() + QStringLiteral("/index.json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(all).toJson());
}

void Bibles::importFile(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const QPointer<Bibles> self(this);
    // Read off the thread the windows run on: a whole Bible is several megabytes of XML.
    QThreadPool::globalInstance()->start([self, path] {
        QFile file(path);
        QByteArray data;
        QString error;
        auto read = std::make_shared<bible::Bible>();
        if (!file.open(QIODevice::ReadOnly))
            error = QStringLiteral("%1 could not be opened.").arg(QFileInfo(path).fileName());
        else
            data = file.readAll();
        if (error.isEmpty())
            biblexml::read(data, QFileInfo(path).completeBaseName(), read.get(), &error);
        QMetaObject::invokeMethod(self, [self, path, data, read, error] {
            if (!self)
                return;
            QString problem = error;
            for (const Imported &each : std::as_const(self->m_imported)) {
                if (problem.isEmpty() && each.name == read->name)
                    problem = QStringLiteral("There is a Bible called %1 already.").arg(read->name);
            }
            QString name;
            if (problem.isEmpty()) {
                QDir().mkpath(self->folder());
                name = fileNameFor(read->name) + QStringLiteral(".xml");
                for (int n = 2; QFileInfo::exists(self->folder() + u'/' + name); ++n)
                    name = fileNameFor(read->name) + QStringLiteral(" %1.xml").arg(n);
                QFile copy(self->folder() + u'/' + name);
                if (!copy.open(QIODevice::WriteOnly) || copy.write(data) != data.size())
                    problem = QStringLiteral("The Bible could not be kept: %1").arg(copy.errorString());
            }
            if (!problem.isEmpty()) {
                SessionLog::write("bibles", QStringLiteral("%1 was not imported: %2").arg(QFileInfo(path).fileName(), problem));
                emit self->imported(problem, QString());
                return;
            }
            self->m_imported.append({name, read->name, read->abbreviation});
            self->m_read.insert(name, read);
            self->writeIndex();
            SessionLog::write("bibles", QStringLiteral("%1 imported, as %2").arg(QFileInfo(path).fileName(), name));
            emit self->translationsChanged();
            emit self->imported(QString(), QStringLiteral("file:") + name);
        }, Qt::QueuedConnection);
    });
}

QString Bibles::remove(const QString &id)
{
    if (!id.startsWith(QLatin1String("file:")))
        return QStringLiteral("Only an imported Bible can be taken out.");
    const QString name = id.mid(5);
    for (int i = 0; i < m_imported.size(); ++i) {
        if (m_imported[i].file != name)
            continue;
        // (Only a file of the Bibles folder, by the name the list has for it.)
        const QString path = folder() + u'/' + name;
        if (QFileInfo(path).absolutePath() != QFileInfo(folder()).absoluteFilePath() || !QFile::remove(path))
            return QStringLiteral("%1 could not be taken out.").arg(m_imported[i].name);
        SessionLog::write("bibles", QStringLiteral("%1 taken out").arg(name));
        m_imported.removeAt(i);
        m_read.remove(id);
        writeIndex();
        emit translationsChanged();
        return {};
    }
    return QStringLiteral("There is no such Bible.");
}

void Bibles::withBible(const QString &file, std::function<void(const bible::Bible *, const QString &)> ready)
{
    // (`file` is the Bible's id, "file:…" or "included:…", which m_read keeps it by.)
    // (Answered after this has returned, though the Bible is read already: whoever asked
    // is to know the number of what they asked before its answer comes.)
    if (const auto found = m_read.constFind(file); found != m_read.constEnd()) {
        const std::shared_ptr<bible::Bible> read = *found;
        QMetaObject::invokeMethod(this, [read, ready] { ready(read.get(), QString()); }, Qt::QueuedConnection);
        return;
    }
    const bool started = m_reading.contains(file);
    m_reading[file].append(ready);
    if (started)
        return;
    const QString path = pathOf(file);
    const QPointer<Bibles> self(this);
    QThreadPool::globalInstance()->start([self, path, file] {
        auto read = std::make_shared<bible::Bible>();
        QString error;
        QFile source(path);
        if (!source.open(QIODevice::ReadOnly))
            error = QStringLiteral("The Bible's file could not be read.");
        else
            biblexml::read(source.readAll(), QFileInfo(path).completeBaseName(), read.get(), &error);
        QMetaObject::invokeMethod(self, [self, file, read, error] {
            if (!self)
                return;
            if (error.isEmpty())
                self->m_read.insert(file, read);
            const auto waiting = self->m_reading.take(file);
            for (const auto &each : waiting)
                each(error.isEmpty() ? read.get() : nullptr, error);
        }, Qt::QueuedConnection);
    });
}

// ---- api.bible

void Bibles::refreshApi()
{
    if (!hasKey()) {
        m_api.clear();
        m_apiProblem.clear();
        emit translationsChanged();
        return;
    }
    if (m_apiLoading)
        return;
    m_apiAsked = true;
    m_apiLoading = true;
    emit translationsChanged();
    QUrl url = apibible::baseUrl();
    url.setPath(QStringLiteral("/v1/bibles"));
    QNetworkRequest request(asked(url));
    request.setRawHeader("api-key", key().toUtf8());
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_apiLoading = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        QString error;
        if (status == 200)
            m_api = apibible::readBibles(body, &error);
        else
            error = apibible::errorOf(status, body);
        m_apiProblem = error;
        SessionLog::write("bibles", error.isEmpty() ? QStringLiteral("api.bible gives %1 with this key").arg(m_api.size() == 1 ? QStringLiteral("1 Bible") : QStringLiteral("%1 Bibles").arg(m_api.size()))
                                                    : QStringLiteral("api.bible's list of Bibles: %1").arg(error));
        emit translationsChanged();
    });
}

void Bibles::fetch(const std::shared_ptr<Pending> &pending)
{
    if (pending->ids.isEmpty()) {
        QList<bible::PassageVerse> verses = apibible::versesOf(pending->passages, pending->reference);
        if (verses.isEmpty() && pending->instead.valid()) {
            // Past the end of a chapter: the next one's first verse, asked for instead
            auto again = std::make_shared<Pending>();
            again->request = pending->request;
            again->id = pending->id;
            again->reference = pending->instead;
            again->grow = pending->grow;
            again->was = pending->was;
            again->bookName = pending->bookName;
            again->ids = apibible::passageIds(again->reference);
            fetch(again);
            return;
        }
        if (pending->lastOnly && !verses.isEmpty())
            verses = {verses.last()};
        QString error = pending->error;
        if (!verses.isEmpty())
            error.clear();
        else if (pending->grow != 0 && error.isEmpty())
            error = bookEdge(pending->grow, pending->reference.book, pending->bookName);
        else if (error.isEmpty())
            error = QStringLiteral("api.bible gave no verses for this passage.");
        QStringList tokens;
        for (const apibible::Passage &passage : std::as_const(pending->passages)) {
            if (!passage.fumsToken.isEmpty())
                tokens << passage.fumsToken;
        }
        bible::Reference reference = pending->reference;
        if (pending->lastOnly && !verses.isEmpty())
            reference = bible::referenceTo(verses);
        // ← Verse and Verse →: the verse added to the passage it grew from
        if (pending->grow != 0 && !verses.isEmpty()) {
            verses = pending->grow > 0 ? pending->was + verses : verses + pending->was;
            reference = bible::referenceTo(verses);
        }
        answer(pending->request, pending->id, reference, verses, pending->passages.isEmpty() ? QString() : pending->passages.first().copyright, error);
        if (!verses.isEmpty())
            fums(tokens);
        return;
    }
    const QString passageId = pending->ids.takeFirst();
    QNetworkRequest request(asked(apibible::passageUrl(pending->id.mid(4), passageId)));
    request.setRawHeader("api-key", key().toUtf8());
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, pending] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (status == 200) {
            apibible::Passage passage;
            QString error;
            if (apibible::readPassage(body, &passage, &error))
                pending->passages.append(passage);
            else if (pending->error.isEmpty())
                pending->error = error;
        } else if (pending->error.isEmpty()) {
            pending->error = apibible::errorOf(status, body);
        }
        fetch(pending);
    });
}

void Bibles::fums(const QStringList &tokens)
{
    if (tokens.isEmpty())
        return;
    // The device's id: made once, at random, and kept with the Bibles. It says nothing
    // of who uses the app.
    if (m_device.isEmpty()) {
        QFile file(folder() + QStringLiteral("/device"));
        if (file.open(QIODevice::ReadOnly))
            m_device = QString::fromUtf8(file.readAll()).trimmed();
        if (m_device.isEmpty()) {
            m_device = QUuid::createUuid().toString(QUuid::WithoutBraces);
            QDir().mkpath(folder());
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(m_device.toUtf8());
        }
    }
    QNetworkRequest request(asked(apibible::fumsUrl(tokens, m_device, m_session)));
    request.setTransferTimeout(20000);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

// ---- Looking a passage up

QVariantList Bibles::books(const QString &id)
{
    QVariantList all;
    if (!pathOf(id).isEmpty()) {
        const QString file = id;
        if (const auto found = m_read.constFind(file); found != m_read.constEnd()) {
            for (const bible::Book &book : (*found)->books)
                all.append(QVariantMap {{"number", book.number}, {"name", book.name}, {"chapters", int(book.chapters.size())}});
            return all;
        }
        // (Read now, for next time: until it is, the 66 in English.)
        withBible(file, [](const bible::Bible *, const QString &) {});
    }
    for (int book = 1; book <= bible::bookCount; ++book)
        all.append(QVariantMap {{"number", book}, {"name", bible::englishName(book)}, {"chapters", bible::chapterCount(book)}});
    return all;
}

void Bibles::answer(int request, const QString &id, const bible::Reference &reference, QList<bible::PassageVerse> verses,
                    const QString &copyright, const QString &error)
{
    Found kept;
    kept.id = id;
    kept.reference = reference;
    kept.verses = verses;
    kept.copyright = copyright;
    QString name;
    for (const QVariant &each : translations()) {
        const QVariantMap translation = each.toMap();
        if (translation.value("id").toString() == id) {
            kept.abbreviation = translation.value("abbreviation").toString();
            name = translation.value("name").toString();
        }
    }
    if (!pathOf(id).isEmpty()) {
        if (const auto read = m_read.constFind(id); read != m_read.constEnd()) {
            if (const bible::Book *book = (*read)->book(reference.book))
                kept.bookName = book->name;
            if (kept.copyright.isEmpty())
                kept.copyright = (*read)->copyright;
        }
    }
    QVariantList shown;
    for (const bible::PassageVerse &v : std::as_const(verses))
        shown.append(QVariantMap {{"chapter", v.chapter}, {"number", v.verse.number}, {"text", v.verse.text}});
    // Kept for making slides and stepping, the last few only
    m_found.insert(request, kept);
    while (m_found.size() > 12)
        m_found.remove(*std::min_element(m_found.keyBegin(), m_found.keyEnd()));
    const QString written = verses.isEmpty() ? QString() : bible::written(reference, kept.bookName);
    SessionLog::write("bibles", error.isEmpty() ? QStringLiteral("%1 in %2: %3").arg(written, name, verses.size() == 1 ? QStringLiteral("1 verse") : QStringLiteral("%1 verses").arg(verses.size()))
                                                : QStringLiteral("a passage in %1: %2").arg(name, error));
    emit found(request, QVariantMap {{"request", request}, {"id", id}, {"translation", kept.abbreviation}, {"translationName", name},
                                     {"reference", written}, {"verses", shown}, {"copyright", kept.copyright}, {"error", error}});
}

int Bibles::lookup(const QString &id, const QString &typed)
{
    const int request = ++m_requests;
    if (!pathOf(id).isEmpty()) {
        const QPointer<Bibles> self(this);
        withBible(id, [self, request, id, typed](const bible::Bible *read, const QString &error) {
            if (!self)
                return;
            if (!read) {
                self->answer(request, id, {}, {}, {}, error);
                return;
            }
            const bible::Reference reference = bible::parse(typed, read->books);
            QString problem;
            const QList<bible::PassageVerse> verses = bible::passage(*read, reference, &problem);
            self->answer(request, id, reference, verses, read->copyright, problem);
        });
        return request;
    }
    if (!id.startsWith(QLatin1String("api:")) || !hasKey()) {
        QMetaObject::invokeMethod(this, [this, request, id] { answer(request, id, {}, {}, {}, QStringLiteral("There is no Bible chosen.")); },
                                  Qt::QueuedConnection);
        return request;
    }
    auto pending = std::make_shared<Pending>();
    pending->request = request;
    pending->id = id;
    pending->reference = bible::parse(typed);
    if (!pending->reference.valid()) {
        QMetaObject::invokeMethod(this, [this, request, id] { answer(request, id, {}, {}, {}, QStringLiteral("That is not a passage that can be read.")); },
                                  Qt::QueuedConnection);
        return request;
    }
    pending->ids = apibible::passageIds(pending->reference);
    fetch(pending);
    return request;
}

// The verse added to a passage by Verse → (delta 1) or ← Verse (-1), within the
// passage's book: at its first or last verse there is nothing more to add, and saying so
// leaves the passage as it is (BiblePanel keeps it).
int Bibles::step(int from, int delta)
{
    const int request = ++m_requests;
    const Found was = m_found.value(from);
    if (was.verses.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, request, was] { answer(request, was.id, {}, {}, {}, QStringLiteral("There is no passage to add to.")); },
                                  Qt::QueuedConnection);
        return request;
    }
    const int book = was.verses.first().book;
    if (!pathOf(was.id).isEmpty()) {
        const QPointer<Bibles> self(this);
        withBible(was.id, [self, request, was, delta, book](const bible::Bible *read, const QString &error) {
            if (!self)
                return;
            if (!read) {
                self->answer(request, was.id, {}, {}, {}, error);
                return;
            }
            const bible::Reference reference = delta > 0 ? bible::nextVerse(*read, was.verses) : bible::previousVerse(*read, was.verses);
            QString problem = bookEdge(delta, book, was.bookName);
            QList<bible::PassageVerse> added;
            if (reference.valid() && reference.book == book)
                added = bible::passage(*read, reference, &problem);
            if (added.isEmpty()) {
                self->answer(request, was.id, {}, {}, {}, problem);
                return;
            }
            const QList<bible::PassageVerse> all = delta > 0 ? was.verses + added : added + was.verses;
            self->answer(request, was.id, bible::referenceTo(all), all, read->copyright, QString());
        });
        return request;
    }
    // api.bible says nothing of where a chapter ends but by having no verse past it: the
    // next verse is asked for, and the next chapter's first if there is none; the verse
    // before the first of a chapter is the last of the chapter before, asked for whole.
    auto pending = std::make_shared<Pending>();
    pending->request = request;
    pending->id = was.id;
    pending->grow = delta;
    pending->was = was.verses;
    pending->bookName = was.bookName;
    const bible::PassageVerse edge = delta > 0 ? was.verses.last() : was.verses.first();
    const auto one = [book](int chapter, int verse) {
        bible::Reference r;
        r.book = book;
        r.ranges.append({chapter, verse, chapter, verse});
        return r;
    };
    if (delta > 0) {
        pending->reference = one(edge.chapter, edge.verse.number + 1);
        if (edge.chapter < bible::chapterCount(book))
            pending->instead = one(edge.chapter + 1, 1);
    } else if (edge.verse.number > 1) {
        pending->reference = one(edge.chapter, edge.verse.number - 1);
    } else if (edge.chapter > 1) {
        pending->reference.book = book;
        pending->reference.ranges.append({edge.chapter - 1, 0, edge.chapter - 1, 0});
        pending->lastOnly = true;
    } else {
        QMetaObject::invokeMethod(this, [this, request, was, delta, book] { answer(request, was.id, {}, {}, {}, bookEdge(delta, book, was.bookName)); },
                                  Qt::QueuedConnection);
        return request;
    }
    pending->ids = apibible::passageIds(pending->reference);
    fetch(pending);
    return request;
}

// ---- Slides

// By id, which a box keeps whatever it is called: two boxes of one name, or a box with
// none (ProPresenter allows both), are each a box of their own here, where by name the
// second could not be chosen and the nameless one not even shown. The theme's file is
// read each time, so that a box added to the theme in the editor is there at once.
QVariantList Bibles::textBoxes(const QString &place, const QString &slideId) const
{
    QVariantList boxes;
    rv::data::Template::Document theme;
    // (The plain slide is made afresh each time, so its box's id is a stand-in: make()
    // puts the words in its only box whatever it is given.)
    if (place.isEmpty())
        return {QVariantMap {{"id", QStringLiteral("plain")}, {"name", QStringLiteral("Verse")}}};
    if (!themefile::read(m_workspace, place, &theme, nullptr))
        return boxes;
    const rv::data::Template::Slide *slide = themefile::slideOf(theme, slideId);
    if (!slide)
        return boxes;
    QHash<QString, int> seen;
    int number = 0;
    for (const rv::data::Slide::Element &element : slide->base_slide().elements()) {
        if (!element.element().has_text())
            continue;
        ++number;
        QString name = QString::fromStdString(element.element().name()).trimmed();
        if (name.isEmpty())
            name = QStringLiteral("Text Box %1").arg(number);
        const int times = ++seen[name];
        if (times > 1)
            name += QStringLiteral(" (%1)").arg(times);
        boxes.append(QVariantMap {{"id", QString::fromStdString(element.element().uuid().string())}, {"name", name}});
    }
    return boxes;
}

QVariantMap Bibles::make(int request, const QVariantMap &options)
{
    const auto failed = [](const QString &error) { return QVariantMap {{"path", QString()}, {"count", 0}, {"error", error}}; };
    const Found found = m_found.value(request);
    if (found.verses.isEmpty())
        return failed(QStringLiteral("There is no passage to make slides of."));

    // The slide each slide is a copy of: the theme slide chosen, or a plain one
    rv::data::Slide base;
    const QString place = options.value("theme").toString();
    if (!place.isEmpty()) {
        rv::data::Template::Document theme;
        QString error;
        if (!themefile::read(m_workspace, place, &theme, &error))
            return failed(error);
        const rv::data::Template::Slide *slide = themefile::slideOf(theme, options.value("themeSlide").toString());
        if (!slide)
            return failed(QStringLiteral("The theme \"%1\" no longer has that slide.").arg(place));
        base = slide->base_slide();
    } else {
        base = plainSlide();
    }
    int verseAt = place.isEmpty() ? -1 : textBoxAt(base, options.value("verseBox").toString());
    if (verseAt < 0) {
        // (The box asked for is not there: the first that takes words.)
        for (int i = 0; i < base.elements_size() && verseAt < 0; ++i) {
            if (base.elements(i).element().has_text())
                verseAt = i;
        }
    }
    if (verseAt < 0)
        return failed(QStringLiteral("The theme slide has no text box for the words."));
    int referenceAt = place.isEmpty() ? -1 : textBoxAt(base, options.value("referenceBox").toString());
    if (referenceAt == verseAt)
        referenceAt = -1;
    // A box named for the copyright, where a theme has one, is given it
    int copyrightAt = -1;
    for (int i = 0; i < base.elements_size(); ++i) {
        const auto &element = base.elements(i).element();
        if (i != verseAt && i != referenceAt && element.has_text()
            && QString::fromStdString(element.name()).compare(QLatin1String("Copyright"), Qt::CaseInsensitive) == 0)
            copyrightAt = i;
    }

    // The words are laid out by the flow, measured as they will be drawn, at the theme's
    // size. So the copies of the box they go in do not scale their font, up or down: a
    // slide with fewer words would otherwise be in larger letters than the next, and
    // what the flow measured would not be what is drawn (the maintainer, 2026-10-15:
    // "ignore that in any theme verse element"). A transform that makes lines one (Single
    // Line, Replace Line Returns) is kept, as the maintainer asked: a theme can be made to
    // run verses of poetry on as one line, for a lower third say. The words are then
    // measured joined, as they are drawn, and a reference With Verse runs on after them
    // instead of starting a line. The two that put each word or each letter on a line of
    // its own would undo the flow altogether, and are not kept. (What ProPresenter makes
    // of such boxes in its own Bible slides is not known here.)
    using Text = rv::data::Graphics::Text;
    bool joins = false;  // whether the box makes its lines one
    QString joinWith;    // and with what
    {
        Text *text = base.mutable_elements(verseAt)->mutable_element()->mutable_text();
        text->set_scale_behavior(Text::SCALE_BEHAVIOR_NONE);
        if (text->transform() == Text::TRANSFORM_SINGLE_LINE) {
            joins = true;
            joinWith = QStringLiteral(" ");
        } else if (text->transform() == Text::TRANSFORM_REPLACE_LINE_RETURNS) {
            joins = true;
            joinWith = QString::fromStdString(text->transformdelimiter());
        } else {
            text->set_transform(Text::TRANSFORM_NONE);
        }
    }

    // How the words are written: in the box's own format, a verse's number a little
    // over the line, red letters in red where that is asked for
    const rv::data::Graphics::Element &box = base.elements(verseAt).element();
    const RichText model = proconvert::readText(box.text());
    const TextRun format = model.firstRun();
    const Qt::Alignment alignment = model.paragraphs.isEmpty() ? Qt::AlignHCenter : model.paragraphs.first().alignment;
    const QSizeF room(box.bounds().size().width() - box.text().margins().left() - box.text().margins().right(),
                      box.bounds().size().height() - box.text().margins().top() - box.text().margins().bottom());
    const bool numbers = options.value("verseNumbers").toBool();
    const bool color = options.value("color").toBool();
    const QString references = options.value("references", QStringLiteral("last")).toString();
    const QString suffix = options.value("translation").toBool() && !found.abbreviation.isEmpty() ? u' ' + found.abbreviation : QString();
    const QList<bibleflow::Word> words = bibleflow::wordsOf(found.verses);
    const QString passageReference = bible::written(found.reference, found.bookName) + suffix;
    const auto versesOf = [&](int from, int to) {
        QList<bible::PassageVerse> some;
        for (int i = from; i < to; ++i) {
            if (some.isEmpty() || words[i].first)
                some.append(found.verses.at(words[i].verse));
        }
        return some;
    };
    const auto referenceOn = [&](int from, int to) -> QString {
        if (references == u"none")
            return {};
        if (references == u"verse")
            return bible::written(bible::referenceTo(versesOf(from, to)), found.bookName) + suffix;
        if (references == u"each" || to == words.size())
            return passageReference;
        return {};
    };
    const auto richFor = [&](int from, int to, const QString &reference) {
        RichText text;
        TextParagraph paragraph;
        paragraph.alignment = alignment;
        for (int i = from; i < to; ++i) {
            if (numbers && words[i].first) {
                TextRun number = format;
                number.superscript = 1;
                number.text = QString::number(found.verses.at(words[i].verse).verse.number) + QStringLiteral(" ");
                paragraph.runs.append(number);
            }
            TextRun run = format;
            run.text = words[i].text + (i + 1 < to ? QStringLiteral(" ") : QString());
            if (color && words[i].red)
                run.fill = QColor(0xff, 0x5a, 0x4f);
            if (!paragraph.runs.isEmpty() && paragraph.runs.last().sameFormat(run))
                paragraph.runs.last().text += run.text;
            else
                paragraph.runs.append(run);
        }
        text.paragraphs.append(paragraph);
        if (!reference.isEmpty() && referenceAt < 0) {
            TextParagraph after;
            after.alignment = alignment;
            TextRun run = format;
            run.text = reference;
            after.runs.append(run);
            text.paragraphs.append(after);
        }
        return text;
    };
    const auto fits = [&](int from, int to) {
        // (Measured only: the glyphs are not wanted. With them, a chapter of twenty verses
        // went from found to shown in 564 ms on the 2017 laptop, under the test runner;
        // without, in 155. It is asked about twice for each word: see bibleflow.h. Joined
        // where the box joins its lines, as the slide will draw it: proconvert.cpp.)
        const RichText rich = richFor(from, to, referenceOn(from, to));
        const TextLayoutResult laid = measureText(joins ? joinedLines(rich, joinWith) : rich, room.width());
        if (laid.height > room.height() + 0.5)
            return false;
        for (const QRectF &line : laid.lines) {
            if (line.width() > room.width() + 0.5)
                return false;
        }
        return true;
    };
    bibleflow::Options flowing;
    flowing.mode = options.value("flow").toString() == u"fill" ? bibleflow::Mode::Fill : bibleflow::Mode::Even;
    flowing.breakOnNewVerse = options.value("breakOnNewVerse").toBool();

    // Slides kept as they were made last: where the passage has grown by Verse → or
    // ← Verse round a slide of the last ones made that is live, and nothing else
    // about how they are made has changed, the slides up to the live one (grown at the
    // end) or from it (grown at the start) stay, and the rest are flowed again with the
    // verse added (bibleflow::flow with `kept`).
    QVariantMap choices = options;
    choices.remove(QStringLiteral("live"));
    const int live = options.value("live", -1).toInt();
    QList<bibleflow::Slide> kept;
    bool grewAtStart = false;
    const QList<bible::PassageVerse> before = m_found.value(m_lastRequest).verses;
    if (live >= 0 && live < m_lastSlides.size() && choices == m_lastOptions && found.verses.size() > before.size() && !before.isEmpty()) {
        const auto sameAt = [&](qsizetype at) {
            for (qsizetype i = 0; i < before.size(); ++i) {
                const bible::PassageVerse &a = found.verses.at(at + i);
                const bible::PassageVerse &b = before.at(i);
                if (a.book != b.book || a.chapter != b.chapter || a.verse.number != b.verse.number)
                    return false;
            }
            return true;
        };
        if (sameAt(0)) {
            for (int i = 0; i <= live; ++i)
                kept.append({m_lastSlides.at(i).first, m_lastSlides.at(i).second});
        } else if (const qsizetype added = found.verses.size() - before.size(); sameAt(added)) {
            // (Every word of the old passage is that many words further on.)
            const int shift = int(std::count_if(words.begin(), words.end(), [added](const bibleflow::Word &w) { return w.verse < added; }));
            for (int i = live; i < m_lastSlides.size(); ++i)
                kept.append({m_lastSlides.at(i).first + shift, m_lastSlides.at(i).second + shift});
            grewAtStart = true;
        }
    }
    const QList<bibleflow::Slide> slides = bibleflow::flow(words, flowing, fits, kept);
    // Where the live slide is now, if it was kept
    int liveAt = -1;
    if (!kept.isEmpty())
        liveAt = int(slides.indexOf(grewAtStart ? kept.first() : kept.last()));

    // The reference each slide is given. A kept slide whose words are not to change may
    // be given a longer one now (Passage Each, or the last slide's with Passage Last, the
    // passage having grown at its start); where that would no longer fit, it keeps the
    // one it had.
    QStringList slideReferences;
    for (qsizetype s = 0; s < slides.size(); ++s) {
        const bibleflow::Slide &each = slides.at(s);
        QString reference = referenceOn(each.from, each.to);
        const qsizetype k = kept.indexOf(each);
        if (k >= 0 && referenceAt < 0 && !fits(each.from, each.to)) {
            const int old = grewAtStart ? live + int(k) : int(k);
            reference = m_lastReferences.value(old);
        }
        slideReferences << reference;
    }

    // The presentation: one group, named for the passage; each slide labelled with the
    // verses it holds
    const QString name = bible::written(found.reference, found.bookName) + (found.abbreviation.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(found.abbreviation));
    rv::data::Presentation presentation = proconvert::newPresentation(name);
    presentation.mutable_last_modified_date()->set_seconds(QDateTime::currentSecsSinceEpoch());
    auto *group = presentation.add_cue_groups();
    group->mutable_group()->mutable_uuid()->set_string(workspace::newUuid());
    group->mutable_group()->set_name(bible::written(found.reference, found.bookName).toStdString());
    group->mutable_group()->mutable_hotkey();
    const QSizeF size(base.size().width() > 0 ? base.size().width() : 1920, base.size().height() > 0 ? base.size().height() : 1080);
    for (qsizetype s = 0; s < slides.size(); ++s) {
        const bibleflow::Slide &each = slides.at(s);
        const QString label = bible::written(bible::referenceTo(versesOf(each.from, each.to)), found.bookName);
        rv::data::Cue *cue = proconvert::addBlankCue(&presentation, label.toStdString(), size);
        group->add_cue_identifiers()->set_string(cue->uuid().string());
        rv::data::Slide *slide = cue->mutable_actions(0)->mutable_slide()->mutable_presentation()->mutable_base_slide();
        const std::string uuid = slide->uuid().string();
        *slide = base;
        slide->mutable_uuid()->set_string(uuid);
        for (int i = 0; i < slide->elements_size(); ++i) {
            rv::data::Graphics::Element *element = slide->mutable_elements(i)->mutable_element();
            // The boxes that hold the passage are the slide's own; the rest (shapes,
            // pictures) keep the theme's ids, as a slide dressed in a theme has them
            // (themefile.h). With no theme, every one is the slide's own.
            if (i == verseAt || i == referenceAt || i == copyrightAt || place.isEmpty())
                element->mutable_uuid()->set_string(workspace::newUuid());
            if (!element->has_text())
                continue;
            const QString &reference = slideReferences.at(s);
            if (i == verseAt) {
                proconvert::writeText(element->mutable_text(), richFor(each.from, each.to, reference));
            } else {
                const RichText own = proconvert::readText(element->text());
                const QString words = i == referenceAt ? reference : i == copyrightAt ? found.copyright : QString();
                proconvert::writeText(element->mutable_text(),
                                      RichText::plain(words, own.firstRun(), own.paragraphs.isEmpty() ? Qt::AlignHCenter : own.paragraphs.first().alignment));
            }
        }
    }
    // Each in a folder of its own, under the passage's name, which is what the window
    // shows as the presentation's
    if (!m_made.isValid() || !QDir(m_made.path()).mkpath(QString::number(++m_makes)))
        return failed(QStringLiteral("There is nowhere to put the slides."));
    QString fileName = name;
    fileName.replace(u'/', u'-');
    const QString path = m_made.filePath(QStringLiteral("%1/%2.pro").arg(m_makes).arg(fileName));
    const QString error = proconvert::writePresentation(path, presentation);
    if (!error.isEmpty())
        return failed(error);
    m_lastMade = path;
    m_lastName = name;
    m_lastRequest = request;
    m_lastSlides.clear();
    for (const bibleflow::Slide &each : slides)
        m_lastSlides.append({each.from, each.to});
    m_lastReferences = slideReferences;
    m_lastOptions = choices;
    return {{"path", path}, {"count", int(slides.size())}, {"error", QString()}, {"name", name}, {"request", request}, {"live", liveAt}};
}

QVariantMap Bibles::saveAs(const QString &library)
{
    rv::data::Presentation presentation;
    QString error;
    if (m_lastMade.isEmpty() || !proconvert::readPresentation(m_lastMade, &presentation, &error))
        return {{"path", QString()}, {"error", error.isEmpty() ? QStringLiteral("There are no slides to save.") : error}};
    if (!QFileInfo(library).isDir())
        return {{"path", QString()}, {"error", QStringLiteral("There is no library to put them in.")}};
    QString unique;
    const QString path = proconvert::presentationFileFor(library, m_lastName, QStringLiteral("Scripture"), &unique);
    presentation.set_name(unique.toStdString());
    error = proconvert::writePresentation(path, presentation);
    if (!error.isEmpty())
        return {{"path", QString()}, {"error", error}};
    SessionLog::write("bibles", QStringLiteral("%1 saved as a presentation in %2").arg(unique, QFileInfo(library).fileName()));
    return {{"path", path}, {"error", QString()}};
}

QString Bibles::copyInto(const QString &target, const QString &afterSlideId)
{
    rv::data::Presentation made;
    QString error;
    if (m_lastMade.isEmpty() || !proconvert::readPresentation(m_lastMade, &made, &error))
        return error.isEmpty() ? QStringLiteral("There are no slides to copy.") : error;
    keepCopy(target, m_workspace);
    QString after = afterSlideId;
    for (const rv::data::Cue &cue : made.cues()) {
        const std::string bytes = cue.SerializeAsString();
        QString madeId;
        error = ProDocument::pasteCue(target, after, true, QByteArray::fromStdString(bytes), &madeId);
        if (!error.isEmpty())
            return error;
        after = madeId;
    }
    SessionLog::write("bibles", QStringLiteral("%1 slides of %2 put into %3").arg(made.cues_size()).arg(m_lastName, QFileInfo(target).completeBaseName()));
    return {};
}
