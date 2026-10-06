#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// What is live, for whatever shows it somewhere other than on the output.
//
// A text box can be linked to the words of the slide that is live, or of the one that
// comes after it. That is what a stage layout is made of: the people on stage read the
// words from boxes like that, laid out as a slide is (see StageLayouts). Such a box is
// drawn by asking here what the words are now (SlideElement does, as it asks Timers for
// a timer's time), so here is where the operator window says what is live.
//
// The words of a slide are those of its text boxes that show, one after another, in the
// order the slide has them, with no formatting: the box that shows them has its own.
//
// There is one of these, which QML reaches by its name.
class Show : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The slide that is live and the one that would come next, as maps (see
    // proconvert), or empty maps for none
    Q_PROPERTY(QVariantMap currentSlide READ currentSlide WRITE setCurrentSlide NOTIFY changed)
    Q_PROPERTY(QVariantMap nextSlide READ nextSlide WRITE setNextSlide NOTIFY changed)
    // Goes up whenever either changes: read it in a binding to have the binding follow.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    // Which of a slide's text a link asks for, as ProPresenter's files have it: all of
    // its words; its notes; or the words of its elements of a given name
    enum Source { Words = 0, Notes = 1, ElementNamed = 2 };
    Q_ENUM(Source)

    using QObject::QObject;

    QVariantMap currentSlide() const { return m_current; }
    void setCurrentSlide(const QVariantMap &slide);
    QVariantMap nextSlide() const { return m_next; }
    void setNextSlide(const QVariantMap &slide);
    int revision() const { return m_revision; }

    // What a text box linked to a slide's text shows: of the live slide, or with `next`
    // of the one after it; `source` is a Source and `name` the element name it goes by,
    // if it goes by one; `transform` is what is done to the text on the way, as for
    // text linked from another element (see proconvert). Nothing for a slide's notes,
    // which this app does not read.
    Q_INVOKABLE QString slideText(bool next, int source, const QString &name, int transform) const;

signals:
    void changed();

private:
    QVariantMap m_current;
    QVariantMap m_next;
    int m_revision = 0;
};
