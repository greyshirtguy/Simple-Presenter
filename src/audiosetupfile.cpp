#include "audiosetupfile.h"

#include "audiorouting.h"
#include "proworkspace.pb.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <functional>

namespace audiosetupfile {

namespace {

QString pathOf(const QString &workspace)
{
    return workspace + QStringLiteral("/Configuration/Workspace");
}

QString load(const QString &workspace, rv::data::ProPresenterWorkspace *document)
{
    document->Clear();
    QFile file(pathOf(workspace));
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("The workspace's set-up file could not be read: %1").arg(file.errorString());
    const QByteArray bytes = file.readAll();
    if (!document->ParseFromArray(bytes.constData(), int(bytes.size()))) {
        document->Clear();
        return QStringLiteral("The workspace's set-up file is not one this app can read, so it is left alone");
    }
    return {};
}

QString save(const QString &workspace, const rv::data::ProPresenterWorkspace &document)
{
    if (!QDir().mkpath(workspace + QStringLiteral("/Configuration")))
        return QStringLiteral("The workspace's Configuration folder could not be made");
    // Written beside the file and then put in its place, so that the file is never half
    // of one.
    QSaveFile file(pathOf(workspace));
    const std::string bytes = document.SerializeAsString();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size()) || !file.commit())
        return QStringLiteral("The workspace's set-up file could not be written: %1").arg(file.errorString());
    return {};
}

// A change to the buses: the file read whole, the buses the workspace has had put in it
// if it has none, `change` made to them, and the file written back.
QString change(const QString &workspace, const std::function<QString(google::protobuf::RepeatedPtrField<rv::data::DigitalAudio::Bus> *)> &change)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    auto *buses = document.mutable_digital_audio_setup()->mutable_buses();
    if (buses->empty()) {
        for (const Channel &channel : defaults())
            buses->Add()->set_name(channel.name.toStdString());
    }
    const QString refused = change(buses);
    if (!refused.isEmpty())
        return refused;
    if (document.audio_channel_count() != 0)
        document.set_audio_channel_count(uint32_t(buses->size()));
    return save(workspace, document);
}

} // namespace

QString defaultName(int index)
{
    return QStringLiteral("Channel %1").arg(index + 1);
}

QList<Channel> defaults()
{
    return {{defaultName(0)}, {defaultName(1)}};
}

Setup read(const QString &workspace)
{
    Setup setup;
    rv::data::ProPresenterWorkspace document;
    setup.error = load(workspace, &document);
    const auto &buses = document.digital_audio_setup().buses();
    for (int index = 0; index < qMin(buses.size(), audiorouting::kMostChannels); ++index) {
        const auto &bus = buses[index];
        Channel channel;
        channel.name = QString::fromStdString(bus.name());
        if (channel.name.trimmed().isEmpty())
            channel.name = defaultName(index);
        channel.muted = bus.muted();
        channel.solo = bus.solo();
        setup.channels.append(channel);
    }
    setup.fromFile = !setup.channels.isEmpty();
    if (!setup.fromFile)
        setup.channels = defaults();
    return setup;
}

QString setCount(const QString &workspace, int count)
{
    if (count < 1 || count > audiorouting::kMostChannels)
        return QStringLiteral("The app has from one to %1 channels").arg(audiorouting::kMostChannels);
    return change(workspace, [count](auto *buses) {
        while (buses->size() > count)
            buses->RemoveLast();
        while (buses->size() < count) {
            const std::string name = defaultName(buses->size()).toStdString();
            buses->Add()->set_name(name);
        }
        return QString();
    });
}

QString rename(const QString &workspace, int index, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return QStringLiteral("A channel needs a name");
    return change(workspace, [index, trimmed](auto *buses) {
        if (index < 0 || index >= buses->size())
            return QStringLiteral("There is no such channel");
        (*buses)[index].set_name(trimmed.toStdString());
        return QString();
    });
}

QString setMuted(const QString &workspace, int index, bool muted)
{
    return change(workspace, [index, muted](auto *buses) {
        if (index < 0 || index >= buses->size())
            return QStringLiteral("There is no such channel");
        (*buses)[index].set_muted(muted);
        return QString();
    });
}

QString setSolo(const QString &workspace, int index, bool solo)
{
    return change(workspace, [index, solo](auto *buses) {
        if (index < 0 || index >= buses->size())
            return QStringLiteral("There is no such channel");
        (*buses)[index].set_solo(solo);
        return QString();
    });
}

QString setAllMuted(const QString &workspace, bool muted)
{
    return change(workspace, [muted](auto *buses) {
        for (auto &bus : *buses)
            bus.set_muted(muted);
        return QString();
    });
}

QString clearSolos(const QString &workspace)
{
    return change(workspace, [](auto *buses) {
        for (auto &bus : *buses)
            bus.set_solo(false);
        return QString();
    });
}

}
