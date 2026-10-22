// ProPresenter's copyright display (src/ccli.h): the workspace's settings read from a
// file made as ProPresenter writes one and changed there, with a look at what else is
// still as it was; a presentation's details; the line they make; and which slides show it.

#include "ccli.h"

#include <google/protobuf/unknown_field_set.h>

#include <QTest>

using namespace ccli;

namespace {

std::string encoded(const google::protobuf::Message &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

// The settings as ProPresenter 21.4 writes them (the maintainer's file, its template's
// text box cut to what matters here, the ids made up), and a field the descriptions of
// ProPresenter's files have no name for, as a later version's might be
rv::data::CCLIDocument proPresentersOwn()
{
    rv::data::CCLIDocument document;
    rv::data::Slide *slide = document.mutable_template_()->mutable_base_slide();
    slide->mutable_uuid()->set_string("4A4A4A4A-1111-2222-3333-444444444444");
    slide->mutable_size()->set_width(1920);
    slide->mutable_size()->set_height(1080);
    rv::data::Slide::Element *box = slide->add_elements();
    box->mutable_element()->mutable_uuid()->set_string("4B4B4B4B-1111-2222-3333-444444444444");
    box->mutable_element()->mutable_bounds()->mutable_origin()->set_x(845);
    box->mutable_element()->mutable_bounds()->mutable_size()->set_width(1054);
    box->set_info(3);
    box->add_data_links()->mutable_ccli_text();
    box->mutable_text_scroller()->set_scroll_rate(0.5);
    const std::pair<rv::data::CopyrightLayout::TokenType, const char *> tokens[] = {
        {rv::data::CopyrightLayout::Title, nullptr},     {rv::data::CopyrightLayout::Text, "\n"},
        {rv::data::CopyrightLayout::Artist, nullptr},    {rv::data::CopyrightLayout::Text, "\n"},
        {rv::data::CopyrightLayout::Publisher, nullptr}, {rv::data::CopyrightLayout::Text, "\302\251 "},
        {rv::data::CopyrightLayout::CopyrightYear, nullptr}, {rv::data::CopyrightLayout::Text, "\n"},
        {rv::data::CopyrightLayout::LicenseNumber, nullptr},
    };
    for (const auto &[type, text] : tokens) {
        auto *token = document.mutable_layout()->add_tokens();
        token->set_token_type(type);
        if (text)
            token->set_text(text);
    }
    document.GetReflection()->MutableUnknownFields(&document)->AddVarint(900, 7);
    return document;
}

QVariantMap made(const char *kind, const char *text = "")
{
    return {{"kind", kind}, {"text", text}};
}

QVariantMap song()
{
    return {{"title", "A Made Up Song"}, {"artist", "Someone"}, {"author", "Someone Else"}, {"publisher", "A Publisher"},
            {"year", 2001}, {"songNumber", 1234567}};
}

} // namespace

class TestCcli : public QObject
{
    Q_OBJECT

private slots:
    void proPresentersSettingsAreReadAsItWritesThem()
    {
        const QVariantMap settings = describe(proPresentersOwn());
        QCOMPARE(settings.value("enabled").toBool(), false);
        QCOMPARE(settings.value("display").toString(), QStringLiteral("first"));
        QVERIFY(settings.value("license").toString().isEmpty());
        QStringList kinds;
        for (const QVariant &token : settings.value("tokens").toList())
            kinds << token.toMap().value("kind").toString();
        QCOMPARE(kinds, QStringList({"title", "text", "artist", "text", "publisher", "text", "year", "text", "license"}));
        QCOMPARE(settings.value("tokens").toList().at(5).toMap().value("text").toString(), QStringLiteral("© "));
    }

    void settingsAreChangedWithTheRestLeftAlone()
    {
        rv::data::CCLIDocument document = proPresentersOwn();
        QVERIFY(change(&document, {{"enabled", true}, {"license", " 12345 "}, {"display", "firstAndLast"}}).isEmpty());
        QCOMPARE(document.enable_ccli_display(), true);
        QCOMPARE(document.ccli_license(), std::string("12345"));
        QCOMPARE(document.display_type(), rv::data::CCLIDocument::DISPLAY_TYPE_FIRST_AND_LAST_SLIDE);
        rv::data::CCLIDocument expected = proPresentersOwn();
        expected.set_enable_ccli_display(true);
        expected.set_ccli_license("12345");
        expected.set_display_type(rv::data::CCLIDocument::DISPLAY_TYPE_FIRST_AND_LAST_SLIDE);
        QCOMPARE(encoded(document), encoded(expected));
        // The same tokens again change nothing; others replace them
        QVERIFY(change(&document, {{"tokens", describe(proPresentersOwn()).value("tokens")}}).isEmpty());
        QCOMPARE(encoded(document), encoded(expected));
        QVERIFY(change(&document, {{"tokens", QVariantList {made("title"), made("text", " / "), made("songNumber")}}}).isEmpty());
        QCOMPARE(document.layout().tokens_size(), 3);
        QCOMPARE(document.layout().tokens(1).text(), std::string(" / "));
        QVERIFY(document.layout().tokens(2).text().empty());
        QCOMPARE(document.template_().base_slide().elements(0).element().uuid().string(), std::string("4B4B4B4B-1111-2222-3333-444444444444"));
        QVERIFY(!change(&document, {{"display", "sideways"}}).isEmpty());
        QVERIFY(!change(&document, {{"tokens", QVariantList {made("colour")}}}).isEmpty());
    }

    void aWorkspaceWithNoFileHasProPresentersLayout()
    {
        int ids = 0;
        const rv::data::CCLIDocument document = ccli::made([&ids] { return "ID-" + std::to_string(++ids); });
        QCOMPARE(describe(document).value("tokens"), describe(proPresentersOwn()).value("tokens"));
        QCOMPARE(document.enable_ccli_display(), false);
        QCOMPARE(document.template_().base_slide().uuid().string(), std::string("ID-1"));
        QCOMPARE(document.template_().base_slide().size().width(), 1920.0);
    }

    void aPresentationsDetailsAreReadAndChanged()
    {
        rv::data::Presentation::CCLI details;
        details.set_song_title("A Made Up Song");
        details.set_copyright_year(2001);
        details.set_display(true);
        QVariantMap read = describe(details);
        QCOMPARE(read.value("title").toString(), QStringLiteral("A Made Up Song"));
        QCOMPARE(read.value("year").toInt(), 2001);
        QCOMPARE(read.value("songNumber").toInt(), 0);
        QVERIFY(read.value("display").toBool());
        const std::string was = encoded(details);
        change(&details, {{"title", "A Made Up Song"}, {"year", 2001}});
        QCOMPARE(encoded(details), was);
        change(&details, {{"artist", "Someone"}, {"songNumber", 1234567}, {"display", false}});
        QCOMPARE(details.artist_credits(), std::string("Someone"));
        QCOMPARE(details.song_number(), 1234567u);
        QVERIFY(!details.display());
    }

    void theLineIsTheLayoutsTokensWithTheSongsDetails()
    {
        const QVariantList tokens = describe(proPresentersOwn()).value("tokens").toList();
        QCOMPARE(compose(tokens, song(), "998877"), QStringLiteral("A Made Up Song\nSomeone\nA Publisher© 2001\n998877"));
        // No artist: the line break after it goes with it
        QVariantMap noArtist = song();
        noArtist.remove("artist");
        QCOMPARE(compose(tokens, noArtist, "998877"), QStringLiteral("A Made Up Song\nA Publisher© 2001\n998877"));
        // No licence, nor year: nor what was between them and the rest
        QVariantMap bare = song();
        bare.remove("year");
        QCOMPARE(compose(tokens, bare, ""), QStringLiteral("A Made Up Song\nSomeone\nA Publisher"));
        // Words before the first detail are kept where it has something
        QCOMPARE(compose({made("text", "CCLI Song # "), made("songNumber")}, song(), {}), QStringLiteral("CCLI Song # 1234567"));
        QCOMPARE(compose({made("text", "CCLI Song # "), made("songNumber")}, {}, {}), QString());
        QCOMPARE(compose({made("text", "Words alone")}, {}, {}), QString());
        // No title: the line begins with the artist, not with the line break before it
        QVariantMap noTitle = song();
        noTitle.remove("title");
        QCOMPARE(compose(tokens, noTitle, "998877"), QStringLiteral("Someone\nA Publisher© 2001\n998877"));
        // No publisher: the year keeps its own words before it
        QVariantMap noPublisher = song();
        noPublisher.remove("publisher");
        QCOMPARE(compose(tokens, noPublisher, ""), QStringLiteral("A Made Up Song\nSomeone© 2001"));
    }

    void theSlidesThatShowIt()
    {
        QVERIFY(shownOn("first", 0, 5) && !shownOn("first", 4, 5));
        QVERIFY(shownOn("last", 4, 5) && !shownOn("last", 0, 5));
        QVERIFY(shownOn("firstAndLast", 0, 5) && shownOn("firstAndLast", 4, 5) && !shownOn("firstAndLast", 2, 5));
        QVERIFY(shownOn("all", 2, 5));
        QVERIFY(shownOn("first", 0, 1) && shownOn("last", 0, 1));
        QVERIFY(!shownOn("all", 5, 5) && !shownOn("all", -1, 5) && !shownOn("all", 0, 0));
    }
};

QTEST_GUILESS_MAIN(TestCcli)
#include "tst_ccli.moc"
