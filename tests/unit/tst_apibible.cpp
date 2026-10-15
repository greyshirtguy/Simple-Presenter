// What is asked of api.bible and how its answers are read (src/apibible.h), on answers
// made up for the test in the shape the service gives them (its passages as JSON, a
// verse's words marked by verse id, the words of Jesus in a "wj" span). No Bible's
// words are in this repository, and nothing here goes on the network.

#include "apibible.h"

#include <QTest>
#include <QUrlQuery>

using namespace apibible;

namespace {

bible::Reference ref(int book, QList<bible::Range> ranges)
{
    bible::Reference r;
    r.book = book;
    r.ranges = ranges;
    return r;
}

const QByteArray passageAnswer = R"({
  "data": {
    "id": "JHN.3.16-JHN.3.17", "bibleId": "made-up", "reference": "John 3:16-17", "verseCount": 2,
    "copyright": "<p>Made up for a test. Not a real Bible.</p>",
    "content": [
      {"name": "para", "type": "tag", "attrs": {"style": "s1"}, "items": [{"text": "A heading, not Scripture", "type": "text"}]},
      {"name": "para", "type": "tag", "attrs": {"style": "p"}, "items": [
        {"name": "verse", "type": "tag", "attrs": {"number": "16", "style": "v", "sid": "JHN 3:16"}, "items": [{"text": "16", "type": "text"}]},
        {"text": "The first made-up verse, ", "type": "text", "attrs": {"verseId": "JHN.3.16", "verseOrgIds": ["JHN.3.16"]}},
        {"name": "char", "type": "tag", "attrs": {"style": "wj"}, "items": [{"text": "said in red ", "type": "text", "attrs": {"verseId": "JHN.3.16"}}]},
        {"text": "and done.", "type": "text", "attrs": {"verseId": "JHN.3.16"}},
        {"name": "note", "type": "tag", "attrs": {"style": "f"}, "items": [{"text": "a note", "type": "text"}]}
      ]},
      {"name": "para", "type": "tag", "attrs": {"style": "q1"}, "items": [
        {"name": "verse", "type": "tag", "attrs": {"number": "17", "style": "v"}, "items": [{"text": "17", "type": "text"}]},
        {"text": "A line of poetry", "type": "text", "attrs": {"verseId": "JHN.3.17"}}
      ]},
      {"name": "para", "type": "tag", "attrs": {"style": "q2"}, "items": [
        {"text": "and its second line.", "type": "text", "attrs": {"verseId": "JHN.3.17"}}
      ]}
    ]
  },
  "meta": {"fumsToken": "a-made-up-token"}
})";

}

class TestApiBible : public QObject
{
    Q_OBJECT

private slots:
    void theBiblesAKeyGives()
    {
        const QByteArray answer = R"({"data": [
            {"id": "b2", "name": "Second Made Up", "abbreviation": "SMU", "abbreviationLocal": "SMU", "language": {"name": "English"}},
            {"id": "b1", "name": "First Made Up", "abbreviation": "FMU", "abbreviationLocal": "", "language": {"name": "English"}, "description": "A test"},
            {"id": "b3", "name": "Andere", "abbreviation": "AND", "language": {"name": "German"}},
            {"name": "No id, left out"}
        ]})";
        QString error;
        const QList<Translation> all = readBibles(answer, &error);
        QCOMPARE(all.size(), 3);
        QCOMPARE(all[0].id, "b1");
        QCOMPARE(all[0].abbreviation, "FMU");
        QCOMPARE(all[0].description, "A test");
        QCOMPARE(all[1].id, "b2");
        QCOMPARE(all[2].language, "German");
        QVERIFY(readBibles("{\"statusCode\": 401}", &error).isEmpty() && !error.isEmpty());
    }

    void whatIsAskedForAReference()
    {
        QCOMPARE(passageIds(ref(43, {{3, 16, 3, 18}})), QStringList({"JHN.3.16-JHN.3.18"}));
        QCOMPARE(passageIds(ref(43, {{3, 16, 3, 16}})), QStringList({"JHN.3.16"}));
        QCOMPARE(passageIds(ref(19, {{23, 0, 23, 0}})), QStringList({"PSA.23"}));
        // Across chapters, and whole chapters: a chapter at a time
        QCOMPARE(passageIds(ref(1, {{1, 31, 2, 3}})), QStringList({"GEN.1", "GEN.2"}));
        QCOMPARE(passageIds(ref(8, {{1, 0, 2, 0}})), QStringList({"RUT.1", "RUT.2"}));
        QCOMPARE(passageIds(ref(43, {{3, 16, 3, 16}, {3, 18, 3, 18}})), QStringList({"JHN.3.16", "JHN.3.18"}));
        const QUrl url = passageUrl("made-up", "JHN.3.16");
        QCOMPARE(url.path(), "/v1/bibles/made-up/passages/JHN.3.16");
        QCOMPARE(url.host(), "rest.api.bible");
        const QUrlQuery query(url);
        QCOMPARE(query.queryItemValue("content-type"), "json");
        QCOMPARE(query.queryItemValue("include-titles"), "false");
        QCOMPARE(query.queryItemValue("fums-version"), "3");
    }

    void aPassageReadFromItsAnswer()
    {
        Passage passage;
        QString error;
        QVERIFY2(readPassage(passageAnswer, &passage, &error), qPrintable(error));
        QCOMPARE(passage.copyright, "Made up for a test. Not a real Bible.");
        QCOMPARE(passage.fumsToken, "a-made-up-token");
        QCOMPARE(passage.verses.size(), 2);
        const bible::PassageVerse &first = passage.verses[0];
        QVERIFY(first.book == 43 && first.chapter == 3 && first.verse.number == 16);
        // No heading, no verse number, no note
        QCOMPARE(first.verse.text, "The first made-up verse, said in red and done.");
        QCOMPARE(first.verse.red.size(), 1);
        QCOMPARE(first.verse.text.mid(first.verse.red[0].first, first.verse.red[0].second), "said in red");
        // The lines of a verse of poetry, one space apart
        QCOMPARE(passage.verses[1].verse.text, "A line of poetry and its second line.");
        QVERIFY(!readPassage("{}", &passage, &error) && !error.isEmpty());
    }

    void theVersesWantedOfChaptersAskedForWhole()
    {
        Passage one;
        for (int verse = 29; verse <= 31; ++verse)
            one.verses.append({1, 1, {verse, QStringLiteral("v%1").arg(verse), {}}});
        Passage two;
        for (int verse = 1; verse <= 4; ++verse)
            two.verses.append({1, 2, {verse, QStringLiteral("w%1").arg(verse), {}}});
        const QList<bible::PassageVerse> wanted = versesOf({one, two}, ref(1, {{1, 31, 2, 3}}));
        QStringList texts;
        for (const auto &v : wanted)
            texts << v.verse.text;
        QCOMPARE(texts, QStringList({"v31", "w1", "w2", "w3"}));
        // Each once, though asked for twice
        QCOMPARE(versesOf({one, one}, ref(1, {{1, 0, 1, 0}})).size(), 3);
    }

    void errorsSaidPlainly()
    {
        QCOMPARE(errorOf(401, R"({"statusCode":401,"error":"Unauthorized","message":"Invalid API key"})"),
                 "api.bible did not take the key. It said: Invalid API key");
        QVERIFY(errorOf(429, "").startsWith("This key has had"));
        QCOMPARE(errorOf(0, ""), "api.bible could not be reached.");
    }

    void fums()
    {
        const QUrlQuery query(fumsUrl({"one", "two"}, "device", "session"));
        QCOMPARE(query.allQueryItemValues("t"), QStringList({"one", "two"}));
        QCOMPARE(query.queryItemValue("dId"), "device");
        QCOMPARE(query.queryItemValue("sId"), "session");
    }
};

QTEST_APPLESS_MAIN(TestApiBible)
#include "tst_apibible.moc"
