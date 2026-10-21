#include "videoinputfile.h"

#include "action.pb.h"
#include "proworkspace.pb.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

// Video inputs in ProPresenter's files (see videoinputfile.h): the list in the
// workspace's set-up file, read as ProPresenter writes it, an input added, renamed and
// removed with everything else in the file kept; and the Video Input Action and fill,
// read as ProPresenter wrote one and made new.
namespace {

using namespace videoinputfile;

// A workspace's set-up file as the maintainer's has it: a screen, a look of it, and
// ProPresenter's first input, with no device on this computer and no sound; and a field
// of the file this app knows nothing of (field 900).
QByteArray proWorkspace()
{
    rv::data::ProPresenterWorkspace document;
    rv::data::ProPresenterScreen *screen = document.add_pro_screens();
    screen->mutable_uuid()->set_string("SCREEN-1");
    screen->set_name("Main Projector");
    rv::data::ProAudienceLook *look = document.add_audience_looks();
    look->mutable_uuid()->set_string("LOOK-1");
    look->set_name("Everything");
    look->add_screen_looks()->set_live_video_enabled(true);
    rv::data::VideoInput *input = document.add_videoinputs();
    input->mutable_uuid()->set_string("6859FACB-8EAE-470D-A0F2-AE8BE864393B");
    input->set_user_description("Input 1");
    input->mutable_display_color()->set_red(0.0471f);
    input->mutable_display_color()->set_green(0.1725f);
    input->mutable_display_color()->set_blue(0.3529f);
    input->mutable_display_color()->set_alpha(1);
    input->set_audio_type(rv::data::VideoInput::AUDIO_DEVICE_TYPE_NONE);
    input->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
    std::string bytes = document.SerializeAsString();
    bytes += std::string("\xa0\x38\x07", 3);
    return QByteArray::fromStdString(bytes);
}

QString write(const QTemporaryDir &dir, const QByteArray &bytes)
{
    QDir().mkpath(dir.path() + "/Configuration");
    QFile file(dir.path() + "/Configuration/Workspace");
    if (!file.open(QIODevice::WriteOnly))
        qFatal("the test could not write %s", qPrintable(file.fileName()));
    file.write(bytes);
    return dir.path();
}

rv::data::ProPresenterWorkspace readBack(const QString &workspace, QByteArray *raw = nullptr)
{
    QFile file(workspace + "/Configuration/Workspace");
    if (!file.open(QIODevice::ReadOnly))
        qFatal("the test could not read %s", qPrintable(file.fileName()));
    const QByteArray bytes = file.readAll();
    if (raw)
        *raw = bytes;
    rv::data::ProPresenterWorkspace document;
    document.ParseFromArray(bytes.constData(), int(bytes.size()));
    return document;
}

// ProPresenter's own Video Input Action, from a presentation of the maintainer's: live
// video with its drawing, its sound routed nowhere on two of its channels (it has
// sixteen), an input it says nothing of (so the first), on the input layer.
rv::data::Action proAction()
{
    rv::data::Action action;
    action.mutable_uuid()->set_string("516CCF1F-BC5D-432E-AC51-CD03BE2C8D4D");
    action.set_isenabled(true);
    action.set_type(rv::data::Action::ACTION_TYPE_MEDIA);
    rv::data::Media *element = action.mutable_media()->mutable_element();
    element->mutable_uuid()->set_string("AEB6675B-1761-4C11-8FB9-9FEB4152EE6C");
    element->mutable_metadata();
    element->mutable_live_video()->mutable_drawing()->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
    element->mutable_live_video()->mutable_audio()->set_volume(1);
    element->mutable_live_video()->mutable_audio()->add_audio_channels()->add_outputs();
    element->mutable_live_video()->mutable_audio()->add_audio_channels()->set_index(1);
    element->mutable_live_video()->mutable_live_video();
    action.mutable_media()->mutable_audio();
    action.mutable_media()->set_layer_type(rv::data::Action::LAYER_TYPE_INPUT);
    return action;
}

} // namespace

class TestVideoInputFile : public QObject
{
    Q_OBJECT

private slots:
    void theListIsReadAsProPresenterWritesIt()
    {
        QTemporaryDir dir;
        const QString workspace = write(dir, proWorkspace());
        QString error;
        const QList<Input> list = inputs(workspace, &error);
        QCOMPARE(error, QString());
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().id, "6859FACB-8EAE-470D-A0F2-AE8BE864393B");
        QCOMPARE(list.first().name, "Input 1");
        QCOMPARE(list.first().color, "#0c2c5a");
        QCOMPARE(list.first().audioType, 1);
        QCOMPARE(list.first().proDevice, QString());
        // A workspace with no file has none, and that is no error
        QTemporaryDir empty;
        QVERIFY(inputs(empty.path(), &error).isEmpty());
        QCOMPARE(error, QString());
    }

    void anInputIsAddedLastWithAllElseKept()
    {
        QTemporaryDir dir;
        const QString workspace = write(dir, proWorkspace());
        QString id;
        QCOMPARE(add(workspace, "Camera", &id), QString());
        QCOMPARE(id.size(), 36);
        QByteArray raw;
        const rv::data::ProPresenterWorkspace document = readBack(workspace, &raw);
        QCOMPARE(document.videoinputs_size(), 2);
        const rv::data::VideoInput &made = document.videoinputs(1);
        QCOMPARE(made.user_description(), "Camera");
        QCOMPARE(QString::fromStdString(made.uuid().string()), id);
        QCOMPARE(made.alpha_type(), rv::data::ALPHA_TYPE_STRAIGHT);
        QVERIFY(made.has_display_color());
        QVERIFY(!made.has_video_input_device());
        // ProPresenter's first, the screen, the look and the field this app does not know,
        // as they were
        QCOMPARE(document.videoinputs(0).user_description(), "Input 1");
        QCOMPARE(document.videoinputs(0).audio_type(), rv::data::VideoInput::AUDIO_DEVICE_TYPE_NONE);
        QCOMPARE(document.pro_screens(0).name(), "Main Projector");
        QVERIFY(document.audience_looks(0).screen_looks(0).live_video_enabled());
        QVERIFY(raw.contains(QByteArray("\xa0\x38\x07", 3)));
    }

    void anInputIsRenamedAndRemovedAndNothingElse()
    {
        QTemporaryDir dir;
        const QString workspace = write(dir, proWorkspace());
        QString id;
        add(workspace, "Camera", &id);
        QCOMPARE(rename(workspace, "6859FACB-8EAE-470D-A0F2-AE8BE864393B", "Pulpit"), QString());
        QCOMPARE(inputs(workspace, nullptr).first().name, "Pulpit");
        // The same name again writes nothing
        QFile file(workspace + "/Configuration/Workspace");
        const QDateTime before = QFileInfo(file).lastModified();
        QTest::qWait(20);
        QCOMPARE(rename(workspace, "6859FACB-8EAE-470D-A0F2-AE8BE864393B", "Pulpit"), QString());
        QCOMPARE(QFileInfo(file).lastModified(), before);
        QCOMPARE(remove(workspace, "6859FACB-8EAE-470D-A0F2-AE8BE864393B"), QString());
        QByteArray raw;
        const rv::data::ProPresenterWorkspace document = readBack(workspace, &raw);
        QCOMPARE(document.videoinputs_size(), 1);
        QCOMPARE(document.videoinputs(0).user_description(), "Camera");
        QCOMPARE(document.pro_screens_size(), 1);
        QVERIFY(raw.contains(QByteArray("\xa0\x38\x07", 3)));
        QVERIFY(!remove(workspace, "no such input").isEmpty());
        QVERIFY(!rename(workspace, "no such input", "x").isEmpty());
    }

    void aWorkspaceWithNoFileIsGivenOneWithTheInputAlone()
    {
        QTemporaryDir dir;
        QString id;
        QCOMPARE(add(dir.path(), "Input 1", &id), QString());
        const rv::data::ProPresenterWorkspace document = readBack(dir.path());
        QCOMPARE(document.videoinputs_size(), 1);
        QCOMPARE(document.pro_screens_size(), 0);
    }

    void proPresentersActionIsReadForTheFirstInput()
    {
        const rv::data::Action action = proAction();
        QVERIFY(isAction(action));
        QCOMPARE(actionInput(action), 0);
        rv::data::Action other = action;
        other.mutable_media()->mutable_element()->clear_live_video();
        other.mutable_media()->mutable_element()->mutable_video();
        QVERIFY(!isAction(other));
    }

    void anActionIsMadeAsProPresenterWritesOne()
    {
        rv::data::Action action;
        buildAction(2, &action);
        QVERIFY(isAction(action));
        QCOMPARE(actionInput(action), 2);
        QCOMPARE(action.type(), rv::data::Action::ACTION_TYPE_MEDIA);
        QVERIFY(action.isenabled());
        QCOMPARE(action.media().layer_type(), rv::data::Action::LAYER_TYPE_INPUT);
        QVERIFY(action.media().has_audio());
        QCOMPARE(action.media().element().live_video().drawing().alpha_type(), rv::data::ALPHA_TYPE_STRAIGHT);
        QCOMPARE(action.media().element().live_video().audio().volume(), 1.0);
        QCOMPARE(action.uuid().string().size(), size_t(36));
        QCOMPARE(action.media().element().uuid().string().size(), size_t(36));
        // (Not ProPresenter's sixteen channels of sound routed nowhere)
        QCOMPARE(action.media().element().live_video().audio().audio_channels_size(), 0);
    }

    void anActionThatIsThereIsGivenAnotherInputAndKeepsTheRest()
    {
        rv::data::Action action = proAction();
        buildAction(1, &action);
        QCOMPARE(actionInput(action), 1);
        QCOMPARE(action.uuid().string(), "516CCF1F-BC5D-432E-AC51-CD03BE2C8D4D");
        QCOMPARE(action.media().element().uuid().string(), "AEB6675B-1761-4C11-8FB9-9FEB4152EE6C");
        QCOMPARE(action.media().element().live_video().audio().audio_channels_size(), 2);
        // Back to the first: written as a place of 0, which proto3 leaves out, as
        // ProPresenter's own did
        buildAction(0, &action);
        QCOMPARE(action.SerializeAsString(), proAction().SerializeAsString());
    }

    void aFillIsAnInputByItsPlace()
    {
        rv::data::Media media;
        QVERIFY(!isInput(media));
        media.mutable_live_video()->mutable_drawing()->set_scale_behavior(rv::data::Media::SCALE_BEHAVIOR_FILL);
        setInput(3, &media);
        QVERIFY(isInput(media));
        QCOMPARE(inputOf(media), 3);
        QCOMPARE(media.live_video().drawing().scale_behavior(), rv::data::Media::SCALE_BEHAVIOR_FILL);
    }

    void automaticIsTheLargestPictureUpToFullHdAtItsFastest()
    {
        // The webcam of the maintainer's laptop, as Qt lists it
        QList<Mode> webcam {{1280, 720, 30, "Jpeg"}, {640, 480, 30, "Jpeg"}, {640, 360, 30, "Jpeg"}, {960, 540, 30, "Jpeg"},
                            {848, 480, 30, "Jpeg"}, {640, 480, 30, "YUYV"}, {640, 360, 30, "YUYV"}, {320, 240, 30, "YUYV"}};
        QCOMPARE(chooseMode(webcam, QString()), 0);
        QCOMPARE(webcam.at(0).key(), "1280x720@30 Jpeg");
        QCOMPARE(webcam.at(0).label(), "1280 × 720, 30 a second, compressed");
        // One asked for, and one asked for that the device does not have
        QCOMPARE(chooseMode(webcam, "640x480@30 YUYV"), 5);
        QCOMPARE(chooseMode(webcam, "1920x1080@60 Jpeg"), 0);
        // Of two the same size and speed, the one sent as it is
        QCOMPARE(chooseMode({{640, 480, 30, "Jpeg"}, {640, 480, 30, "YUYV"}}, QString()), 1);
        // Faster up to 30, and no bigger than 1920 by 1080 where there is one that is not
        QCOMPARE(chooseMode({{1920, 1080, 15, "YUYV"}, {1920, 1080, 30, "Jpeg"}, {3840, 2160, 30, "Jpeg"}}, QString()), 1);
        QCOMPARE(chooseMode({{1920, 1080, 30, "Jpeg"}, {1920, 1080, 60, "Jpeg"}}, QString()), 0);
        // Only bigger ones: the smallest of them
        QCOMPARE(chooseMode({{3840, 2160, 30, "Jpeg"}, {2560, 1440, 30, "Jpeg"}}, QString()), 1);
        QCOMPARE(chooseMode({}, QString()), -1);
    }

    void aDevicesOwnSoundIsFoundByItsName()
    {
        const QStringList inputs {"Built-in Audio Analog Stereo", "HD Pro Webcam C920 Analog Stereo", "Cam Link 4K Digital Stereo (IEC958)"};
        QCOMPARE(ownAudio("HD Pro Webcam C920", inputs), 1);
        QCOMPARE(ownAudio("Cam Link 4K: Cam Link 4K", inputs), 2);
        // The maintainer's laptop's webcam has no microphone of its own that says so
        QCOMPARE(ownAudio("Integrated_Webcam_HD: Integrate", inputs), -1);
        QCOMPARE(ownAudio("", inputs), -1);
    }
};

QTEST_GUILESS_MAIN(TestVideoInputFile)
#include "tst_videoinputfile.moc"
