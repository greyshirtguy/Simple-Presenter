#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// How this run of the app decodes video, for the settings screen and for the notice
// the guard puts up: whether the graphics chip's video decoders are left untried, who
// asked for that, and what the guard makes of this computer. The choice itself, and all
// there is to say about it, is decodingchoice's (see decodingchoice.h, and
// decodingguard.h for the guard); it is made once, as the app starts, and this is only
// what QML reads it by.
//
// There is one of these, which QML reaches by its name.
class VideoDecoding : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Whether every video is decoded by the processor, with no hardware decoder tried
    Q_PROPERTY(bool softwareOnly READ softwareOnly CONSTANT)
    // The same in a few words, with who asked: "by the processor only, as the settings
    // ask"
    Q_PROPERTY(QString inWords READ inWords CONSTANT)
    // Whether this computer is one the guard holds hardware decoding back on, and why,
    // in words that finish "On this computer ..."
    Q_PROPERTY(bool guarded READ guarded CONSTANT)
    Q_PROPERTY(QString guardReason READ guardReason CONSTANT)
    // Whether the guard is why this run decodes by the processor: nobody having asked
    // for anything, it held
    Q_PROPERTY(bool guardInForce READ guardInForce CONSTANT)
    // Whether hardware decoding is being tried on a computer the guard would hold it
    // back on, because it was asked for
    Q_PROPERTY(bool overridden READ overridden CONSTANT)

public:
    using QObject::QObject;

    bool softwareOnly() const;
    QString inWords() const;
    bool guarded() const;
    QString guardReason() const;
    bool guardInForce() const;
    bool overridden() const;
};
