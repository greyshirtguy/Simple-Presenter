// The rows of a playlist of presentations changed as the extended API asks
// (src/playlistrows.h), on a playlist laid out as ProPresenter writes one, with a field
// it knows nothing of and a row of a kind this app does not open: a heading made as
// ProPresenter writes one and changed; and a playlist given a whole list of rows, the
// rows that stay keeping their ids and everything else of them, to the byte.

#include "playlistrows.h"

#include "playlist.pb.h"

#include <QHash>
#include <QTest>

#include <google/protobuf/unknown_field_set.h>

using playlistrows::Wanted;

namespace {

// Something ProPresenter wrote that this app knows nothing of
void addUnknown(google::protobuf::Message *message, int number, const std::string &value)
{
    message->GetReflection()->MutableUnknownFields(message)->AddLengthDelimited(number, value);
}

rv::data::PlaylistItem headerRow(const std::string &id, const std::string &name, float red, float green, float blue)
{
    rv::data::PlaylistItem item;
    item.mutable_uuid()->set_string(id);
    item.set_name(name);
    rv::data::Color *color = item.mutable_header()->mutable_color();
    color->set_red(red);
    color->set_green(green);
    color->set_blue(blue);
    color->set_alpha(1);
    return item;
}

rv::data::PlaylistItem presentationRow(const std::string &id, const std::string &name)
{
    rv::data::PlaylistItem item;
    item.mutable_uuid()->set_string(id);
    item.set_name(name);
    item.mutable_presentation()->mutable_document_path()->set_absolute_string("file:///ws/Libraries/Songs/" + name + ".pro");
    return item;
}

// A playlist as ProPresenter might have left one: a heading, two songs (one shown in an
// arrangement, with something of a ProPresenter to come on its row), a row of a kind
// this app does not open, a hidden row, and the first song again
rv::data::Playlist service()
{
    rv::data::Playlist playlist;
    playlist.mutable_uuid()->set_string("PLAYLIST");
    playlist.set_name("Sunday");
    auto *items = playlist.mutable_items()->mutable_items();
    *items->Add() = headerRow("H1", "Worship", 1, 0.5764f, 0);
    *items->Add() = presentationRow("P1", "Amazing Grace");
    rv::data::PlaylistItem second = presentationRow("P2", "How Great");
    second.mutable_presentation()->mutable_arrangement()->set_string("ARR-1");
    second.mutable_presentation()->set_arrangement_name("Short");
    addUnknown(&second, 77, "of a ProPresenter to come");
    *items->Add() = second;
    rv::data::PlaylistItem placeholder;
    placeholder.mutable_uuid()->set_string("X1");
    placeholder.set_name("Offering");
    placeholder.mutable_placeholder();
    *items->Add() = placeholder;
    rv::data::PlaylistItem hidden = presentationRow("P3", "Hidden Song");
    hidden.set_is_hidden(true);
    *items->Add() = hidden;
    *items->Add() = presentationRow("P4", "Amazing Grace");
    return playlist;
}

// The file each row of service() is on this computer
const QStringList kFiles {QString(), QStringLiteral("/ws/Libraries/Songs/Amazing Grace.pro"), QStringLiteral("/ws/Libraries/Songs/How Great.pro"), QString(),
                          QStringLiteral("/ws/Libraries/Songs/Hidden Song.pro"), QStringLiteral("/ws/Libraries/Songs/Amazing Grace.pro")};

Wanted row(const QString &id)
{
    Wanted wanted;
    wanted.kind = Wanted::Row;
    wanted.id = id;
    return wanted;
}

Wanted heading(const QString &name, const QString &color = QString())
{
    Wanted wanted;
    wanted.kind = Wanted::Header;
    wanted.name = name;
    wanted.color = color;
    return wanted;
}

Wanted song(const QString &file)
{
    Wanted wanted;
    wanted.kind = Wanted::Presentation;
    wanted.file = file;
    return wanted;
}

QStringList idsOf(const rv::data::Playlist &playlist)
{
    QStringList ids;
    for (const rv::data::PlaylistItem &item : playlist.items().items())
        ids.append(QString::fromStdString(item.uuid().string()));
    return ids;
}

}

class TestPlaylistRows : public QObject
{
    Q_OBJECT

    int m_made = 0;

    QString set(rv::data::Playlist *playlist, const QList<Wanted> &wanted, QStringList *ids = nullptr)
    {
        return playlistrows::set(playlist, wanted, kFiles, [](const QString &file) {
            return presentationRow("NEW:" + file.section(u'/', -1).toStdString(), file.section(u'/', -1).section(u'.', 0, 0).toStdString());
        }, [this] { return "MADE-" + std::to_string(++m_made); }, ids);
    }

private slots:
    void init() { m_made = 0; }

    // ---- a heading

    void aHeadingIsMadeAsProPresenterWritesOne()
    {
        rv::data::PlaylistItem item;
        QVERIFY(playlistrows::makeHeader(&item, QStringLiteral("Worship"), QStringLiteral("#ff9300"), "H9"));
        // The id, the words as its name, and a header holding the colour with its alpha at one
        rv::data::PlaylistItem expected;
        expected.mutable_uuid()->set_string("H9");
        expected.set_name("Worship");
        rv::data::Color *color = expected.mutable_header()->mutable_color();
        color->set_red(1);
        color->set_green(float(0x93 / 255.0));
        color->set_alpha(1);
        QCOMPARE(item.SerializeAsString(), expected.SerializeAsString());

        // With no colour: a header with none in it
        rv::data::PlaylistItem plain;
        QVERIFY(playlistrows::makeHeader(&plain, QStringLiteral("Message"), QString(), "H8"));
        QVERIFY(plain.has_header());
        QVERIFY(!plain.header().has_color());
        // What is no colour is refused
        rv::data::PlaylistItem wrong;
        QVERIFY(!playlistrows::makeHeader(&wrong, QStringLiteral("x"), QStringLiteral("orange-ish"), "H7"));
        QVERIFY(!playlistrows::makeHeader(&wrong, QStringLiteral("x"), QStringLiteral("#ff93"), "H7"));
    }

    void aHeadingIsRenamedAndRecolouredAndNothingElseOfItTouched()
    {
        rv::data::PlaylistItem item = headerRow("H1", "Worship", 1, 0.5764f, 0);
        addUnknown(&item, 77, "kept");
        addUnknown(item.mutable_header(), 78, "kept too");
        const std::string before = item.SerializeAsString();

        // As it is already: not written again (the colour would come out in other digits)
        const QString same = QStringLiteral("Worship");
        const QString sameColor = QStringLiteral("#ff9300");
        QVERIFY(playlistrows::changeHeader(&item, &same, &sameColor).isEmpty());
        QCOMPARE(item.SerializeAsString(), before);

        const QString name = QStringLiteral("Praise");
        QVERIFY(playlistrows::changeHeader(&item, &name, nullptr).isEmpty());
        QCOMPARE(item.name(), std::string("Praise"));
        const QString blue = QStringLiteral("#0000ff");
        QVERIFY(playlistrows::changeHeader(&item, nullptr, &blue).isEmpty());
        QCOMPARE(item.header().color().blue(), 1.0f);
        QCOMPARE(item.header().color().red(), 0.0f);
        // What it knows nothing of is still there, on the row and in its header
        QCOMPARE(item.GetReflection()->GetUnknownFields(item).field_count(), 1);
        QCOMPARE(item.header().GetReflection()->GetUnknownFields(item.header()).field_count(), 1);
        // No colour takes it away
        const QString none;
        QVERIFY(playlistrows::changeHeader(&item, nullptr, &none).isEmpty());
        QVERIFY(!item.header().has_color());

        const QString wrong = QStringLiteral("blueish");
        QVERIFY(!playlistrows::changeHeader(&item, nullptr, &wrong).isEmpty());
        // A row that is not a heading has no words or colour of its own
        rv::data::PlaylistItem song = presentationRow("P1", "Song");
        const std::string songBefore = song.SerializeAsString();
        QVERIFY(!playlistrows::changeHeader(&song, &name, nullptr).isEmpty());
        QCOMPARE(song.SerializeAsString(), songBefore);
    }

    // ---- a whole list of rows

    void aPlaylistGivenTheRowsItHasIsAsItWas()
    {
        rv::data::Playlist playlist = service();
        QHash<QString, std::string> before;
        for (const rv::data::PlaylistItem &item : playlist.items().items())
            before.insert(QString::fromStdString(item.uuid().string()), item.SerializeAsString());
        const std::string whole = playlist.SerializeAsString();
        // By their ids: the playlist is as it was, to the byte, the hidden row where it was
        QVERIFY(set(&playlist, {row("H1"), row("P1"), row("P2"), row("X1"), row("P4")}).isEmpty());
        QCOMPARE(idsOf(playlist), QStringList({"H1", "P1", "P2", "X1", "P3", "P4"}));
        QCOMPARE(playlist.SerializeAsString(), whole);
        // And by what they are (the row this app cannot open, by its id)
        QVERIFY(set(&playlist, {heading("Worship"), song(QStringLiteral("/ws/Libraries/Songs/Amazing Grace.pro")),
                                song(QStringLiteral("/ws/Libraries/Songs/How Great.pro")), row("X1"),
                                song(QStringLiteral("/ws/Libraries/Songs/Amazing Grace.pro"))}).isEmpty());
        QCOMPARE(playlist.SerializeAsString(), whole);
        // In another order, the hidden row goes with the row it came after
        QVERIFY(set(&playlist, {row("X1"), row("H1"), row("P1"), row("P2"), row("P4")}).isEmpty());
        QCOMPARE(idsOf(playlist), QStringList({"X1", "P3", "H1", "P1", "P2", "P4"}));
        for (const rv::data::PlaylistItem &item : playlist.items().items())
            QCOMPARE(item.SerializeAsString(), before.value(QString::fromStdString(item.uuid().string())));
        QCOMPARE(m_made, 0);
    }

    void rowsThatStayKeepTheirIdsAndAllElseOfThem()
    {
        rv::data::Playlist playlist = service();
        const std::string worship = playlist.items().items(0).SerializeAsString();
        const std::string second = playlist.items().items(2).SerializeAsString();
        QStringList ids;
        // The same things asked for by what they are, in another order, with one new of each
        QVERIFY(set(&playlist, {heading("Welcome"), song(QStringLiteral("/ws/Libraries/Songs/New Song.pro")), heading("Worship"),
                                song(QStringLiteral("/ws/Libraries/Songs/How Great.pro")), song(QStringLiteral("/ws/Libraries/Songs/Amazing Grace.pro"))},
                    &ids).isEmpty());
        QCOMPARE(ids, QStringList({"MADE-1", "NEW:New Song.pro", "H1", "P2", "P1"}));
        // The heading and the song that stayed are as they were, to the byte: the
        // arrangement, and what a ProPresenter to come had put on the row
        QCOMPARE(playlist.items().items(2).SerializeAsString(), worship);
        QCOMPARE(playlist.items().items(3).SerializeAsString(), second);
        // What was not asked for is gone (the row this app cannot open, and the song's
        // second coming up), but for the hidden row, which is kept last
        // (which came after the row this app cannot open, and so goes after the rest)
        QCOMPARE(idsOf(playlist), QStringList({"MADE-1", "NEW:New Song.pro", "H1", "P2", "P1", "P3"}));
        // A new heading is one as ProPresenter writes it, with no colour said
        QCOMPARE(playlist.items().items(0).name(), std::string("Welcome"));
        QVERIFY(playlist.items().items(0).has_header());
        QVERIFY(!playlist.items().items(0).header().has_color());
    }

    void aPresentationAskedForTwiceIsTwoRows()
    {
        rv::data::Playlist playlist = service();
        QStringList ids;
        const QString grace = QStringLiteral("/ws/Libraries/Songs/Amazing Grace.pro");
        QVERIFY(set(&playlist, {song(grace), song(grace), song(grace)}, &ids).isEmpty());
        // The two rows there were, in their order, and a third made
        QCOMPARE(ids, QStringList({"P1", "P4", "NEW:Amazing Grace.pro"}));
    }

    void aRowNamedByItsIdIsNotTakenByWhatLooksLikeIt()
    {
        rv::data::Playlist playlist = service();
        QStringList ids;
        const QString grace = QStringLiteral("/ws/Libraries/Songs/Amazing Grace.pro");
        // The song asked for by what it is, then its first row by id: the id's row is
        // the id's, and the song asked for takes the other
        QVERIFY(set(&playlist, {song(grace), row("P1")}, &ids).isEmpty());
        QCOMPARE(ids, QStringList({"P4", "P1"}));
        // A row this app cannot open is kept by naming it
        playlist = service();
        QVERIFY(set(&playlist, {row("X1"), heading("Worship")}, &ids).isEmpty());
        QCOMPARE(ids, QStringList({"X1", "H1"}));
        QVERIFY(playlist.items().items(0).has_placeholder());
    }

    void aHeadingKeptTakesTheColourAskedAndAPresentationTheArrangement()
    {
        rv::data::Playlist playlist = service();
        Wanted great = song(QStringLiteral("/ws/Libraries/Songs/How Great.pro"));
        great.ownArrangement = false;
        great.arrangementId = QStringLiteral("ARR-2");
        great.arrangementName = QStringLiteral("Long");
        Wanted grace = song(QStringLiteral("/ws/Libraries/Songs/Amazing Grace.pro"));
        grace.ownArrangement = false;
        QVERIFY(set(&playlist, {heading("Worship", QStringLiteral("#00ff00")), great, grace}).isEmpty());
        QCOMPARE(playlist.items().items(0).header().color().green(), 1.0f);
        QCOMPARE(playlist.items().items(0).header().color().red(), 0.0f);
        QCOMPARE(playlist.items().items(1).presentation().arrangement().string(), std::string("ARR-2"));
        QCOMPARE(playlist.items().items(1).presentation().arrangement_name(), std::string("Long"));
        // (And what else the row had is still on it)
        QCOMPARE(playlist.items().items(1).GetReflection()->GetUnknownFields(playlist.items().items(1)).field_count(), 1);
        // No arrangement asked for outright: every group in order, which is none written
        QVERIFY(!playlist.items().items(2).presentation().has_arrangement());

        // And one put back to none where it had one
        Wanted plain = song(QStringLiteral("/ws/Libraries/Songs/How Great.pro"));
        plain.ownArrangement = false;
        QVERIFY(set(&playlist, {plain}).isEmpty());
        QVERIFY(!playlist.items().items(0).presentation().has_arrangement());
        QVERIFY(playlist.items().items(0).presentation().arrangement_name().empty());
    }

    void whatCannotBeLeavesThePlaylistAsItWas()
    {
        rv::data::Playlist playlist = service();
        const std::string before = playlist.SerializeAsString();
        QVERIFY(!set(&playlist, {row("H1"), row("NOT-THERE")}).isEmpty());
        QCOMPARE(playlist.SerializeAsString(), before);
        QVERIFY(!set(&playlist, {heading("Worship", QStringLiteral("not a colour"))}).isEmpty());
        QCOMPARE(playlist.SerializeAsString(), before);
        // (A hidden row cannot be asked for: it is not shown, so nothing knows its id)
        QVERIFY(!set(&playlist, {row("P3")}).isEmpty());
        QCOMPARE(playlist.SerializeAsString(), before);
    }

    void rowsAreAddedUnderARowOrAtTheEndAndTheOthersStayWhereTheyAre()
    {
        rv::data::Playlist playlist = service();
        QHash<QString, std::string> before;
        for (const rv::data::PlaylistItem &item : playlist.items().items())
            before.insert(QString::fromStdString(item.uuid().string()), item.SerializeAsString());
        const auto add = [this](rv::data::Playlist *to, const QList<Wanted> &wanted, const QString &after, QStringList *ids) {
            return playlistrows::insert(to, wanted, after, [](const QString &file) {
                return presentationRow("NEW:" + file.section(u'/', -1).toStdString(), file.section(u'/', -1).section(u'.', 0, 0).toStdString());
            }, [this] { return "MADE-" + std::to_string(++m_made); }, ids);
        };
        QStringList ids;
        // Under the first song: a heading and a song that is in the playlist already, as rows of their own
        Wanted grace = song(QStringLiteral("/ws/Libraries/Songs/Amazing Grace.pro"));
        grace.ownArrangement = false;
        grace.arrangementId = QStringLiteral("ARR-9");
        grace.arrangementName = QStringLiteral("Reprise");
        QVERIFY(add(&playlist, {heading("Response", QStringLiteral("#0000ff")), grace}, QStringLiteral("P1"), &ids).isEmpty());
        QCOMPARE(ids, QStringList({"MADE-1", "NEW:Amazing Grace.pro"}));
        // The rows there were are where they were, the hidden one too, each whole
        QCOMPARE(idsOf(playlist), QStringList({"H1", "P1", "MADE-1", "NEW:Amazing Grace.pro", "P2", "X1", "P3", "P4"}));
        for (const rv::data::PlaylistItem &item : playlist.items().items()) {
            const QString id = QString::fromStdString(item.uuid().string());
            if (before.contains(id))
                QCOMPARE(item.SerializeAsString(), before.value(id));
        }
        QCOMPARE(playlist.items().items(2).header().color().blue(), 1.0f);
        QCOMPARE(playlist.items().items(3).presentation().arrangement_name(), std::string("Reprise"));
        // At the end
        QVERIFY(add(&playlist, {heading("Close")}, QString(), &ids).isEmpty());
        QCOMPARE(idsOf(playlist).last(), QStringLiteral("MADE-2"));

        // What cannot be adds nothing: a row to go under that is not there, a colour that
        // is none (though a row before it could be made), a row by its id
        const std::string whole = playlist.SerializeAsString();
        QVERIFY(!add(&playlist, {heading("x")}, QStringLiteral("NOT-THERE"), &ids).isEmpty());
        QVERIFY(!add(&playlist, {heading("fine"), heading("x", QStringLiteral("blueish"))}, QString(), &ids).isEmpty());
        QVERIFY(!add(&playlist, {row("P1")}, QString(), &ids).isEmpty());
        QCOMPARE(playlist.SerializeAsString(), whole);
    }

    void aPlaylistGivenNothingKeepsOnlyWhatIsHidden()
    {
        rv::data::Playlist playlist = service();
        QVERIFY(set(&playlist, {}).isEmpty());
        QCOMPARE(idsOf(playlist), QStringList({"P3"}));
        // And the playlist itself is the playlist still
        QCOMPARE(playlist.name(), std::string("Sunday"));
        QCOMPARE(playlist.uuid().string(), std::string("PLAYLIST"));
    }
};

QTEST_GUILESS_MAIN(TestPlaylistRows)
#include "tst_playlistrows.moc"
