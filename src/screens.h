#pragma once

#include "screenfile.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The workspace's screens, and what each is sent out through on this computer.
//
// A screen is somewhere the show is drawn for: an audience screen gets the slides, the
// media and the props; a stage screen gets a stage layout. A workspace can have sixteen,
// of both kinds together. Two things are known of each, and they are kept in two places:
//
//   - What it is: its id, its name and its kind. That is the workspace's, and is in
//     ProPresenter's own file of how the workspace is set up (see screenfile.h), so the
//     screens ProPresenter made are the screens here, and a stage action that names one
//     finds it.
//   - What it is sent out through: `output`. That is this computer's, since another
//     computer has other things plugged in, and is in the app's own settings, under the
//     screen's id. It is one of
//         "window"   a small window of its own that floats over the operator window,
//                    and can be made to fill whatever display it is put on;
//         "display"  one of the computer's displays, by its name, filled;
//         "ndi"      the network, as an NDI source of a given name, size and rate;
//         "none"     nothing: the screen is there for things to name, and is not drawn.
//     A screen nothing has been said about is a window if it is the first of its kind,
//     as the one output and the one stage display always were, and otherwise nothing.
//
// What ProPresenter says a screen is connected to (a display, a video card) is left in
// its file as it was found, and is not gone by here.
//
// That an output is one of a few kinds, each with its own settings, is meant to be
// added to: a video card, or a screen sent out with a delay, colour correction or its
// corners pinned, would be more of the same. So is what a screen shows being its own:
// each screen is drawn by itself (see the operator window, which makes one scene per
// screen), which is what lets a look, later, give each its own layers.
//
// There is one of these, which QML reaches by its name.
class Screens : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Every screen, in the workspace's order:
    //   { id, name, kind ("audience" or "stage"), first (of its kind), width, height,
    //     output, display, displayThere, ndiName, ndiWidth, ndiHeight, ndiRate }
    // `display` is the name of the display a "display" output fills and `displayThere`
    // whether one of that name is plugged in. The ndi ones are an "ndi" output's name
    // on the network, its size, and its frames a second as text ("30", "59.94").
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)
    // Those of each kind
    Q_PROPERTY(QVariantList audience READ audience NOTIFY changed)
    Q_PROPERTY(QVariantList stage READ stage NOTIFY changed)
    // The stage layout ProPresenter's file gives each stage screen, by the ids of both:
    // what a stage screen starts with where nothing else is remembered for it
    Q_PROPERTY(QVariantMap layouts READ layouts NOTIFY changed)
    // The computer's displays: { name, label, width, height }
    Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)
    // The frame rates an NDI output can be sent at, as text
    Q_PROPERTY(QStringList rates READ rates CONSTANT)
    Q_PROPERTY(int limit READ limit CONSTANT)
    // Whether what the screens are sent out through is read from the settings and kept
    // in them. A test or the self-test, which must not go by what was last set up on
    // this computer nor change it, has it off; set it before open().
    Q_PROPERTY(bool remember MEMBER m_remember NOTIFY rememberChanged)

public:
    explicit Screens(QObject *parent = nullptr);

    QVariantList screens() const { return m_screens; }
    QVariantList audience() const { return ofKind(false); }
    QVariantList stage() const { return ofKind(true); }
    QVariantMap layouts() const { return m_layouts; }
    QVariantList displays() const { return m_displays; }
    QStringList rates() const;
    int limit() const { return screenfile::limit; }

    // Reads the screens of a workspace folder. Answers with what went wrong, or nothing.
    Q_INVOKABLE QString open(const QString &workspace);

    // Changes to the list, each written to the workspace's file; each answers with what
    // went wrong, or nothing. `kind` is "audience" or "stage".
    Q_INVOKABLE QString add(const QString &kind);
    Q_INVOKABLE QString remove(const QString &id);
    Q_INVOKABLE QString rename(const QString &id, const QString &name);

    // What a screen is sent out through on this computer: any of output, display,
    // ndiName, ndiWidth, ndiHeight and ndiRate, as in `screens`.
    Q_INVOKABLE void setOutput(const QString &id, const QVariantMap &changes);

    // A frame rate as two whole numbers, as video counts it: "59.94" is 60000 over 1001.
    Q_INVOKABLE QVariantMap rateOf(const QString &rate) const;

signals:
    void changed();
    void displaysChanged();
    void rememberChanged();

private:
    QVariantList ofKind(bool stage) const;
    void readDisplays();
    void rebuild();
    QVariantMap outputOf(const screenfile::Screen &screen, bool first) const;

    QString m_workspace;
    QList<screenfile::Screen> m_list;
    QVariantList m_screens;
    QVariantMap m_layouts;
    QVariantList m_displays;
    // What has been set here for each screen's output, by the screen's id
    QMap<QString, QVariantMap> m_outputs;
    bool m_remember = true;
};
