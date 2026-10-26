// The app's channels of sound in ProPresenter's set-up file (src/audiosetupfile.h), tried on
// folders made for the purpose: what is read from a workspace with no file and from one as
// ProPresenter writes it (four channels, a device of six with its matrix, and an output
// for SDI and NDI); what each change writes; and that the rest of the file, the device and
// its matrix most of all, goes back as it came.

#include "audiosetupfile.h"

#include "proworkspace.pb.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace audiosetupfile;

namespace {

rv::data::ProPresenterWorkspace fileOf(const QString &workspace)
{
    rv::data::ProPresenterWorkspace document;
    QFile file(workspace + "/Configuration/Workspace");
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = file.readAll();
        document.ParseFromArray(bytes.constData(), int(bytes.size()));
    }
    return document;
}

// A file as ProPresenter writes it, in the form of the maintainer's: four channels
// named by ProPresenter, an interface of six outputs with the first four fed by the four
// channels and the last two by none, and an output for SDI and NDI with a matrix of its
// own; and a screen and a setting of its own besides.
void writeProPresenters(const QString &workspace, bool withCount = false)
{
    rv::data::ProPresenterWorkspace document;
    auto *setup = document.mutable_digital_audio_setup();
    for (int bus = 1; bus <= 4; ++bus)
        setup->add_buses()->set_name("Pro Channel " + std::to_string(bus));
    auto *device = setup->mutable_main_output_device();
    device->set_name("Scarlett 6i6 USB");
    device->set_renderid("AppleUSBAudioEngine:Focusrite:Scarlett 6i6 USB:00034788:1,2");
    auto *format = device->add_formats();
    format->set_sample_rate(48000);
    format->set_bit_depth(32);
    format->set_type(rv::data::DigitalAudio::Device::Format::TYPE_FLOAT);
    auto *routing = device->mutable_routing();
    for (int channel = 0; channel < 6; ++channel)
        routing->add_channels();
    for (int channel = 0; channel < 6; ++channel) {
        auto *map = routing->add_map();
        map->set_channel_index(uint32_t(channel));
        if (channel < 4)
            map->add_mapped_indices(uint32_t(channel));
    }
    routing->set_is_custom_map(true);
    routing->mutable_master_channel();
    auto *sdi = setup->mutable_sdi_ndi_device();
    sdi->set_name("SDI NDI");
    sdi->mutable_routing()->add_map()->add_mapped_indices(0);
    setup->set_monitor_on_mains(true);
    auto *screen = document.add_pro_screens();
    screen->set_name("Main Hall");
    screen->mutable_uuid()->set_string("AAAA");
    document.set_selected_library_name("Songs");
    document.set_audio_input_transition_time(1);
    if (withCount)
        document.set_audio_channel_count(4);
    QDir().mkpath(workspace + "/Configuration");
    QFile file(workspace + "/Configuration/Workspace");
    QVERIFY(file.open(QIODevice::WriteOnly));
    const std::string bytes = document.SerializeAsString();
    file.write(bytes.data(), qint64(bytes.size()));
}

QStringList namesOf(const Setup &setup)
{
    QStringList names;
    for (const Channel &channel : setup.channels)
        names << channel.name;
    return names;
}

} // namespace

class TestAudioSetupFile : public QObject
{
    Q_OBJECT

private slots:
    void noFile()
    {
        QTemporaryDir folder;
        const Setup setup = read(folder.path());
        QVERIFY(!setup.fromFile);
        QVERIFY(setup.error.isEmpty());
        QCOMPARE(namesOf(setup), (QStringList{"Channel 1", "Channel 2"}));
        // The first change writes the two, and then the change
        QVERIFY(rename(folder.path(), 1, "Right").isEmpty());
        const Setup after = read(folder.path());
        QVERIFY(after.fromFile);
        QCOMPARE(namesOf(after), (QStringList{"Channel 1", "Right"}));
    }

    void readsProPresenters()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        const Setup setup = read(folder.path());
        QVERIFY(setup.fromFile);
        QCOMPARE(namesOf(setup), (QStringList{"Pro Channel 1", "Pro Channel 2", "Pro Channel 3", "Pro Channel 4"}));
        QVERIFY(!setup.channels[0].muted && !setup.channels[0].solo);
    }

    void count()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        const rv::data::ProPresenterWorkspace before = fileOf(folder.path());
        QVERIFY(setCount(folder.path(), 6).isEmpty());
        QCOMPARE(namesOf(read(folder.path())), (QStringList{"Pro Channel 1", "Pro Channel 2", "Pro Channel 3", "Pro Channel 4", "Channel 5", "Channel 6"}));
        // The device, its matrix, the SDI and NDI output and everything else as they were
        rv::data::ProPresenterWorkspace after = fileOf(folder.path());
        QCOMPARE(after.digital_audio_setup().main_output_device().SerializeAsString(), before.digital_audio_setup().main_output_device().SerializeAsString());
        QCOMPARE(after.digital_audio_setup().sdi_ndi_device().SerializeAsString(), before.digital_audio_setup().sdi_ndi_device().SerializeAsString());
        QVERIFY(after.digital_audio_setup().monitor_on_mains());
        QCOMPARE(after.pro_screens_size(), 1);
        QCOMPARE(after.selected_library_name(), std::string("Songs"));
        QCOMPARE(after.audio_input_transition_time(), 1.0);
        // A count the file did not have is not given it
        QCOMPARE(after.audio_channel_count(), 0u);
        // Fewer: the last taken off
        QVERIFY(setCount(folder.path(), 2).isEmpty());
        QCOMPARE(namesOf(read(folder.path())), (QStringList{"Pro Channel 1", "Pro Channel 2"}));
        // From one to sixteen, and nothing changed for a number outside them
        QVERIFY(!setCount(folder.path(), 0).isEmpty());
        QVERIFY(!setCount(folder.path(), 17).isEmpty());
        QCOMPARE(read(folder.path()).channels.size(), 2);
        // The whole of everything else, unchanged by the counting
        after = fileOf(folder.path());
        rv::data::ProPresenterWorkspace wanted = before;
        wanted.mutable_digital_audio_setup()->mutable_buses()->DeleteSubrange(2, 2);
        QCOMPARE(after.SerializeAsString(), wanted.SerializeAsString());
    }

    void countKeptWhereTheFileHasOne()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path(), true);
        QVERIFY(setCount(folder.path(), 8).isEmpty());
        QCOMPARE(fileOf(folder.path()).audio_channel_count(), 8u);
    }

    void namesMutesSolos()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        QVERIFY(rename(folder.path(), 2, "  Band  ").isEmpty());
        QVERIFY(!rename(folder.path(), 2, "   ").isEmpty());
        QVERIFY(!rename(folder.path(), 9, "Nothing").isEmpty());
        QVERIFY(setMuted(folder.path(), 1, true).isEmpty());
        QVERIFY(setSolo(folder.path(), 3, true).isEmpty());
        Setup setup = read(folder.path());
        QCOMPARE(setup.channels[2].name, QString("Band"));
        QVERIFY(setup.channels[1].muted);
        QVERIFY(setup.channels[3].solo);
        QVERIFY(clearSolos(folder.path()).isEmpty());
        QVERIFY(setAllMuted(folder.path(), true).isEmpty());
        setup = read(folder.path());
        for (const Channel &channel : setup.channels)
            QVERIFY(channel.muted && !channel.solo);
        QVERIFY(setAllMuted(folder.path(), false).isEmpty());
        // A channel's tone is never written
        const rv::data::ProPresenterWorkspace written = fileOf(folder.path());
        for (const auto &bus : written.digital_audio_setup().buses())
            QVERIFY(!bus.test_tone());
    }

    void notOneThisAppCanRead()
    {
        QTemporaryDir folder;
        QDir().mkpath(folder.path() + "/Configuration");
        QFile file(folder.path() + "/Configuration/Workspace");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("\xff\xff\xff not a workspace");
        file.close();
        const Setup setup = read(folder.path());
        QVERIFY(!setup.error.isEmpty());
        QCOMPARE(setup.channels.size(), 2);
        // And left alone
        QVERIFY(!setCount(folder.path(), 4).isEmpty());
        QFile again(folder.path() + "/Configuration/Workspace");
        QVERIFY(again.open(QIODevice::ReadOnly));
        QCOMPARE(again.readAll(), QByteArray("\xff\xff\xff not a workspace"));
    }
};

QTEST_GUILESS_MAIN(TestAudioSetupFile)
#include "tst_audiosetupfile.moc"
