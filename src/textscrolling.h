#pragma once

#include <QObject>
#include <QSizeF>
#include <QVariant>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// The rules of Scrolling Text (textscroller.h) for QML, and what ScrollingText needs to
// know of its words: how big they are laid out with room to spare (a line that goes
// across is as long as it is) or in the box's width (words that go up or down).
class TextScrolling : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // How fast, in slide units a second, at a Speed of `rate`
    Q_INVOKABLE double speed(double rate) const;
    // Where the words start, as textscroller::start says
    Q_INVOKABLE double start(bool offScreen, double room) const;
    // Where each going of the words is (textscroller::positions)
    Q_INVOKABLE QVariantList positions(double travelled, double start, double length, double gap, double room, bool repeat) const;
    // The size of the words StrokedText draws for `content` (a RichText) and
    // `replacement` (StrokedText::shown), laid out `width` wide, in slide units: as wide
    // as their widest line and as high as their lines
    Q_INVOKABLE QSizeF naturalSize(const QVariant &content, const QVariant &replacement, double width) const;
};
