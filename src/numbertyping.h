#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// A slide asked for by its number, for the operator window: what the digits typed so
// far make, and which slide that is. The rules are slidenumber's (see slidenumber.h,
// which says how the whole of it goes); this is only what QML reaches them by.
//
// There is one of these, which QML reaches by its name.
class NumberTyping : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // What is typed so far with one more key, and with its last digit taken off
    Q_INVOKABLE QString typed(const QString &soFar, const QString &key) const;
    Q_INVOKABLE QString erased(const QString &soFar) const;
    // The place, from 0, of the slide a number asks for among so many; -1 if none has it
    Q_INVOKABLE int slideFor(const QString &typed, int slides) const;
};
