#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace rv::data {
class Action;
class Media;
}

// Video inputs: live video coming into the computer (a camera, a capture card) that the
// show can put on the screens, as ProPresenter has them. Plain rules on ProPresenter's
// files, with nothing of the windows or of the capture in them, so that they can be
// tried by themselves (tests/unit/tst_videoinputfile.cpp); VideoInputs (videoinputs.h)
// does the capturing.
//
// Where ProPresenter keeps them, and so where this does:
//
//   - The list. A workspace's Configuration/Workspace (ProPresenterWorkspace, the file
//     the screens, the looks and the masks are in) has `videoInputs`: each an id, a name
//     ("Input 1" is what ProPresenter calls its first), a colour it is tagged with, a
//     thumbnail, how its sound is chosen, and the device it is, as ProPresenter on the
//     computer that made it found the device. The list is read here, and an input added,
//     renamed or removed in it; everything else in the file goes back as it came. The
//     device, the mode it is captured in, its sound and its thumbnail are this
//     computer's (another computer has other things plugged in) and are kept in the
//     app's own settings, not here: ProPresenter's say about them is left as it is.
//
//   - What names an input. A slide's or a macro's Video Input Action, and a shape filled
//     with one, name it by its PLACE in that list (`live_video_index`, from 0), and not
//     by its id: so the first input is the one a file that says nothing names. (Seen in a
//     presentation of the maintainer's, 2026-10-20, whose action says nothing and whose
//     workspace has one input.) An input removed from the list moves those after it up
//     a place, and what named them names another; ProPresenter, which names them the
//     same way, presumably does the same.
//
//   - The action. A Video Input Action is a media action whose element is live video,
//     on the layer ProPresenter calls the input layer (LAYER_TYPE_INPUT). One made here
//     is written as that one of ProPresenter's is, but for what it has no use for: its
//     sixteen channels of sound routed nowhere, which ProPresenter writes and the file
//     format does not need, are left out. A GUESS that ProPresenter reads one without
//     them as it would one with.
//
//   - A fill. A shape filled with an input has a media fill whose media is live video,
//     with the input's place in it. None of the maintainer's files has one: that it is
//     written as the action's element is, is A GUESS from the file format.
//
// The maintainer's choices, 2026-10-21: set up as ProPresenter does, the list in the
// workspace's file and each input's device on this computer; the computer's own capture
// devices (what Linux and Qt find) now, Blackmagic DeckLink inputs after; with sound.
namespace videoinputfile {

struct Input
{
    QString id;
    QString name;
    // The colour it is tagged with, "#rrggbb", "" for none
    QString color;
    // How ProPresenter chooses its sound: 0 the device's own, 1 none, 2 another source
    int audioType = 0;
    // The device as ProPresenter's file names it, which is a device of the computer that
    // made the file, and is only shown
    QString proDevice;
    bool operator==(const Input &other) const = default;
};

// The workspace's inputs, in the file's order (which is what an action's number goes
// by). A workspace with no file, or with no inputs in it, has none, which is not an
// error.
QList<Input> inputs(const QString &workspace, QString *error);

// Each changes the file and writes it back, and returns an error message, empty on
// success. A new input is last in the list, named `name` and tagged in the colour
// ProPresenter gave its own first (as the maintainer's file has it); its id is put in
// *id.
QString add(const QString &workspace, const QString &name, QString *id);
QString rename(const QString &workspace, const QString &id, const QString &name);
QString remove(const QString &workspace, const QString &id);

// Whether an action is a Video Input Action, and which input it is for (its place)
bool isAction(const rv::data::Action &action);
int actionInput(const rv::data::Action &action);
// Makes `action` a Video Input Action for the input at that place, as ProPresenter
// writes one (see above). One that is one already keeps everything else it has.
void buildAction(int input, rv::data::Action *action);

// Whether a piece of media is a video input, which input (its place), and making it one
// for a fill (see above): what else the media had, its drawing, is kept.
bool isInput(const rv::data::Media &media);
int inputOf(const rv::data::Media &media);
void setInput(int input, rv::data::Media *media);

// ---- This computer's side of an input: how its device is captured, and its sound

// One of the ways a device can send its picture: its size, how many frames a second at
// most, and the form of its frames as Qt names it ("Jpeg", "YUYV" and so on). Its key,
// "1280x720@30 Jpeg", is what is kept of the choice; its label is what is shown.
struct Mode
{
    int width = 0;
    int height = 0;
    double fps = 0;
    QString format;
    QString key() const;
    QString label() const;
};
// Which of a device's modes it is captured in, by its place among them: the one asked
// for by its key, if the device has it, and otherwise (or for "", which is Automatic)
// the best of them for a show: the largest picture no bigger than 1920 by 1080, the
// faster of two the same size (up to 30 frames a second, which is what the screens are
// fed at), and of two the same, one the device sends as it is (YUYV and the like) over
// one it sends compressed (JPEG), which the computer would have to decode, frame by
// frame. -1 for a device that says it has none. (A GUESS at what ProPresenter's own
// automatic choice is: its help says only that a device that says what it sends is set
// to that.)
int chooseMode(const QList<Mode> &modes, const QString &wanted);
// The sound that is a device's own, among the computer's audio inputs by their names:
// the place of the one whose name has the device's name in it, or the device's first
// word of more than three letters; -1 for none. (A camera with a microphone in it comes
// as two devices, one for each, whose names are alike: "HD Pro Webcam C920" and "HD Pro
// Webcam C920 Analog Stereo". A GUESS at what "the device's embedded audio" is, as
// ProPresenter's help calls it, on Linux.)
int ownAudio(const QString &device, const QStringList &audioInputs);

} // namespace videoinputfile
