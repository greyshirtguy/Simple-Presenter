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
//     screen's id (and so is its video delay, which goes with what it is sent out through:
//     ProPresenter keeps one with each output, in the file, for its own outputs, and that
//     is left as it is). It is one of
//         "window"   a small window of its own that floats over the operator window,
//                    and can be made to fill whatever display it is put on;
//         "display"  one of the computer's displays, by its name, filled;
//         "ndi"      the network, as an NDI source of a given name, size and rate;
//         "decklink" an output of a Blackmagic DeckLink card, in a given format, and
//                    keyed or not (see decklink.h);
//         "none"     nothing: the screen is there for things to name, and is not drawn.
//     A screen nothing has been said about is a window if it is the first of its kind,
//     as the one output and the one stage display always were, and otherwise nothing.
//
// What ProPresenter says a screen is connected to (a display, a video card) is left in
// its file as it was found, and is not gone by here.
//
// That an output is one of a few kinds, each with its own settings, is meant to be
// added to: a screen's colour corrected, or its corners pinned, would be more of the
// same, as the card's outputs and the video delay are. So is what a screen shows being
// its own: each screen is drawn by itself (see the operator window, which makes one
// scene per screen), which is what lets a look, later, give each its own layers.
//
// There is one of these, which QML reaches by its name.
class Screens : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Every screen, in the workspace's order:
    //   { id, name, kind ("audience" or "stage"), first (of its kind), width, height,
    //     output, display, displayThere, ndiName, ndiWidth, ndiHeight, ndiRate,
    //     decklinkOutput, decklinkName, decklinkFormat, decklinkAlpha, decklinkKey,
    //     decklinkBlend, videoDelay }
    // `display` is the name of the display a "display" output fills and `displayThere`
    // whether one of that name is plugged in. The ndi ones are an "ndi" output's name
    // on the network, its size, and its frames a second as text ("30", "59.94"). The
    // decklink ones are a "decklink" output's card output (by DeckLink's id, and the name
    // it had), its format ("1080p29.97"), and its keying: alpha "none", "premultiplied"
    // or "straight", key "internal" or "external", and the blend, 0 to 100. `videoDelay` is
    // how many of its frames late it is shown (see OutputScene), 0 to delayline::kMostFrames.
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)
    // Those of each kind
    Q_PROPERTY(QVariantList audience READ audience NOTIFY changed)
    Q_PROPERTY(QVariantList stage READ stage NOTIFY changed)
    // The stage layout ProPresenter's file gives each stage screen, by the ids of both:
    // what a stage screen starts with where nothing else is remembered for it
    Q_PROPERTY(QVariantMap layouts READ layouts NOTIFY changed)
    // The computer's displays: { name, label, width, height, refresh (times a second) }
    Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)
    // The frame rates an NDI output can be sent at, as text
    Q_PROPERTY(QStringList rates READ rates CONSTANT)
    Q_PROPERTY(int limit READ limit CONSTANT)
    // The most frames a screen's video can be delayed by, and whether a delay can be made
    // here at all (the app drawing with OpenGL: see FrameDelay)
    Q_PROPERTY(int mostVideoDelay READ mostVideoDelay CONSTANT)
    Q_PROPERTY(bool videoDelayPossible READ videoDelayPossible CONSTANT)
    // Whether the audience screens show how they are doing (ProPresenter's Show Performance
    // Statistics On Screen; see OutputScene): this computer's, in the app's settings, as
    // ProPresenter keeps it in its preferences
    Q_PROPERTY(bool showStatistics READ showStatistics WRITE setShowStatistics NOTIFY showStatisticsChanged)
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
    int mostVideoDelay() const;
    bool videoDelayPossible() const;
    bool showStatistics() const { return m_showStatistics; }
    void setShowStatistics(bool show);

    // Reads the screens of a workspace folder. Answers with what went wrong, or nothing.
    Q_INVOKABLE QString open(const QString &workspace);

    // Changes to the list, each written to the workspace's file; each answers with what
    // went wrong, or nothing. `kind` is "audience" or "stage".
    Q_INVOKABLE QString add(const QString &kind);
    Q_INVOKABLE QString remove(const QString &id);
    Q_INVOKABLE QString rename(const QString &id, const QString &name);

    // What a screen is sent out through on this computer: any of output, display,
    // ndiName, ndiWidth, ndiHeight and ndiRate, and the decklink ones, as in `screens`.
    Q_INVOKABLE void setOutput(const QString &id, const QVariantMap &changes);

    // A frame rate as two whole numbers, as video counts it: "59.94" is 60000 over 1001.
    Q_INVOKABLE QVariantMap rateOf(const QString &rate) const;
    // How long a frame of a screen is (as `screens` has it), in milliseconds, for its video
    // delay, which is in its frames: a refresh of the display it fills (of the first, for a
    // window, which can be anywhere), a frame of its NDI rate or of its card's format
    Q_INVOKABLE double frameIntervalOf(const QVariantMap &screen) const;
    // A refresh of the display of that name, in milliseconds (a sixtieth of a second for
    // one not known): for a window, by the display it is on
    Q_INVOKABLE double refreshIntervalOf(const QString &display) const;

signals:
    void changed();
    void displaysChanged();
    void rememberChanged();
    void showStatisticsChanged();

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
    bool m_showStatistics = false;
};
