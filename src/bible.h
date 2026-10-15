#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// What a Bible is, here: its books, chapters and verses, the 66 books by number and
// name, and a reference to a passage of it, read from what someone types.
//
// Plain values and functions, with nothing of the windows or the network in them
// (tests/unit/tst_bible.cpp). Where a Bible comes from is elsewhere: an XML file the
// person has (biblexml.h), or api.bible, asked for each passage as it is wanted
// (apibible.h). No Bible's words are in this app: they are other people's, and most
// translations are copyright. How a passage is laid out on slides is bibleflow.h.
//
// The books are the 66 of the Protestant canon, in its order, numbered 1 (Genesis) to
// 66 (Revelation), each with the id USFM gives it ("GEN", "JHN"), which is the id
// api.bible goes by, and the one OSIS gives it ("Gen", "John"). Books outside them (the
// Apocrypha, which some Bibles have) are read where a file has them and kept by their
// own names, numbered from 67; a reference reaches them by those names.
namespace bible {

struct Verse
{
    int number = 0;
    QString text;
    // Stretches of the text in red, "red letter" Bibles' words of Jesus: start and
    // length, in the text's units. (ProPresenter's Preserve Font Color shows them.)
    QList<QPair<int, int>> red;
};

struct Chapter
{
    int number = 0;
    QList<Verse> verses;
};

struct Book
{
    int number = 0; // 1 to 66, or 67 and on for a book outside them
    QString name;   // as the Bible names it, or the English name where it does not
    QList<Chapter> chapters;
};

struct Bible
{
    QString name;         // "World English Bible"
    QString abbreviation; // "WEB"
    QString copyright;    // what the file says of it, to be shown with its words
    QList<Book> books;

    const Book *book(int number) const;
};

// ---- The books

// How many there are, and each one's names: its English name, USFM's id and OSIS's id.
// Empty for a number that is no book of the 66.
constexpr int bookCount = 66;
QString englishName(int book);
QString usfmId(int book);
QString osisId(int book);
// The number of a book by any of those ids, or by its name or a usual short form of it
// in English ("Matt", "Mt", "1 Cor", "I John", "Song of Solomon", "Ps"), case and
// spaces and full stops not counting. 0 if it is none. A Bible's own names (`own`, in
// other languages, say) are looked at too, first.
int bookNumber(const QString &name, const QList<Book> &own = {});
// Whether a book has only one chapter, where "Jude 3" means its third verse
bool singleChapter(int book);
// How many chapters a book has in English Bibles (Joel 3, Malachi 4, as they number
// them; Hebrew numbering has one more for each), for a Book menu where no file says
int chapterCount(int book);

// ---- A reference

// A stretch of verses: from one chapter and verse to another. A verse of 0 is the
// chapter's first verse where it starts a stretch and its last where it ends one, so
// "Psalm 23" is chapter 23, verse 0, to chapter 23, verse 0.
struct Range
{
    int fromChapter = 0;
    int fromVerse = 0;
    int toChapter = 0;
    int toVerse = 0;

    bool operator==(const Range &other) const = default;
};

struct Reference
{
    int book = 0; // 0: none could be read
    QString bookName; // the name it was matched by, for a book outside the 66
    QList<Range> ranges;

    bool valid() const { return book > 0 && !ranges.isEmpty(); }
    bool operator==(const Reference &other) const = default;
};

// Reads what someone types for a passage. The book, by name or a short form of it, and
// then chapters and verses in any of the ways people write them:
//   "John 3:16", "John 3.16", "John 3 16"      a verse
//   "John 3:16-18", "John 3 16 18", "Jn 3:16–18" verses 16 to 18
//   "Matt 1 2 3"                                Matthew 1:2-3, as ProPresenter reads it
//   "Psalm 23", "Ps 23"                         the whole chapter
//   "Gen 1:1-2:3"                               across a chapter
//   "Ruth 1-2"                                  whole chapters
//   "John 3:16, 18, 20-21"                      several stretches of one chapter
//   "Jude 3"                                    a book of one chapter: its verse
// A reference that cannot be read has no book.
Reference parse(const QString &typed, const QList<Book> &own = {});

// A reference written out, as it goes on a slide: "John 3:16–18", "Psalm 23",
// "Genesis 1:1–2:3", "John 3:16, 18". `bookName` is the name to give the book (a
// Bible's own, say); the English one if it is empty.
QString written(const Reference &reference, const QString &bookName = {});

// ---- A passage of a Bible

// One verse of a passage, where it is from
struct PassageVerse
{
    int book = 0;
    int chapter = 0;
    Verse verse;
};

// The verses of a Bible a reference names, in order; ends of a stretch that lie past
// the end of a chapter or a book are taken to mean its end. Empty, with *error set, if
// the Bible has none of them.
QList<PassageVerse> passage(const Bible &bible, const Reference &reference, QString *error);

// The reference to just these verses (each slide's own, where a slide says which
// verses it holds): "John 3:16", "John 3:16–17", "Genesis 1:31–2:1".
Reference referenceTo(const QList<PassageVerse> &verses);

// The verse after or before the last or first of these, in the Bible, across chapters
// and books; a reference with no book where there is none (Bible mode's ← Verse and
// Verse →).
Reference nextVerse(const Bible &bible, const QList<PassageVerse> &verses);
Reference previousVerse(const Bible &bible, const QList<PassageVerse> &verses);

}
