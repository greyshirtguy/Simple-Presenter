#include "biblexml.h"

#include <QHash>
#include <QXmlStreamReader>

namespace biblexml {

namespace {

// A verse's words as they are read, piece by piece, with which pieces are red; made one
// text at the end, with every run of white space one space and none at either end.
struct Words
{
    QList<std::pair<QString, bool>> pieces;

    void add(const QString &text, bool red) { pieces.append({text, red}); }
    bool empty() const { return pieces.isEmpty(); }

    bible::Verse verse(int number) const
    {
        bible::Verse verse;
        verse.number = number;
        bool space = false;
        for (const auto &[text, red] : pieces) {
            int start = -1;
            for (const QChar c : text) {
                if (c.isSpace()) {
                    space = !verse.text.isEmpty();
                    continue;
                }
                if (space) {
                    verse.text += u' ';
                    space = false;
                }
                if (red && start < 0)
                    start = int(verse.text.size());
                verse.text += c;
            }
            if (red && start >= 0) {
                // (Next to a red stretch just before it, the two are one.)
                if (!verse.red.isEmpty() && verse.red.last().first + verse.red.last().second >= start - 1)
                    verse.red.last().second = int(verse.text.size()) - verse.red.last().first;
                else
                    verse.red.append({start, int(verse.text.size()) - start});
            }
        }
        return verse;
    }
};

// What is being built: the Bible, and where in it the reading is
struct Builder
{
    bible::Bible *bible = nullptr;
    QHash<int, int> bookAt; // a book's number to its place in bible->books
    int book = -1;          // the place of the book being read
    int chapter = -1;       // and of its chapter
    int nextOutside = bible::bookCount + 1;
    int verses = 0;

    void startBook(int number, const QString &name)
    {
        if (!bookAt.contains(number)) {
            bible::Book made;
            made.number = number;
            made.name = !name.trimmed().isEmpty() ? name.trimmed() : bible::englishName(number);
            bible->books.append(made);
            bookAt.insert(number, int(bible->books.size()) - 1);
        }
        book = bookAt.value(number);
        chapter = -1;
    }

    void startChapter(int number)
    {
        if (book < 0)
            return;
        QList<bible::Chapter> &chapters = bible->books[book].chapters;
        for (int i = 0; i < chapters.size(); ++i) {
            if (chapters[i].number == number) {
                chapter = i;
                return;
            }
        }
        bible::Chapter made;
        made.number = number;
        chapters.append(made);
        chapter = int(chapters.size()) - 1;
    }

    void addVerse(int number, const Words &words)
    {
        if (book < 0 || chapter < 0 || number <= 0)
            return;
        const bible::Verse verse = words.verse(number);
        if (verse.text.isEmpty())
            return;
        bible->books[book].chapters[chapter].verses.append(verse);
        ++verses;
    }

    // A book outside the 66, by its name: given the next number past them
    int outside(const QString &name)
    {
        for (const bible::Book &each : std::as_const(bible->books)) {
            if (each.number > bible::bookCount && each.name == name)
                return each.number;
        }
        return nextOutside++;
    }
};

// The leading digits of an attribute ("12", or "12-13" for verses joined); 0 for none
int leadingNumber(const QString &text)
{
    int n = 0;
    bool any = false;
    for (const QChar c : text.trimmed()) {
        if (!c.isDigit())
            break;
        n = n * 10 + c.digitValue();
        any = true;
    }
    return any ? n : 0;
}

QString attribute(const QXmlStreamReader &xml, const char *name)
{
    return xml.attributes().value(QLatin1String(name)).toString();
}

bool named(const QXmlStreamReader &xml, const char *name)
{
    return xml.name().compare(QLatin1String(name), Qt::CaseInsensitive) == 0;
}

// Reads the words of the element the reader is at, to its end: its text and its
// children's, but for what is no part of the verse
void wordsOf(QXmlStreamReader &xml, Words *words, bool red, const QStringList &skipped, const QStringList &reds)
{
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement())
            return;
        if (xml.isCharacters()) {
            words->add(xml.text().toString(), red);
        } else if (xml.isStartElement()) {
            const QString name = xml.name().toString().toLower();
            if (skipped.contains(name)) {
                xml.skipCurrentElement();
            } else if (name == u"br" || name == u"lb") {
                words->add(QStringLiteral(" "), red);
                xml.skipCurrentElement();
            } else {
                // Red: Zefania's STYLE in red, or OSIS's quotation of Jesus
                const QString css = attribute(xml, "css").toLower() + attribute(xml, "fs").toLower();
                const bool isRed = reds.contains(name)
                                && (css.contains(u"red") || css.contains(u"#ff0000") || css.contains(u"#f00")
                                    || attribute(xml, "who").compare(QLatin1String("Jesus"), Qt::CaseInsensitive) == 0);
                wordsOf(xml, words, red || isRed, skipped, reds);
            }
        }
    }
}

// ---- Zefania

void readZefania(QXmlStreamReader &xml, Builder &out)
{
    const QStringList skipped {QStringLiteral("note"), QStringLiteral("xref"), QStringLiteral("media"), QStringLiteral("prolog"),
                               QStringLiteral("caption"), QStringLiteral("remark")};
    const QStringList reds {QStringLiteral("style")};
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        if (named(xml, "XMLBIBLE")) {
            const QString name = attribute(xml, "biblename");
            if (!name.isEmpty())
                out.bible->name = name;
        } else if (named(xml, "title") && out.bible->name.isEmpty()) {
            out.bible->name = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
        } else if (named(xml, "identifier")) {
            out.bible->abbreviation = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
        } else if (named(xml, "rights")) {
            out.bible->copyright = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
        } else if (named(xml, "BIBLEBOOK")) {
            int number = leadingNumber(attribute(xml, "bnumber"));
            const QString name = attribute(xml, "bname");
            if (number <= 0)
                number = bible::bookNumber(name);
            if (number <= 0)
                number = out.outside(name);
            out.startBook(number, name);
        } else if (named(xml, "CHAPTER")) {
            out.startChapter(leadingNumber(attribute(xml, "cnumber")));
        } else if (named(xml, "VERS")) {
            const int number = leadingNumber(attribute(xml, "vnumber"));
            Words words;
            wordsOf(xml, &words, false, skipped, reds);
            out.addVerse(number, words);
        }
    }
}

// ---- OSIS

// A book, chapter and verse from an OSIS id ("Gen.1.1", or the first of several)
struct OsisId
{
    QString book;
    int chapter = 0;
    int verse = 0;
};

OsisId osisIdOf(const QString &id)
{
    const QStringList parts = id.trimmed().split(u' ').value(0).split(u'.');
    OsisId found;
    if (parts.size() >= 3) {
        found.book = parts[0];
        found.chapter = leadingNumber(parts[1]);
        found.verse = leadingNumber(parts[2]);
    }
    return found;
}

void readOsis(QXmlStreamReader &xml, Builder &out)
{
    // The 66 by OSIS's ids
    QHash<QString, int> ids;
    for (int i = 1; i <= bible::bookCount; ++i)
        ids.insert(bible::osisId(i).toLower(), i);
    const auto bookFor = [&](const QString &id) {
        const int number = ids.value(id.toLower());
        return number > 0 ? number : out.outside(id);
    };
    const QStringList skipped {QStringLiteral("note"), QStringLiteral("title"), QStringLiteral("reference"), QStringLiteral("rdg"),
                               QStringLiteral("figure")};
    bool inHeader = false;
    // A verse being read between its two milestones, and whether its words are red
    int openVerse = 0;
    Words words;
    bool red = false;
    QString openBook;
    int openChapter = 0;
    const auto close = [&] {
        if (openVerse > 0) {
            out.startBook(bookFor(openBook), {});
            out.startChapter(openChapter);
            out.addVerse(openVerse, words);
        }
        openVerse = 0;
        words = {};
    };
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement()) {
            if (named(xml, "header"))
                inHeader = false;
            else if (named(xml, "q") && red && openVerse > 0)
                red = false;
            continue;
        }
        if (xml.isCharacters()) {
            if (openVerse > 0)
                words.add(xml.text().toString(), red);
            continue;
        }
        if (!xml.isStartElement())
            continue;
        if (named(xml, "osisText")) {
            out.bible->abbreviation = attribute(xml, "osisIDWork");
        } else if (named(xml, "header")) {
            inHeader = true;
        } else if (inHeader && named(xml, "title")) {
            if (out.bible->name.isEmpty())
                out.bible->name = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
        } else if (inHeader && named(xml, "rights")) {
            if (out.bible->copyright.isEmpty())
                out.bible->copyright = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
        } else if (inHeader) {
            continue;
        } else if (named(xml, "verse")) {
            const QString start = attribute(xml, "sID");
            const QString end = attribute(xml, "eID");
            if (!end.isEmpty()) {
                close();
            } else if (!start.isEmpty() || !attribute(xml, "osisID").isEmpty()) {
                close();
                const OsisId id = osisIdOf(!attribute(xml, "osisID").isEmpty() ? attribute(xml, "osisID") : start);
                if (start.isEmpty()) {
                    // A verse that holds its words: read them now
                    Words held;
                    wordsOf(xml, &held, red, skipped, {QStringLiteral("q")});
                    out.startBook(bookFor(id.book), {});
                    out.startChapter(id.chapter);
                    out.addVerse(id.verse, held);
                } else {
                    openVerse = id.verse;
                    openBook = id.book;
                    openChapter = id.chapter;
                }
            }
        } else if (named(xml, "q")) {
            // The words of Jesus: a quotation of his, held or between milestones
            const bool jesus = attribute(xml, "who").compare(QLatin1String("Jesus"), Qt::CaseInsensitive) == 0;
            if (!attribute(xml, "eID").isEmpty())
                red = false;
            else if (jesus)
                red = true;
        } else if (named(xml, "lb") || named(xml, "l")) {
            if (openVerse > 0)
                words.add(QStringLiteral(" "), red);
        } else if (openVerse > 0 && skipped.contains(xml.name().toString().toLower())) {
            xml.skipCurrentElement();
        }
    }
    close();
}

// ---- Beblia, OpenSong

void readNumbered(QXmlStreamReader &xml, Builder &out, bool openSong)
{
    int order = 0;
    const char *bookTag = openSong ? "b" : "book";
    const char *chapterTag = openSong ? "c" : "chapter";
    const char *verseTag = openSong ? "v" : "verse";
    const char *numberAttr = openSong ? "n" : "number";
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        if (named(xml, "bible")) {
            const QString name = attribute(xml, "translation").isEmpty() ? attribute(xml, "name") : attribute(xml, "translation");
            if (!name.isEmpty())
                out.bible->name = name;
            if (!attribute(xml, "copyright").isEmpty())
                out.bible->copyright = attribute(xml, "copyright");
        } else if (named(xml, bookTag)) {
            ++order;
            QString name = openSong ? attribute(xml, "n") : attribute(xml, "name");
            int number = openSong ? 0 : leadingNumber(attribute(xml, "number"));
            if (number <= 0 && !name.isEmpty())
                number = bible::bookNumber(name);
            // (A file in another language names its books in it: they are taken in the
            // order of the 66.)
            if (number <= 0)
                number = order <= bible::bookCount ? order : out.outside(name);
            out.startBook(number, name);
        } else if (named(xml, chapterTag)) {
            out.startChapter(leadingNumber(attribute(xml, numberAttr)));
        } else if (named(xml, verseTag)) {
            const int number = leadingNumber(attribute(xml, numberAttr));
            Words words;
            wordsOf(xml, &words, false, {QStringLiteral("note")}, {});
            out.addVerse(number, words);
        }
    }
}

}

Format detect(const QByteArray &data)
{
    QXmlStreamReader xml(data);
    QString root;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        const QString name = xml.name().toString().toLower();
        if (root.isEmpty()) {
            if (name == u"xmlbible")
                return Format::Zefania;
            if (name == u"osis" || name == u"osistext")
                return Format::Osis;
            if (name != u"bible")
                return Format::Unknown;
            root = name;
            continue;
        }
        // Under <bible>: Beblia's testaments and books, or OpenSong's b
        if (name == u"testament" || name == u"book")
            return Format::Beblia;
        if (name == u"b")
            return Format::OpenSong;
        return Format::Unknown;
    }
    return Format::Unknown;
}

QString formatName(Format format)
{
    switch (format) {
    case Format::Zefania: return QStringLiteral("Zefania");
    case Format::Osis: return QStringLiteral("OSIS");
    case Format::Beblia: return QStringLiteral("Beblia");
    case Format::OpenSong: return QStringLiteral("OpenSong");
    case Format::Unknown: break;
    }
    return QStringLiteral("unknown");
}

bool read(const QByteArray &data, const QString &fallbackName, bible::Bible *bible, QString *error, Format *format)
{
    *bible = {};
    const Format kind = detect(data);
    if (format)
        *format = kind;
    if (kind == Format::Unknown) {
        if (error)
            *error = QStringLiteral("This is not a Bible file that can be read here: Zefania, OSIS, Beblia and OpenSong XML can be.");
        return false;
    }
    Builder out;
    out.bible = bible;
    QXmlStreamReader xml(data);
    switch (kind) {
    case Format::Zefania: readZefania(xml, out); break;
    case Format::Osis: readOsis(xml, out); break;
    case Format::Beblia: readNumbered(xml, out, false); break;
    case Format::OpenSong: readNumbered(xml, out, true); break;
    case Format::Unknown: break;
    }
    if (xml.hasError()) {
        if (error)
            *error = QStringLiteral("The file could not be read: %1 (line %2).").arg(xml.errorString()).arg(xml.lineNumber());
        return false;
    }
    if (out.verses == 0) {
        if (error)
            *error = QStringLiteral("There are no verses in this file that could be read.");
        return false;
    }
    if (bible->name.isEmpty())
        bible->name = fallbackName;
    // The books in the order of the 66, those outside them after; each chapter's verses
    // in order of their numbers, as files do not always have them
    std::stable_sort(bible->books.begin(), bible->books.end(), [](const bible::Book &a, const bible::Book &b) { return a.number < b.number; });
    for (bible::Book &book : bible->books) {
        std::stable_sort(book.chapters.begin(), book.chapters.end(), [](const bible::Chapter &a, const bible::Chapter &b) { return a.number < b.number; });
        for (bible::Chapter &chapter : book.chapters)
            std::stable_sort(chapter.verses.begin(), chapter.verses.end(), [](const bible::Verse &a, const bible::Verse &b) { return a.number < b.number; });
    }
    return true;
}

}
