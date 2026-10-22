// ProPresenter's Scrolling Text (src/textscroller.h): the two boxes of ProPresenter's that
// were looked at, read; each change of the inspector's, changing that and nothing else;
// a box switched to scrolling given ProPresenter's settings for a new one, or keeping its
// own; how fast a Speed is; and where the words are as they go, once and over and over.

#include "textscroller.h"

#include <google/protobuf/unknown_field_set.h>

#include <QTest>

using namespace textscroller;

namespace {

std::string encoded(const google::protobuf::Message &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

// The two as ProPresenter wrote them: going up, off the box to begin with, once; and going
// left, over and over. And a field the descriptions of ProPresenter's files have no name
// for, as a later version's might be.
Scroller up()
{
    Scroller scroller;
    scroller.set_should_scroll(true);
    scroller.set_scroll_rate(0.48800599700149921);
    scroller.set_repeat_distance(0.12345679012345678);
    scroller.set_scrolling_direction(Scroller::DIRECTION_UP);
    scroller.set_starts_off_screen(true);
    scroller.set_fade_left(0.10194902548725637);
    scroller.set_fade_right(0.10194902548725637);
    scroller.GetReflection()->MutableUnknownFields(&scroller)->AddVarint(900, 7);
    return scroller;
}

Scroller left()
{
    Scroller scroller;
    scroller.set_should_scroll(true);
    scroller.set_scroll_rate(0.15099187380497131);
    scroller.set_should_repeat(true);
    scroller.set_repeat_distance(0.044115013342192456);
    scroller.set_fade_left(0.0994263862332696);
    scroller.set_fade_right(0.0994263862332696);
    return scroller;
}

QList<double> rounded(const QList<double> &values)
{
    QList<double> out;
    for (double value : values)
        out << std::round(value * 1000) / 1000;
    return out;
}

} // namespace

class TestTextScroller : public QObject
{
    Q_OBJECT

private slots:
    void readAsProPresenterWroteThem()
    {
        const QVariantMap rising = describe(up());
        QCOMPARE(rising.value("direction").toString(), QStringLiteral("up"));
        QVERIFY(rising.value("offScreen").toBool() && !rising.value("repeat").toBool());
        QCOMPARE(rising.value("rate").toDouble(), 0.48800599700149921);
        QCOMPARE(rising.value("fadeLeft").toDouble(), 0.10194902548725637);
        const QVariantMap across = describe(left());
        QCOMPARE(across.value("direction").toString(), QStringLiteral("left"));
        QVERIFY(across.value("repeat").toBool() && !across.value("offScreen").toBool());
        QCOMPARE(across.value("distance").toDouble(), 0.044115013342192456);
    }

    void aChangeChangesItsFieldAndNothingElse()
    {
        const auto changed = [](const QVariantMap &changes, const std::function<void(Scroller *)> &expect) {
            Scroller scroller = up();
            Scroller wanted = up();
            QVERIFY(asks(changes));
            change(&scroller, changes);
            expect(&wanted);
            QCOMPARE(encoded(scroller), encoded(wanted));
        };
        changed({{"scrollDirection", "right"}}, [](Scroller *s) { s->set_scrolling_direction(Scroller::DIRECTION_RIGHT); });
        changed({{"scrollDirection", "left"}}, [](Scroller *s) { s->set_scrolling_direction(Scroller::DIRECTION_LEFT); });
        changed({{"scrollRate", 0.9}}, [](Scroller *s) { s->set_scroll_rate(0.9); });
        changed({{"scrollRate", 7}}, [](Scroller *s) { s->set_scroll_rate(1); });
        changed({{"scrollRepeat", true}}, [](Scroller *s) { s->set_should_repeat(true); });
        changed({{"scrollDistance", 0.3}}, [](Scroller *s) { s->set_repeat_distance(0.3); });
        changed({{"scrollOffScreen", false}}, [](Scroller *s) { s->set_starts_off_screen(false); });
        changed({{"scrollFadeLeft", 0.2}, {"scrollFadeRight", 0}}, [](Scroller *s) { s->set_fade_left(0.2); s->set_fade_right(0); });
        changed({{"scrollOn", false}}, [](Scroller *s) { s->set_should_scroll(false); });
        QVERIFY(!asks({{"x", 3}, {"fillOn", true}}));
    }

    void switchedOnABoxIsGivenProPresentersSettingsOrKeepsItsOwn()
    {
        Scroller none;
        change(&none, {{"scrollOn", true}});
        QVERIFY(none.should_scroll() && none.should_repeat());
        QCOMPARE(none.scroll_rate(), 0.5);
        QCOMPARE(none.repeat_distance(), 0.05);
        Scroller own = left();
        own.set_should_scroll(false);
        change(&own, {{"scrollOn", true}});
        QCOMPARE(encoded(own), encoded(left()));
    }

    void howFastASpeedIs()
    {
        QCOMPARE(speed(0), 20.0);
        QCOMPARE(speed(0.5), 100.0);
        QCOMPARE(speed(1), 500.0);
        QVERIFY(speed(0.15) < speed(0.49) && speed(-1) == speed(0) && speed(2) == speed(1));
    }

    void whereTheWordsAre()
    {
        // Once, starting off a box 1000 long, words 300 long: in at the far end, across,
        // and gone once they are past its start
        const double from = start(true, 1000);
        QCOMPARE(from, 1000.0);
        QVERIFY(positions(0, from, 300, 50, 1000, false, 10).isEmpty());
        QCOMPARE(positions(1, from, 300, 50, 1000, false, 10), QList<double> {999});
        QCOMPARE(positions(1200, from, 300, 50, 1000, false, 10), QList<double> {-200});
        QVERIFY(positions(1300, from, 300, 50, 1000, false, 10).isEmpty());
        // Where they are to begin with: at the start, and going from there
        QCOMPARE(start(false, 1000), 0.0);
        QCOMPARE(positions(100, 0, 300, 50, 1000, false, 10), QList<double> {-100});
        // Over and over, every 350: as many goings as there is room for, the first that
        // can be seen first
        QCOMPARE(rounded(positions(0, 0, 300, 50, 1000, true, 10)), (QList<double> {0, 350, 700}));
        QCOMPARE(rounded(positions(400, 0, 300, 50, 1000, true, 10)), (QList<double> {-50, 300, 650}));
        QCOMPARE(rounded(positions(10400, 0, 300, 50, 1000, true, 10)), (QList<double> {-250, 100, 450, 800}));
        QCOMPARE(positions(400, 0, 300, 50, 1000, true, 2).size(), 2);
        QVERIFY(positions(0, 0, 0, 50, 1000, true, 10).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestTextScroller)
#include "tst_textscroller.moc"
