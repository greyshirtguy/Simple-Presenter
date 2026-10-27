#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// The rules of which slides are selected (slideselection.h) as QML can call them:
// `SlideSelection`. There is nothing here but the handing over; what each does, and why,
// is said there. Places come and go as lists of numbers.
class SlideSelection : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    Q_INVOKABLE QVariantList toggled(const QVariantList &selected, int active, int clicked) const;
    Q_INVOKABLE QVariantList extended(const QVariantList &selected, int last, int active, int clicked) const;
    Q_INVOKABLE QVariantList targets(const QVariantList &selected, int count) const;
};
