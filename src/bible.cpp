#include "bible.h"

#include <QRegularExpression>

#include <algorithm>

namespace bible {

namespace {

struct BookInfo
{
    const char *english;
    const char *usfm;
    const char *osis;
    // Other names and usual short forms, written as they are compared: small letters,
    // no spaces or full stops, a leading number as a digit
    const char *others;
};

// The 66 books, in order. The short forms are the usual ones of English Bibles and
// commentaries (SBL's, and the shorter ones people type); where one short form could be
// two books, it is given to the one it is usually taken for (Phil is Philippians, Jud
// is Jude, Jdg Judges), and the other has to be written longer.
const BookInfo books[bookCount] = {
    {"Genesis", "GEN", "Gen", "ge gn"},
    {"Exodus", "EXO", "Exod", "ex exo"},
    {"Leviticus", "LEV", "Lev", "le lv"},
    {"Numbers", "NUM", "Num", "nu nm nb"},
    {"Deuteronomy", "DEU", "Deut", "dt de deu"},
    {"Joshua", "JOS", "Josh", "jos jsh"},
    {"Judges", "JDG", "Judg", "jdg jg jdgs"},
    {"Ruth", "RUT", "Ruth", "ru rth rt"},
    {"1 Samuel", "1SA", "1Sam", "1sa 1sm 1s"},
    {"2 Samuel", "2SA", "2Sam", "2sa 2sm 2s"},
    {"1 Kings", "1KI", "1Kgs", "1ki 1kg 1kgs"},
    {"2 Kings", "2KI", "2Kgs", "2ki 2kg 2kgs"},
    {"1 Chronicles", "1CH", "1Chr", "1ch 1chr 1chron"},
    {"2 Chronicles", "2CH", "2Chr", "2ch 2chr 2chron"},
    {"Ezra", "EZR", "Ezra", "ezr"},
    {"Nehemiah", "NEH", "Neh", "ne"},
    {"Esther", "EST", "Esth", "es est"},
    {"Job", "JOB", "Job", "jb"},
    {"Psalms", "PSA", "Ps", "psalm ps psa pss psm pslm"},
    {"Proverbs", "PRO", "Prov", "pr prv pro"},
    {"Ecclesiastes", "ECC", "Eccl", "ec ecc eccl eccles qoh qoheleth"},
    {"Song of Songs", "SNG", "Song", "songofsolomon song sos so sng canticles cant"},
    {"Isaiah", "ISA", "Isa", "is"},
    {"Jeremiah", "JER", "Jer", "je jr"},
    {"Lamentations", "LAM", "Lam", "la lm"},
    {"Ezekiel", "EZK", "Ezek", "eze ezk ezek"},
    {"Daniel", "DAN", "Dan", "da dn"},
    {"Hosea", "HOS", "Hos", "ho"},
    {"Joel", "JOL", "Joel", "jl joe"},
    {"Amos", "AMO", "Amos", "am"},
    {"Obadiah", "OBA", "Obad", "ob obd"},
    {"Jonah", "JON", "Jonah", "jon jnh"},
    {"Micah", "MIC", "Mic", "mi mc"},
    {"Nahum", "NAM", "Nah", "na"},
    {"Habakkuk", "HAB", "Hab", "hb"},
    {"Zephaniah", "ZEP", "Zeph", "zep zp"},
    {"Haggai", "HAG", "Hag", "hg"},
    {"Zechariah", "ZEC", "Zech", "zec zc"},
    {"Malachi", "MAL", "Mal", "ml"},
    {"Matthew", "MAT", "Matt", "mt mat matt"},
    {"Mark", "MRK", "Mark", "mk mr mrk"},
    {"Luke", "LUK", "Luke", "lk lu luk"},
    {"John", "JHN", "John", "jn jhn joh"},
    {"Acts", "ACT", "Acts", "ac act"},
    {"Romans", "ROM", "Rom", "ro rm"},
    {"1 Corinthians", "1CO", "1Cor", "1co 1cor"},
    {"2 Corinthians", "2CO", "2Cor", "2co 2cor"},
    {"Galatians", "GAL", "Gal", "ga"},
    {"Ephesians", "EPH", "Eph", "ep"},
    {"Philippians", "PHP", "Phil", "phil php pp"},
    {"Colossians", "COL", "Col", "co"},
    {"1 Thessalonians", "1TH", "1Thess", "1th 1thes 1thess"},
    {"2 Thessalonians", "2TH", "2Thess", "2th 2thes 2thess"},
    {"1 Timothy", "1TI", "1Tim", "1ti 1tm"},
    {"2 Timothy", "2TI", "2Tim", "2ti 2tm"},
    {"Titus", "TIT", "Titus", "ti tit"},
    {"Philemon", "PHM", "Phlm", "phm phlm philem"},
    {"Hebrews", "HEB", "Heb", "he"},
    {"James", "JAS", "Jas", "jm ja"},
    {"1 Peter", "1PE", "1Pet", "1pe 1pt 1p"},
    {"2 Peter", "2PE", "2Pet", "2pe 2pt 2p"},
    {"1 John", "1JN", "1John", "1jn 1jo 1jhn"},
    {"2 John", "2JN", "2John", "2jn 2jo 2jhn"},
    {"3 John", "3JN", "3John", "3jn 3jo 3jhn"},
    {"Jude", "JUD", "Jude", "jud jd"},
    {"Revelation", "REV", "Rev", "re rv revelations apocalypse"},
};

// The books of one chapter: Obadiah, Philemon, 2 John, 3 John, Jude
const int oneChapter[] = {31, 57, 63, 64, 65};

const int chapters[bookCount] = {50, 40, 27, 36, 34, 24, 21, 4, 31, 24, 22, 25, 29, 36, 10, 13, 10, 42, 150, 31, 12, 8, 66,
                                 52, 5, 48, 12, 14, 3, 9, 1, 4, 7, 3, 3, 3, 2, 14, 4, 28, 16, 24, 21, 28, 16, 16, 13, 6, 6, 4,
                                 4, 5, 3, 6, 4, 3, 1, 13, 5, 5, 3, 5, 1, 1, 1, 22};

// A name as it is compared: small letters, with no spaces, full stops or other marks, a
// leading "I", "II", "III", "First" ... "3rd" as its digit.
QString key(QString name)
{
    name = name.trimmed().toLower();
    static const QRegularExpression ordinal(QStringLiteral("^(iii|ii|i|1st|2nd|3rd|first|second|third)[\\s.]+(?=\\p{L})"));
    const QRegularExpressionMatch found = ordinal.match(name);
    if (found.hasMatch()) {
        const QString word = found.captured(1);
        const QString digit = (word == u"i" || word == u"1st" || word == u"first") ? QStringLiteral("1")
                            : (word == u"ii" || word == u"2nd" || word == u"second") ? QStringLiteral("2") : QStringLiteral("3");
        name = digit + name.mid(found.capturedLength());
    }
    QString kept;
    for (const QChar c : std::as_const(name)) {
        if (c.isLetterOrNumber())
            kept += c;
    }
    return kept;
}

// The book a key is, among names of which it is the whole or (failing that) the start of
// exactly one. 0 for none, or for more than one.
template<typename Names>
int matchOf(const QString &wanted, const Names &names)
{
    for (const auto &[number, name] : names) {
        if (key(name) == wanted)
            return number;
    }
    int found = 0;
    for (const auto &[number, name] : names) {
        if (key(name).startsWith(wanted)) {
            if (found != 0 && found != number)
                return 0;
            found = number;
        }
    }
    return found;
}

QString dash()
{
    return QStringLiteral("–");
}

}

const Book *Bible::book(int number) const
{
    for (const Book &each : books) {
        if (each.number == number)
            return &each;
    }
    return nullptr;
}

QString englishName(int book)
{
    return book >= 1 && book <= bookCount ? QString::fromLatin1(books[book - 1].english) : QString();
}

QString usfmId(int book)
{
    return book >= 1 && book <= bookCount ? QString::fromLatin1(books[book - 1].usfm) : QString();
}

QString osisId(int book)
{
    return book >= 1 && book <= bookCount ? QString::fromLatin1(books[book - 1].osis) : QString();
}

int chapterCount(int book)
{
    return book >= 1 && book <= bookCount ? chapters[book - 1] : 0;
}

bool singleChapter(int book)
{
    return std::find(std::begin(oneChapter), std::end(oneChapter), book) != std::end(oneChapter);
}

int bookNumber(const QString &name, const QList<Book> &own)
{
    const QString wanted = key(name);
    if (wanted.isEmpty())
        return 0;
    // A Bible's own names first: they are what someone using it would type
    QList<std::pair<int, QString>> owned;
    for (const Book &book : own)
        owned.append({book.number, book.name});
    if (const int number = matchOf(wanted, owned))
        return number;
    // Then the English names, the two ids and the short forms, whole
    for (int i = 0; i < bookCount; ++i) {
        const BookInfo &info = books[i];
        if (key(QString::fromLatin1(info.english)) == wanted || key(QString::fromLatin1(info.usfm)) == wanted
            || key(QString::fromLatin1(info.osis)) == wanted)
            return i + 1;
        const QStringList others = QString::fromLatin1(info.others).split(u' ', Qt::SkipEmptyParts);
        if (others.contains(wanted))
            return i + 1;
    }
    // And last, the start of exactly one English name: "Deuter", "Revel"
    int found = 0;
    for (int i = 0; i < bookCount; ++i) {
        if (key(QString::fromLatin1(books[i].english)).startsWith(wanted)) {
            if (found != 0)
                return 0;
            found = i + 1;
        }
    }
    return found;
}

Reference parse(const QString &typed, const QList<Book> &own)
{
    Reference reference;
    QString text = typed.trimmed();
    // Dashes of every kind are one
    for (const QChar c : {QChar(0x2010), QChar(0x2011), QChar(0x2012), QChar(0x2013), QChar(0x2014), QChar(0x2015), QChar(0x2212)})
        text.replace(c, u'-');
    // The book is everything up to its last letter; the numbers come after it. ("1 Cor
    // 13" has its book's number before the name, and none of the numbers after.)
    qsizetype last = -1;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i).isLetter())
            last = i;
    }
    if (last < 0)
        return reference;
    const QString name = text.left(last + 1);
    const QString numbers = text.mid(last + 1).trimmed();
    reference.book = bookNumber(name, own);
    if (reference.book == 0)
        return reference;
    for (const Book &book : own) {
        if (book.number == reference.book)
            reference.bookName = book.name;
    }
    const bool one = singleChapter(reference.book);
    if (numbers.isEmpty()) {
        // A book alone is its first chapter, as ProPresenter's Book menu opens one
        reference.ranges.append({1, 0, 1, 0});
        return reference;
    }

    // Each stretch, between commas (or semicolons)
    static const QRegularExpression number(QStringLiteral("\\d+"));
    int chapter = 0;
    const QStringList stretches = numbers.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts);
    for (int s = 0; s < stretches.size(); ++s) {
        const QString stretch = stretches.at(s).trimmed();
        const qsizetype dashAt = stretch.indexOf(u'-');
        const auto numbersIn = [](const QString &part) {
            QList<int> all;
            auto found = number.globalMatch(part);
            while (found.hasNext())
                all.append(found.next().captured().toInt());
            return all;
        };
        const QList<int> left = numbersIn(dashAt >= 0 ? stretch.left(dashAt) : stretch);
        const QList<int> right = dashAt >= 0 ? numbersIn(stretch.mid(dashAt + 1)) : QList<int>();
        if (left.isEmpty() || left.size() > 3 || right.size() > 2 || (dashAt >= 0 && right.isEmpty()))
            return {};
        if (std::any_of(left.cbegin(), left.cend(), [](int n) { return n <= 0; })
            || std::any_of(right.cbegin(), right.cend(), [](int n) { return n <= 0; }))
            return {};
        Range range;
        // What the numbers before the dash are: a chapter and a verse, a chapter alone
        // (in the first stretch, of a book of chapters), or a verse alone (later on, or in
        // a book of one chapter).
        const bool chapterAlone = left.size() == 1 && s == 0 && !one;
        if (left.size() == 3 && dashAt < 0) {
            // "Matt 1 2 3": a chapter, and a verse to a verse
            range = {left[0], left[1], left[0], left[2]};
        } else if (left.size() >= 2) {
            chapter = left[0];
            range.fromChapter = chapter;
            range.fromVerse = left[1];
            range.toChapter = chapter;
            range.toVerse = left[1];
            if (left.size() == 3)
                return {};
        } else if (chapterAlone) {
            chapter = left[0];
            range = {chapter, 0, chapter, 0};
        } else {
            if (chapter == 0)
                chapter = 1; // a book of one chapter
            range = {chapter, left[0], chapter, left[0]};
        }
        if (!right.isEmpty()) {
            if (right.size() == 2) {
                range.toChapter = right[0];
                range.toVerse = right[1];
                chapter = right[0];
            } else if (chapterAlone) {
                // "Ruth 1-2": chapters
                range.toChapter = right[0];
                range.toVerse = 0;
                chapter = right[0];
            } else {
                range.toVerse = right[0];
            }
        }
        // A stretch that ends before it starts is no passage anyone means: refused.
        if (range.toChapter < range.fromChapter
            || (range.toChapter == range.fromChapter && range.toVerse != 0 && range.fromVerse > range.toVerse))
            return {};
        reference.ranges.append(range);
    }
    return reference;
}

QString written(const Reference &reference, const QString &bookName)
{
    if (!reference.valid())
        return {};
    QString book = !bookName.isEmpty() ? bookName : !reference.bookName.isEmpty() ? reference.bookName : englishName(reference.book);
    const bool one = singleChapter(reference.book);
    // One chapter of the Psalms is a Psalm
    if (reference.book == 19 && bookName.isEmpty() && reference.bookName.isEmpty() && reference.ranges.size() == 1
        && reference.ranges.first().fromChapter == reference.ranges.first().toChapter)
        book = QStringLiteral("Psalm");
    QString out = book + u' ';
    int chapter = 0;
    for (int i = 0; i < reference.ranges.size(); ++i) {
        const Range &r = reference.ranges.at(i);
        if (i > 0)
            out += r.fromChapter == chapter ? QStringLiteral(", ") : QStringLiteral("; ");
        const bool sameChapter = r.fromChapter == chapter && i > 0;
        if (r.fromVerse == 0 && r.toVerse == 0) {
            // Whole chapters
            out += QString::number(r.fromChapter);
            if (r.toChapter != r.fromChapter)
                out += dash() + QString::number(r.toChapter);
        } else {
            const QString from = (one || sameChapter) ? QString::number(r.fromVerse)
                                                      : QString::number(r.fromChapter) + u':' + QString::number(std::max(1, r.fromVerse));
            out += from;
            if (r.toChapter != r.fromChapter)
                out += dash() + QString::number(r.toChapter) + u':' + (r.toVerse == 0 ? QStringLiteral("end") : QString::number(r.toVerse));
            else if (r.toVerse != r.fromVerse)
                out += dash() + (r.toVerse == 0 ? QStringLiteral("end") : QString::number(r.toVerse));
        }
        chapter = r.toChapter;
    }
    return out;
}

QList<PassageVerse> passage(const Bible &bible, const Reference &reference, QString *error)
{
    QList<PassageVerse> verses;
    const Book *book = bible.book(reference.book);
    if (!reference.valid() || !book) {
        if (error)
            *error = reference.valid() ? QStringLiteral("This Bible has no book of %1.").arg(englishName(reference.book))
                                       : QStringLiteral("That is not a passage that can be read.");
        return verses;
    }
    for (const Range &range : reference.ranges) {
        for (const Chapter &chapter : book->chapters) {
            if (chapter.number < range.fromChapter || chapter.number > range.toChapter)
                continue;
            for (const Verse &verse : chapter.verses) {
                if (chapter.number == range.fromChapter && range.fromVerse != 0 && verse.number < range.fromVerse)
                    continue;
                if (chapter.number == range.toChapter && range.toVerse != 0 && verse.number > range.toVerse)
                    continue;
                verses.append({book->number, chapter.number, verse});
            }
        }
    }
    if (verses.isEmpty() && error)
        *error = QStringLiteral("This Bible has none of %1.").arg(written(reference, book->name));
    return verses;
}

Reference referenceTo(const QList<PassageVerse> &verses)
{
    Reference reference;
    if (verses.isEmpty())
        return reference;
    reference.book = verses.first().book;
    for (const PassageVerse &v : verses) {
        if (!reference.ranges.isEmpty()) {
            Range &open = reference.ranges.last();
            const bool follows = (v.chapter == open.toChapter && v.verse.number == open.toVerse + 1)
                              || (v.chapter == open.toChapter + 1 && v.verse.number == 1);
            if (follows && v.book == reference.book) {
                open.toChapter = v.chapter;
                open.toVerse = v.verse.number;
                continue;
            }
        }
        reference.ranges.append({v.chapter, v.verse.number, v.chapter, v.verse.number});
    }
    return reference;
}

namespace {

// Where a verse is in a Bible: its book, chapter and verse by place; -1 for none
struct Place
{
    int book = -1;
    int chapter = -1;
    int verse = -1;
};

Place placeOf(const Bible &bible, const PassageVerse &v)
{
    for (int b = 0; b < bible.books.size(); ++b) {
        if (bible.books[b].number != v.book)
            continue;
        const QList<Chapter> &chapters = bible.books[b].chapters;
        for (int c = 0; c < chapters.size(); ++c) {
            if (chapters[c].number != v.chapter)
                continue;
            for (int i = 0; i < chapters[c].verses.size(); ++i) {
                if (chapters[c].verses[i].number == v.verse.number)
                    return {b, c, i};
            }
        }
    }
    return {};
}

Reference single(const Bible &bible, const Place &p)
{
    const Book &book = bible.books[p.book];
    const Chapter &chapter = book.chapters[p.chapter];
    const int verse = chapter.verses[p.verse].number;
    Reference reference;
    reference.book = book.number;
    if (book.number > bookCount)
        reference.bookName = book.name;
    reference.ranges.append({chapter.number, verse, chapter.number, verse});
    return reference;
}

}

Reference nextVerse(const Bible &bible, const QList<PassageVerse> &verses)
{
    if (verses.isEmpty())
        return {};
    Place p = placeOf(bible, verses.last());
    if (p.book < 0)
        return {};
    // The next verse of the chapter, or the first of the next chapter with any, or of the
    // next book
    while (true) {
        const Book &book = bible.books[p.book];
        if (p.verse + 1 < book.chapters[p.chapter].verses.size())
            return single(bible, {p.book, p.chapter, p.verse + 1});
        p.verse = -1;
        if (++p.chapter < book.chapters.size())
            continue;
        p.chapter = 0;
        if (++p.book >= bible.books.size())
            return {};
        if (bible.books[p.book].chapters.isEmpty())
            return {};
    }
}

Reference previousVerse(const Bible &bible, const QList<PassageVerse> &verses)
{
    if (verses.isEmpty())
        return {};
    Place p = placeOf(bible, verses.first());
    if (p.book < 0)
        return {};
    while (true) {
        if (p.verse - 1 >= 0)
            return single(bible, {p.book, p.chapter, p.verse - 1});
        if (--p.chapter < 0) {
            if (--p.book < 0 || bible.books[p.book].chapters.isEmpty())
                return {};
            p.chapter = int(bible.books[p.book].chapters.size()) - 1;
        }
        p.verse = int(bible.books[p.book].chapters[p.chapter].verses.size());
    }
}

}
