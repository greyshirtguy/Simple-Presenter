#pragma once

#include "disabledatefile.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Disable dates, for QML: slides set to be disabled at a date and time, this app's own
// (see disabledatefile.h for why they are kept where they are, and the file). Read when a
// workspace is opened, written as they are set and taken off, and carried out as they come.
//
// The file is the truth: each change reads it as it is now, makes the one change and writes
// it back, so that what else is in it is kept.
//
// The clock, as the Calendar's (calendarevents.h): nothing looks at the time while nothing is
// due; a timer is set for the next date (and for no more than an hour on, for a clock put
// right or a computer that slept). A date that has come is carried out however late: a
// workspace opened after a slide's date has the slide disabled then (unlike the Calendar's
// events, which are missed: a slide disabled late is still to be disabled). Carried out, the
// slide is disabled in its presentation's file as ProPresenter disables one (its cue's
// isEnabled, ProDocument::setCueEnabled), after the file where the presentation was has been
// read and found to be that presentation still; and the date is taken out of the file.
// `disabled` then says so, for the operator window to read the presentation again. One that
// cannot be carried out (its presentation not where it was, or not that presentation) is kept,
// said in the log once, and tried again when the workspace is next opened.
class DisableDates : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Goes up with every change to the dates, so that what shows them is asked again
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit DisableDates(QObject *parent = nullptr);

    int revision() const { return m_revision; }

    // Reads a workspace folder's dates, and carries out at once those that have come.
    // Returns an error message, empty on success.
    Q_INVOKABLE QString open(const QString &workspace);
    // The date a slide of a presentation (by their uuids) is to be disabled at, in seconds
    // since 1970; 0 for none
    Q_INVOKABLE double dateOf(const QString &presentation, const QString &slide) const;
    // The dates of a presentation's slides: { slide id: seconds since 1970 }, for the
    // slides' grid to mark them by
    Q_INVOKABLE QVariantMap datesOf(const QString &presentation) const;
    // A date for a slide of the presentation at `path` (a file of this workspace), in seconds
    // since 1970, in place of any it had; one that has come already is carried out at once.
    // Returns an error message, empty on success.
    Q_INVOKABLE QString set(const QString &presentation, const QString &path, const QString &name, const QString &slide, double at);
    // A slide's date taken off: an error message, empty on success (and for a slide with none)
    Q_INVOKABLE QString clear(const QString &presentation, const QString &slide);

signals:
    void changed();
    // A slide of the presentation at `path` has been disabled, its date having come
    void disabled(const QString &path, const QString &slide);

private:
    QString file() const;
    QString reload();
    QString save();
    void schedule();
    void look();
    bool carryOut(const disabledatefile::Entry &entry);

    QString m_workspace;
    QList<disabledatefile::Entry> m_entries;
    // Dates that could not be carried out, said in the log already: presentation + slide
    QSet<QString> m_said;
    QTimer m_timer;
    int m_revision = 0;
};
