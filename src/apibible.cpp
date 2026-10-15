#include "apibible.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QUrlQuery>

namespace apibible {

namespace {

// A verse's words as they come, piece by piece, with which are red; made one text at
// the end, every run of white space one space (as biblexml's are).
struct Gathered
{
    int book = 0;
    int chapter = 0;
    int verse = 0;
    QString text;
    QList<QPair<int, int>> red;
    bool space = false;

    void add(const QString &piece, bool isRed)
    {
        int start = -1;
        for (const QChar c : piece) {
            if (c.isSpace()) {
                space = !text.isEmpty();
                continue;
            }
            if (space) {
                text += u' ';
                space = false;
            }
            if (isRed && start < 0)
                start = int(text.size());
            text += c;
        }
        if (isRed && start >= 0) {
            if (!red.isEmpty() && red.last().first + red.last().second >= start - 1)
                red.last().second = int(text.size()) - red.last().first;
            else
                red.append({start, int(text.size()) - start});
        }
    }
};

// A verse id, "JHN.3.16", as its book, chapter and verse; false for one that is not
bool verseIdOf(const QString &id, int *book, int *chapter, int *verse)
{
    const QStringList parts = id.split(u'.');
    if (parts.size() < 3)
        return false;
    for (int b = 1; b <= bible::bookCount; ++b) {
        if (bible::usfmId(b) == parts[0]) {
            *book = b;
            *chapter = parts[1].toInt();
            *verse = parts[2].toInt();
            return *chapter > 0 && *verse > 0;
        }
    }
    return false;
}

// Paragraph styles that are headings and not Scripture (USFM's): left out if they come
const QStringList headings {QStringLiteral("s"), QStringLiteral("s1"), QStringLiteral("s2"), QStringLiteral("s3"), QStringLiteral("s4"),
                            QStringLiteral("ms"), QStringLiteral("ms1"), QStringLiteral("ms2"), QStringLiteral("mr"), QStringLiteral("r"),
                            QStringLiteral("sr"), QStringLiteral("sp"), QStringLiteral("cl"), QStringLiteral("mt"), QStringLiteral("mt1"),
                            QStringLiteral("mt2"), QStringLiteral("h")};

struct Walk
{
    QList<QString> order;           // verse ids in the order they first come
    QMap<QString, Gathered> verses; // by id
    QString current;
};

void walk(const QJsonValue &node, Walk &state, bool red)
{
    if (node.isArray()) {
        for (const QJsonValue &item : node.toArray())
            walk(item, state, red);
        return;
    }
    if (!node.isObject())
        return;
    const QJsonObject object = node.toObject();
    const QJsonObject attrs = object.value("attrs").toObject();
    const QString type = object.value("type").toString();
    const QString name = object.value("name").toString();
    const QString style = attrs.value("style").toString();
    if (type == u"text") {
        const QString id = attrs.value("verseId").toString();
        if (!id.isEmpty())
            state.current = id;
        if (state.current.isEmpty())
            return; // words before the first verse: a heading, say
        if (!state.verses.contains(state.current)) {
            Gathered made;
            if (!verseIdOf(state.current, &made.book, &made.chapter, &made.verse))
                return;
            state.verses.insert(state.current, made);
            state.order.append(state.current);
        }
        state.verses[state.current].add(object.value("text").toString(), red);
        return;
    }
    // A verse's number, a note, a heading: no words of a verse
    if (name == u"verse" || name == u"note" || (name == u"para" && headings.contains(style)))
        return;
    walk(object.value("items"), state, red || style == u"wj");
    // (The end of a paragraph or a line of poetry is a space between its words and the next.)
    if (name == u"para" && !state.current.isEmpty() && state.verses.contains(state.current))
        state.verses[state.current].space = !state.verses[state.current].text.isEmpty();
}

QString plain(QString text)
{
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    return text.replace(tag, QString()).simplified();
}

}

QUrl baseUrl()
{
    return QUrl(QStringLiteral("https://rest.api.bible"));
}

QList<Translation> readBibles(const QByteArray &json, QString *error)
{
    QList<Translation> all;
    const QJsonDocument document = QJsonDocument::fromJson(json);
    const QJsonValue data = document.object().value("data");
    if (!data.isArray()) {
        if (error)
            *error = QStringLiteral("api.bible's list of Bibles could not be read.");
        return all;
    }
    for (const QJsonValue &item : data.toArray()) {
        const QJsonObject bible = item.toObject();
        Translation each;
        each.id = bible.value("id").toString();
        each.name = bible.value("name").toString().trimmed();
        each.abbreviation = bible.value("abbreviationLocal").toString().trimmed();
        if (each.abbreviation.isEmpty())
            each.abbreviation = bible.value("abbreviation").toString().trimmed();
        each.language = bible.value("language").toObject().value("name").toString();
        each.description = bible.value("description").toString().trimmed();
        if (!each.id.isEmpty())
            all.append(each);
    }
    std::stable_sort(all.begin(), all.end(), [](const Translation &a, const Translation &b) {
        return a.language != b.language ? a.language < b.language : a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return all;
}

QStringList passageIds(const bible::Reference &reference)
{
    QStringList ids;
    const QString book = bible::usfmId(reference.book);
    if (book.isEmpty())
        return ids;
    for (const bible::Range &range : reference.ranges) {
        if (range.fromChapter == range.toChapter && range.fromVerse > 0 && range.toVerse > 0) {
            ids << (range.fromVerse == range.toVerse
                        ? QStringLiteral("%1.%2.%3").arg(book).arg(range.fromChapter).arg(range.fromVerse)
                        : QStringLiteral("%1.%2.%3-%1.%2.%4").arg(book).arg(range.fromChapter).arg(range.fromVerse).arg(range.toVerse));
            continue;
        }
        // Whole chapters, and stretches across chapters, a chapter at a time: no chapter
        // is longer than api.bible gives at once (200 verses), and the verses not wanted
        // are left out after (versesOf)
        for (int chapter = range.fromChapter; chapter <= range.toChapter; ++chapter)
            ids << QStringLiteral("%1.%2").arg(book).arg(chapter);
    }
    ids.removeDuplicates();
    return ids;
}

QUrl passageUrl(const QString &bibleId, const QString &passageId)
{
    QUrl url = baseUrl();
    url.setPath(QStringLiteral("/v1/bibles/%1/passages/%2").arg(bibleId, passageId));
    QUrlQuery query;
    query.addQueryItem("content-type", "json");
    query.addQueryItem("include-notes", "false");
    query.addQueryItem("include-titles", "false");
    query.addQueryItem("include-chapter-numbers", "false");
    query.addQueryItem("include-verse-numbers", "true");
    query.addQueryItem("include-verse-spans", "false");
    query.addQueryItem("fums-version", "3");
    url.setQuery(query);
    return url;
}

bool readPassage(const QByteArray &json, Passage *passage, QString *error)
{
    *passage = {};
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QJsonObject data = root.value("data").toObject();
    if (data.isEmpty()) {
        if (error)
            *error = QStringLiteral("api.bible's answer could not be read.");
        return false;
    }
    passage->copyright = plain(data.value("copyright").toString());
    passage->fumsToken = root.value("meta").toObject().value("fumsToken").toString();
    Walk state;
    walk(data.value("content"), state, false);
    for (const QString &id : std::as_const(state.order)) {
        const Gathered &g = state.verses.value(id);
        if (g.text.isEmpty())
            continue;
        bible::PassageVerse verse;
        verse.book = g.book;
        verse.chapter = g.chapter;
        verse.verse.number = g.verse;
        verse.verse.text = g.text;
        verse.verse.red = g.red;
        passage->verses.append(verse);
    }
    if (passage->verses.isEmpty()) {
        if (error)
            *error = QStringLiteral("api.bible gave no verses for this passage.");
        return false;
    }
    return true;
}

QList<bible::PassageVerse> versesOf(const QList<Passage> &passages, const bible::Reference &reference)
{
    QList<bible::PassageVerse> all;
    for (const bible::Range &range : reference.ranges) {
        for (const Passage &passage : passages) {
            for (const bible::PassageVerse &v : passage.verses) {
                const bool after = v.chapter > range.fromChapter || (v.chapter == range.fromChapter && v.verse.number >= range.fromVerse);
                const bool before = v.chapter < range.toChapter || (v.chapter == range.toChapter && (range.toVerse == 0 || v.verse.number <= range.toVerse));
                if (!after || !before)
                    continue;
                const bool had = std::any_of(all.cbegin(), all.cend(), [&](const bible::PassageVerse &each) {
                    return each.chapter == v.chapter && each.verse.number == v.verse.number;
                });
                if (!had)
                    all.append(v);
            }
        }
    }
    return all;
}

QString errorOf(int status, const QByteArray &json)
{
    const QString message = QJsonDocument::fromJson(json).object().value("message").toString().trimmed();
    QString said;
    switch (status) {
    case 401: said = QStringLiteral("api.bible did not take the key."); break;
    case 403: said = QStringLiteral("This key may not use that Bible."); break;
    case 404: said = QStringLiteral("api.bible has no such passage in this Bible."); break;
    case 429: said = QStringLiteral("This key has had as many requests as api.bible allows it for now."); break;
    default: said = status > 0 ? QStringLiteral("api.bible answered with an error (%1).").arg(status) : QStringLiteral("api.bible could not be reached.");
    }
    return message.isEmpty() ? said : said + QStringLiteral(" It said: ") + message;
}

QUrl fumsUrl(const QStringList &tokens, const QString &deviceId, const QString &sessionId)
{
    QUrl url(QStringLiteral("https://fums.api.bible/f3"));
    QUrlQuery query;
    for (const QString &token : tokens)
        query.addQueryItem("t", token);
    query.addQueryItem("dId", deviceId);
    query.addQueryItem("sId", sessionId);
    url.setQuery(query);
    return url;
}

}
