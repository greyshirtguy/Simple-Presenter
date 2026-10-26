// What a screen sent out through a DeckLink card is sent as (src/decklinkformats.h): the
// formats offered, each with its size, its rate and the SDK's code for it; its pictures
// made straight from premultiplied; and the beat the screens are drawn on, together, by
// the app's clock or by a card's frames.

#include "decklinkformats.h"

#include <QTest>

using namespace decklinkformats;

namespace {

// Passes as decklink.cpp makes them, for a second in which every screen changes at
// every frame (a transition): when each is drawn, and in which pass
struct Drawn
{
    QList<int> passes;
    qint64 shortest = -1;
};
QList<Drawn> simulated(QList<Waiting> screens, qint64 from, qint64 until)
{
    QList<Drawn> drawn(screens.size());
    QList<qint64> last(screens.size(), -1);
    int pass = 0;
    for (qint64 now = qMax(from, passDue(screens)); now < until; now = qMax(now, passDue(screens)), ++pass) {
        for (int i = 0; i < screens.size(); ++i) {
            if (!drawnInPass(screens[i], now))
                continue;
            drawn[i].passes << pass;
            if (last[i] >= 0 && (drawn[i].shortest < 0 || now - last[i] < drawn[i].shortest))
                drawn[i].shortest = now - last[i];
            last[i] = now;
            screens[i].next = nextAfter(screens[i], now);
        }
    }
    return drawn;
}

// The same with the screens kept to a card's frames, read once at `from`: when each pass was
QList<qint64> simulatedOnCard(QList<Waiting> screens, const CardClock &card, qint64 lead, qint64 from, qint64 until)
{
    QList<qint64> passes;
    const auto kept = [&](qint64 now) {
        QList<Waiting> onIt;
        for (const Waiting &screen : screens)
            onIt << onCard(screen, card, lead, now);
        return onIt;
    };
    for (qint64 now = qMax(from, passDue(kept(from))); now < until; now = qMax(now, passDue(kept(now)))) {
        const QList<Waiting> onIt = kept(now);
        bool any = false;
        for (int i = 0; i < screens.size(); ++i) {
            if (!drawnInPass(onIt[i], now))
                continue;
            any = true;
            screens[i].next = nextAfter(onIt[i], now);
        }
        if (any)
            passes << now;
        else
            ++now;
    }
    return passes;
}

}

class TestDeckLinkFormats : public QObject
{
    Q_OBJECT

private slots:
    void theFormatsAsked()
    {
        QStringList names;
        for (const Format &format : formats())
            names << format.name;
        QCOMPARE(names.join(u' '), QStringLiteral("1080p24 1080p25 1080p29.97 1080p30 1080p50 1080p59.94 1080p60 720p50 720p59.94 720p60"));
    }

    void eachHasItsSizeRateAndCode()
    {
        const Format *drop = formatNamed(QStringLiteral("1080p29.97"));
        QVERIFY(drop);
        QCOMPARE(drop->width, 1920);
        QCOMPARE(drop->height, 1080);
        QCOMPARE(drop->rateNumerator, 30000);
        QCOMPARE(drop->rateDenominator, 1001);
        // The SDK's codes, as its header has them (bmdModeHD1080p2997, bmdModeHD720p50, bmdModeHD1080p24)
        QCOMPARE(drop->mode, 0x48703239u);
        QCOMPARE(formatNamed(QStringLiteral("720p50"))->mode, 0x68703530u);
        QCOMPARE(formatNamed(QStringLiteral("720p50"))->height, 720);
        QCOMPARE(formatNamed(QStringLiteral("1080p24"))->mode, 0x32347073u);
        QCOMPARE(formatNamed(QStringLiteral("1080p60"))->rateDenominator, 1);
        QVERIFY(!formatNamed(QStringLiteral("720p25")));
        QVERIFY(!formatNamed(QString()));
    }

    void straightFromPremultiplied()
    {
        // Blue, green, red, opacity: opaque stays, nothing stays nothing, half-seen red
        // at half strength becomes full red, a quarter-seen grey a quarter as bright
        // becomes that grey, and a colour drawing left a little over its opacity is white.
        uchar pixels[] = {10, 20, 30, 255, 0, 0, 0, 0, 0, 0, 128, 128, 16, 16, 16, 64, 70, 70, 70, 64};
        unpremultiplied(pixels, 5);
        const uchar wanted[] = {10, 20, 30, 255, 0, 0, 0, 0, 0, 0, 255, 128, 64, 64, 64, 64, 255, 255, 255, 64};
        for (int i = 0; i < 20; ++i)
            QCOMPARE(int(pixels[i]), int(wanted[i]));
    }

    void aFrameOfEachFormat()
    {
        QCOMPARE(period(*formatNamed(QStringLiteral("1080p29.97"))), qint64(33'366'666));
        QCOMPARE(period(*formatNamed(QStringLiteral("720p50"))), qint64(20'000'000));
        QCOMPARE(period(*formatNamed(QStringLiteral("1080p24"))), qint64(41'666'666));
        QCOMPARE(period(*formatNamed(QStringLiteral("1080p59.94"))), qint64(16'683'333));
    }

    void aPassIsDueWhenTheSoonestIs()
    {
        QCOMPARE(passDue({}), qint64(-1));
        QCOMPARE(passDue({{900, 20}, {500, 33}, {700, 20}}), qint64(500));
        QCOMPARE(passDue({{0, 20}}), qint64(0));
    }

    void aPassWaitsForAFrameToComeBack()
    {
        // The second's card still has its frame: the pass waits for it, half a frame at
        // most; once it has it, or for one not due with the first, it does not wait
        const qint64 frame = 20'000'000;
        const qint64 due = 1'000'000'000;
        QCOMPARE(passDue({{due, frame}, {due, frame, false}}), due + frame / 2);
        QCOMPARE(passDue({{due, frame}, {due, frame, true}}), due);
        QCOMPARE(passDue({{due, frame}, {due + frame, frame, false}}), due);
        // (and when it goes without it, it is not drawn)
        QVERIFY(!drawnInPass({due, frame, false}, due + frame / 2));
        // One with no frame free makes no pass due by itself, long due as it may be:
        // nothing is, until its card gives a frame back
        QCOMPARE(passDue({{due, frame, false}}), qint64(-1));
        QCOMPARE(passDue({{due, frame, false}, {due + frame, frame}}), due + frame + frame / 2);
    }

    void aPassTakesThoseWithinHalfAFrame()
    {
        const qint64 frame = 20'000'000;
        const qint64 now = 1'000'000'000;
        QVERIFY(drawnInPass({now, frame}, now));
        QVERIFY(drawnInPass({now - 5 * frame, frame}, now)); // long due
        QVERIFY(drawnInPass({now + frame / 2, frame}, now));
        QVERIFY(!drawnInPass({now + frame / 2 + 1, frame}, now));
        QVERIFY(!drawnInPass({now + frame, frame}, now));
    }

    void theNextOnTheBeatOrAFrameFromNow()
    {
        const qint64 frame = 20'000'000;
        const qint64 due = 1'000'000'000;
        QCOMPARE(nextAfter({due, frame}, due), due + frame);                   // on time
        QCOMPARE(nextAfter({due, frame}, due + 3'000'000), due + frame);       // a little late: keeps the beat
        QCOMPARE(nextAfter({due, frame}, due - 4'000'000), due + frame);       // a little early, in a pass with others
        QCOMPARE(nextAfter({due, frame}, due + frame + 1), due + 2 * frame + 1); // after a while of nothing
        QCOMPARE(nextAfter({0, frame}, due), due + frame);                     // the first
    }

    void screensOfOneFormatAreDrawnTogether()
    {
        // Three outputs at 29.97, still for a while, then a transition: drawn in the same
        // passes, a frame apart, from the first
        const qint64 frame = 33'366'666;
        const QList<Drawn> drawn = simulated({{0, frame}, {0, frame}, {0, frame}}, 10'000'000'000, 11'000'000'000);
        QCOMPARE(drawn[0].passes.size(), 30);
        QCOMPARE(drawn[1].passes, drawn[0].passes);
        QCOMPARE(drawn[2].passes, drawn[0].passes);
        QCOMPARE(drawn[0].shortest, frame);
    }

    void oneOutOfStepJoinsTheOthers()
    {
        // The third's card gave its frame back late, and it was drawn 12 ms after the
        // others: from the next pass on, it is drawn with them again
        const qint64 frame = 33'366'666;
        const qint64 start = 10'000'000'000;
        const QList<Drawn> drawn = simulated({{start, frame}, {start, frame}, {start + 12'000'000, frame}}, start, start + 1'000'000'000);
        QCOMPARE(drawn[2].passes, drawn[0].passes);
        QCOMPARE(drawn[0].passes.size(), 30);
    }

    void onTheCardsFrames()
    {
        // A card at 29.97 read 10 ms into one of its frames at 100 ms: its frames begin at
        // 90 ms, and every 33.37 ms; with 8 ms of lead, a screen is due 8 ms before each
        const qint64 ms = 1'000'000;
        const CardClock card {100 * ms, 10 * ms, 33'366'666};
        const qint64 lead = 8 * ms;
        const Waiting screen {0, card.period};
        QCOMPARE(onCard(screen, card, lead, 100 * ms).next, 90 * ms + card.period - lead);
        QVERIFY(onCard(screen, card, lead, 100 * ms).onCard);
        // A moment just gone (the pass woken 2 ms late) is still that one's
        QCOMPARE(onCard(screen, card, lead, 84 * ms).next, 82 * ms);
        // and one long gone is not: the next
        QCOMPARE(onCard(screen, card, lead, 86 * ms).next, 82 * ms + card.period);
        // Never twice in one of the card's frames: drawn at 82, next in its next frame
        Waiting drawn = onCard(screen, card, lead, 82 * ms);
        QVERIFY(drawnInPass(drawn, 82 * ms));
        QVERIFY(drawnInPass(drawn, 81 * ms));
        QVERIFY(!drawnInPass(drawn, 80 * ms));
        drawn.next = nextAfter(drawn, 82 * ms);
        QCOMPARE(onCard(drawn, card, lead, 83 * ms).next, 82 * ms + card.period);
        // A card that cannot be read leaves the screen as it was
        QCOMPARE(onCard(screen, CardClock {}, lead, 100 * ms).next, qint64(0));
        QVERIFY(!onCard(screen, CardClock {}, lead, 100 * ms).onCard);
    }

    void keptToACardThatIsNotTheComputersClock()
    {
        // A card whose frames are half a percent longer than the format says, and a second of
        // a transition on two screens of its rate: every pass the same lead before one of the
        // card's frames, a frame of the card's apart, none skipped
        const qint64 ms = 1'000'000;
        const qint64 format = 33'366'666;
        const CardClock card {0, 0, format + format / 200};
        const qint64 lead = 10 * ms;
        const QList<qint64> passes = simulatedOnCard({{0, format}, {0, format}}, card, lead, 1'000 * ms, 2'000 * ms);
        QVERIFY2(passes.size() == 29 || passes.size() == 30, qPrintable(QString::number(passes.size())));
        for (qint64 pass : passes)
            QCOMPARE((pass + lead) % card.period, qint64(0));
        for (qsizetype i = 1; i < passes.size(); ++i)
            QCOMPARE(passes[i] - passes[i - 1], card.period);
    }

    void keptToACardWithNoFrameFree()
    {
        // One kept to a card's frames whose card has not given a frame back is not waited
        // for: the pass stays on the card's beat
        const qint64 frame = 33'366'666;
        const qint64 due = 1'000'000'000;
        Waiting late {due, frame, false};
        late.onCard = true;
        Waiting other {due, frame};
        other.onCard = true;
        QCOMPARE(passDue({other, late}), due);
        QVERIFY(!drawnInPass(late, due));
    }

    void theLeadIsThePassAndAMargin()
    {
        const qint64 ms = 1'000'000;
        QCOMPARE(leadFor(2 * ms, 33'366'666), 10 * ms);
        QCOMPARE(leadFor(0, 33'366'666), 8 * ms);
        // no more than three quarters of a frame
        QCOMPARE(leadFor(30 * ms, 33'366'666), qint64(33'366'666) * 3 / 4);
        QCOMPARE(leadFor(10 * ms, 16'683'333), qint64(16'683'333) * 3 / 4);
    }

    void formatsOfDifferentRatesEachAtMostTheirOwn()
    {
        // One output at 50 and one at 29.97: the first at every one of its frames, the
        // second only in those passes, never oftener than its own frames come, and never
        // sooner than half a frame after the one before
        const qint64 fifty = 20'000'000;
        const qint64 drop = 33'366'666;
        const QList<Drawn> drawn = simulated({{0, fifty}, {0, drop}}, 10'000'000'000, 11'000'000'000);
        QCOMPARE(drawn[0].passes.size(), 50);
        QVERIFY2(drawn[1].passes.size() <= 30 && drawn[1].passes.size() >= 20, qPrintable(QString::number(drawn[1].passes.size())));
        QVERIFY(drawn[1].shortest >= drop / 2);
        for (int pass : drawn[1].passes)
            QVERIFY(drawn[0].passes.contains(pass));
    }
};

QTEST_APPLESS_MAIN(TestDeckLinkFormats)
#include "tst_decklinkformats.moc"
