// A sweep of the screens' scripts (qml/) for what the brief says is not to be in them
// (AGENTS.md): Qt's own animations on anything that moves on a screen, which Qt steps by a
// timer of its own while two windows are showing, out of time with the screen and with a
// video playing on it; what moves on a screen moves by a FrameClock (src/frameclock.h). The
// files of what is drawn on an audience or a stage screen, and in the pictures of one, are
// looked through, their comments aside; the operator window's own are not, where Qt's
// animations are fine. (An idea taken from MxU Slides, whose unit tests read its own
// source for its rules.)

#include <QFile>
#include <QRegularExpression>
#include <QTest>

class TestSourceSweep : public QObject
{
    Q_OBJECT

private slots:
    void nothingOnAScreenMovesByQtsAnimations()
    {
        const QStringList screens = {
            QStringLiteral("OutputScene.qml"), QStringLiteral("TransitionLayer.qml"), QStringLiteral("PropsLayer.qml"),
            QStringLiteral("AnnouncementLayer.qml"), QStringLiteral("Slide.qml"), QStringLiteral("SlideElement.qml"),
            QStringLiteral("MediaContent.qml"), QStringLiteral("MediaLook.qml"), QStringLiteral("MediaEffects.qml"),
            QStringLiteral("MediaPicture.qml"), QStringLiteral("ScrollingText.qml"), QStringLiteral("BuildRun.qml"),
            QStringLiteral("TextFills.qml"), QStringLiteral("VideoInputView.qml"), QStringLiteral("FrameFade.qml"), QStringLiteral("LookGate.qml"),
            QStringLiteral("StageScene.qml"), QStringLiteral("StageView.qml"), QStringLiteral("ScreenPicture.qml"),
            QStringLiteral("ChordedText.qml"), QStringLiteral("Output.qml"), QStringLiteral("Stage.qml"),
            QStringLiteral("AuxWindow.qml")};
        const QRegularExpression animation(QStringLiteral(
            "\\b(NumberAnimation|FrameAnimation|PropertyAnimation|SequentialAnimation|ParallelAnimation|SmoothedAnimation|"
            "SpringAnimation|OpacityAnimator|XAnimator|YAnimator|ScaleAnimator|RotationAnimator|UniformAnimator)\\b|\\bBehavior on\\b"));
        const QRegularExpression blockComment(QStringLiteral("/\\*.*?\\*/"), QRegularExpression::DotMatchesEverythingOption);
        QStringList found;
        for (const QString &name : screens) {
            QFile file(QStringLiteral(SIMPLEPRESENTER_QML_DIR "/") + name);
            QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(name + QStringLiteral(" is not there")));
            QString text = QString::fromUtf8(file.readAll());
            text.remove(blockComment);
            const QStringList lines = text.split(u'\n');
            for (qsizetype i = 0; i < lines.size(); ++i) {
                const QString line = lines.at(i).section(QStringLiteral("//"), 0, 0);
                if (animation.match(line).hasMatch())
                    found << QStringLiteral("%1:%2: %3").arg(name).arg(i + 1).arg(line.trimmed());
            }
        }
        QVERIFY2(found.isEmpty(), qPrintable(found.join(QStringLiteral("\n"))));
    }
};

QTEST_GUILESS_MAIN(TestSourceSweep)
#include "tst_sourcesweep.moc"
