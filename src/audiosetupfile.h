#pragma once

#include <QList>
#include <QString>

// The app's channels of sound, as ProPresenter's file of how the workspace is set up has
// them (Configuration/Workspace: its `digital_audio_setup`, whose `buses` are
// ProPresenter's channels, each with its name and its Mute and Solo).
//
// A workspace has as many channels as it has buses, up to sixteen
// (audiorouting::kMostChannels): ProPresenter's channel count. One whose file has none
// (a workspace ProPresenter has not set up, or a new one) has the two every workspace
// has had, which are not written until the list is first changed. A GUESS: that
// ProPresenter, too, starts a workspace with two, and calls them as this app does,
// "Channel 1" and "Channel 2". (The maintainer's own have "Pro Channel 1" and so on,
// which are kept as they are.)
//
// Of the file, the app reads and writes the buses and nothing else of how sound is set
// up: the output device ProPresenter names there, its rate and its matrix of channels,
// are of the computer ProPresenter runs on, and are left exactly as they were found (see
// AudioMixer for this computer's own). The file's `audio_channel_count`, where
// ProPresenter has set it, is kept equal to the number of buses (a GUESS at what it is:
// none of the maintainer's files have it). A channel's test tone is never written: it is
// for the moment it is switched on (ProPresenter's file has a field for it, which is left
// alone), so that a workspace never opens with a tone sounding.
//
// The file holds a great deal more (screens, looks, masks, recording). As everywhere, a
// change is made by reading the whole file, altering what the change is about and
// writing the whole back, so the rest goes back as it came. These are plain functions
// on a workspace folder (tests/unit/tst_audiosetupfile.cpp).
namespace audiosetupfile {

struct Channel
{
    QString name;
    bool muted = false;
    bool solo = false;

    bool operator==(const Channel &other) const = default;
};

struct Setup
{
    QList<Channel> channels;
    // Whether the file says what channels there are
    bool fromFile = false;
    // What went wrong reading the file, if anything did
    QString error;
};

// The channels of a workspace whose file has none: two
QList<Channel> defaults();
// The name a channel is given when one is added: "Channel 3" for the third
QString defaultName(int index);

Setup read(const QString &workspace);

// Each of these changes the file (making it, with the channels the workspace has had so
// far, if it says nothing of them) and answers with what went wrong, or nothing.
//
// As many channels as `count`, from one to sixteen: the last taken off, or new ones put
// on after them with their default names.
QString setCount(const QString &workspace, int count);
QString rename(const QString &workspace, int index, const QString &name);
QString setMuted(const QString &workspace, int index, bool muted);
QString setSolo(const QString &workspace, int index, bool solo);
// Every channel muted or not, and every solo taken off: ProPresenter's M and S at the
// top of its matrix
QString setAllMuted(const QString &workspace, bool muted);
QString clearSolos(const QString &workspace);

}
