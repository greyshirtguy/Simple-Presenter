// Who decodes video, the graphics chip or the processor alone (src/decodingchoice.h):
// who has the say when the app starts (the command line, the setting, the environment
// as it was found, the guard), what the variable Qt reads is then set to, and what is
// said of it.

#include "decodingchoice.h"

#include <QTest>

using namespace decodingchoice;

namespace {

const QString kReason = QStringLiteral("the graphics card is run by the nouveau driver (its chip is NVE4), and the firmware that driver's video decoder needs is not installed");

// A computer the guard holds hardware decoding back on
Asked guarded()
{
    Asked asked;
    asked.guarded = true;
    asked.guardReason = kReason;
    return asked;
}

}

class TestDecodingChoice : public QObject
{
    Q_OBJECT

private slots:
    // ---- a computer the guard has nothing to say of

    void withNothingAskedQtIsLeftToChooseAndTheVariableIsNotTouched()
    {
        const Choice choice = choose(Asked());
        QVERIFY(!choice.softwareOnly && !choice.guarded && !choice.overridden);
        QCOMPARE(choice.by, By::Nobody);
        QVERIFY(choice.set.isNull());
        QCOMPARE(logLine(choice), "video is decoded by the graphics chip where its driver offers that, and otherwise by the processor");
    }

    void theSettingKeepsItToTheProcessor()
    {
        Asked asked;
        asked.mode = Mode::ProcessorOnly;
        const Choice choice = choose(asked);
        QVERIFY(choice.softwareOnly);
        QCOMPARE(choice.by, By::Settings);
        // The variable is given a list that names nothing, and is not left unset.
        QVERIFY(!choice.set.isNull() && !choice.set.isEmpty() && namesNone(choice.set));
        QCOMPARE(inWords(choice), "by the processor only, as the settings ask");
        QCOMPARE(logLine(choice), "video is decoded by the processor only, as the settings ask (hardware decoding is not tried)");
    }

    void theCommandLineDoesTooAndIsNamedBeforeTheSetting()
    {
        Asked asked;
        asked.softwareFlag = true;
        Choice choice = choose(asked);
        QVERIFY(choice.softwareOnly && namesNone(choice.set) && !choice.set.isNull());
        QCOMPARE(choice.by, By::CommandLine);
        asked.mode = Mode::ProcessorOnly;
        QCOMPARE(choose(asked).by, By::CommandLine);
        // It is the say of the command line even against the setting for the graphics chip.
        asked.mode = Mode::GraphicsChip;
        choice = choose(asked);
        QVERIFY(choice.softwareOnly && choice.by == By::CommandLine);
        QVERIFY(inWords(choice).contains("--software-decoding"));
    }

    void bothOfTheCommandLinesAtOnceIsTheCarefulOne()
    {
        Asked asked;
        asked.softwareFlag = true;
        asked.hardwareFlag = true;
        QVERIFY(choose(asked).softwareOnly);
    }

    void theCommandLineForTheGraphicsChipIsHeardBeforeTheSettingForTheProcessor()
    {
        Asked asked;
        asked.hardwareFlag = true;
        asked.mode = Mode::ProcessorOnly;
        const Choice choice = choose(asked);
        QVERIFY(!choice.softwareOnly && choice.set.isNull());
        QCOMPARE(choice.by, By::CommandLine);
    }

    void askedForTheProcessorItIsSoWhateverTheEnvironmentNames()
    {
        // A variable that names decoders to try does not undo the setting: with the
        // setting for the processor, no hardware decoder is tried.
        for (const bool flag : {true, false}) {
            Asked asked;
            asked.environmentSet = true;
            asked.environment = QByteArrayLiteral("vaapi,vdpau");
            asked.softwareFlag = flag;
            asked.mode = flag ? Mode::Automatic : Mode::ProcessorOnly;
            const Choice choice = choose(asked);
            QVERIFY(choice.softwareOnly);
            QVERIFY(!choice.set.isNull() && namesNone(choice.set));
            QVERIFY(choice.by == (flag ? By::CommandLine : By::Settings));
        }
    }

    void anEnvironmentThatNamesNoDecoderIsLeftAsItIsAndSaidToBeWhy()
    {
        for (const QByteArray &value : {QByteArray(""), QByteArrayLiteral(","), QByteArrayLiteral(" , ,")}) {
            Asked asked;
            asked.environmentSet = true;
            asked.environment = value;
            const Choice choice = choose(asked);
            QVERIFY(choice.softwareOnly);
            QCOMPARE(choice.by, By::Environment);
            QVERIFY(choice.set.isNull());
            QVERIFY(inWords(choice).contains("QT_FFMPEG_DECODING_HW_DEVICE_TYPES") && inWords(choice).contains("names no hardware decoder"));
            QVERIFY(logLine(choice).endsWith("(hardware decoding is not tried)"));
        }
    }

    void anEnvironmentThatNamesDecodersIsLeftAsItIsAndTheyAreNamed()
    {
        Asked asked;
        asked.environmentSet = true;
        asked.environment = QByteArrayLiteral("VAAPI, vdpau,");
        Choice choice = choose(asked);
        QVERIFY(!choice.softwareOnly);
        QCOMPARE(choice.by, By::Environment);
        QVERIFY(choice.set.isNull());
        QCOMPARE(choice.kept, "vaapi, vdpau");
        QVERIFY(logLine(choice).contains("kept to vaapi, vdpau by QT_FFMPEG_DECODING_HW_DEVICE_TYPES"));
        QVERIFY(!logLine(choice).contains("not tried"));
        // And so it is with the setting for the graphics chip, which asks for no more.
        asked.mode = Mode::GraphicsChip;
        choice = choose(asked);
        QVERIFY(!choice.softwareOnly && choice.set.isNull());
        QVERIFY(logLine(choice).contains("kept to vaapi, vdpau by QT_FFMPEG_DECODING_HW_DEVICE_TYPES"));
    }

    // ---- a computer the guard holds hardware decoding back on

    void withNothingAskedTheGuardHolds()
    {
        const Choice choice = choose(guarded());
        QVERIFY(choice.softwareOnly && choice.guarded && !choice.overridden);
        QCOMPARE(choice.by, By::Guard);
        QVERIFY(!choice.set.isNull() && namesNone(choice.set));
        QCOMPARE(inWords(choice), "by the processor only, because " + kReason);
        QCOMPARE(logLine(choice), "video is decoded by the processor only, because " + kReason + " (hardware decoding is not tried)");
    }

    void theSettingForTheGraphicsChipHasItTriedAllTheSame()
    {
        Asked asked = guarded();
        asked.mode = Mode::GraphicsChip;
        const Choice choice = choose(asked);
        QVERIFY(!choice.softwareOnly && choice.guarded && choice.overridden);
        QCOMPARE(choice.by, By::Settings);
        // Nothing is set: Qt is left to try what the machine has.
        QVERIFY(choice.set.isNull());
        QCOMPARE(logLine(choice), "video is decoded by the graphics chip where its driver offers that, and otherwise by the processor: "
                                  "the settings ask for that, although " + kReason);
    }

    void soDoesTheCommandLineForOneRun()
    {
        Asked asked = guarded();
        asked.hardwareFlag = true;
        const Choice choice = choose(asked);
        QVERIFY(!choice.softwareOnly && choice.overridden && choice.set.isNull());
        QCOMPARE(choice.by, By::CommandLine);
        QVERIFY(inWords(choice).contains("--hardware-decoding asks for that, although the graphics card is run by the nouveau driver"));
    }

    void theSettingForTheProcessorIsTheSettingsDoingAndNotTheGuards()
    {
        Asked asked = guarded();
        asked.mode = Mode::ProcessorOnly;
        const Choice choice = choose(asked);
        QVERIFY(choice.softwareOnly && choice.guarded && !choice.overridden);
        QCOMPARE(choice.by, By::Settings);
        QCOMPARE(inWords(choice), "by the processor only, as the settings ask");
    }

    void whoeverSetTheVariableThemselvesHasSaidWhatTheyWantAndTheGuardDoesNotHold()
    {
        Asked asked = guarded();
        asked.environmentSet = true;
        asked.environment = QByteArrayLiteral("vaapi");
        const Choice choice = choose(asked);
        QVERIFY(!choice.softwareOnly && choice.set.isNull());
        QCOMPARE(choice.by, By::Environment);
        // It is still a computer the guard would hold, which the settings can say.
        QVERIFY(choice.guarded && !choice.overridden);
    }

    void aSettingForTheGraphicsChipDoesNotUndoAVariableThatNamesNoDecoder()
    {
        Asked asked = guarded();
        asked.mode = Mode::GraphicsChip;
        asked.environmentSet = true;
        asked.environment = QByteArrayLiteral(",");
        const Choice choice = choose(asked);
        QVERIFY(choice.softwareOnly && !choice.overridden && choice.set.isNull());
        QVERIFY(inWords(choice).contains("names no hardware decoder"));
    }

    // ---- the setting as it is kept, and the rest

    void theSettingAsItIsKept()
    {
        QCOMPARE(modeOf("auto"), Mode::Automatic);
        QCOMPARE(modeOf("processor"), Mode::ProcessorOnly);
        QCOMPARE(modeOf("hardware"), Mode::GraphicsChip);
        // Nothing kept, and anything not understood, is automatic.
        QCOMPARE(modeOf(""), Mode::Automatic);
        QCOMPARE(modeOf("true"), Mode::Automatic);
        for (const Mode mode : {Mode::Automatic, Mode::ProcessorOnly, Mode::GraphicsChip})
            QCOMPARE(modeOf(nameOf(mode)), mode);
    }

    void whatNamesNone()
    {
        QVERIFY(namesNone(QByteArray()));
        QVERIFY(namesNone(""));
        QVERIFY(namesNone(","));
        QVERIFY(namesNone(" ,, "));
        QVERIFY(!namesNone("vaapi"));
        QVERIFY(!namesNone(",vdpau"));
        QCOMPARE(QByteArray(kVariable), "QT_FFMPEG_DECODING_HW_DEVICE_TYPES");
    }

    void theChoiceOfTheRunIsKept()
    {
        QVERIFY(!session().softwareOnly && session().by == By::Nobody);
        setSession(choose(guarded()));
        QVERIFY(session().softwareOnly && session().by == By::Guard && session().guardReason == kReason);
    }
};

QTEST_APPLESS_MAIN(TestDecodingChoice)
#include "tst_decodingchoice.moc"
