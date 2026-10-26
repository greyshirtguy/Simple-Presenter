#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// How many channels of sound a file has, and what each is, for the Media Inspector's
// Audio tab, which has a row for each (and is there only for media that has sound, as
// ProPresenter's is). Qt's players do not say: this asks FFmpeg's libavformat, which the
// app has already (videoframe.cpp), of the file's best stream of sound.
//
// Reading it opens the file and its header, which for a file on a slow disc or a network
// can take a while: so MediaChannels does it on a thread of the pool, never the operator
// window's, and says when it has. (The length of a sound file, which the audio bin needs,
// is audioprobe.h's: it is asked of a player, and is not needed here.)
namespace mediachannels {

struct Found
{
    // 0 for a file with no sound (or that is not there, or is not media)
    int channels = 0;
    // What each is where the file says ("FL", "FR", "LFE"…, FFmpeg's names), else none
    QStringList names;
};

Found read(const QString &path);

} // namespace mediachannels

class MediaChannels : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // The file to read; "" for none
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    // How many channels it has: -1 while it is being read (and for no file)
    Q_PROPERTY(int channels READ channels NOTIFY found)
    Q_PROPERTY(QStringList names READ names NOTIFY found)

public:
    explicit MediaChannels(QObject *parent = nullptr);

    QString path() const { return m_path; }
    void setPath(const QString &path);
    int channels() const { return m_channels; }
    QStringList names() const { return m_names; }

signals:
    void pathChanged();
    void found();

private:
    QString m_path;
    int m_channels = -1;
    QStringList m_names;
    // Which reading is the one wanted: an answer to an earlier one is not taken
    int m_asked = 0;
};
