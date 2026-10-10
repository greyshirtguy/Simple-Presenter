#pragma once

#include <QQuickImageProvider>

// ProPresenter's own pictures, for the things this app has that ProPresenter has too:
// the look, the screens and the stage in the toolbar, the buttons that clear a layer,
// the tabs of the show controls, and what an action on a slide does. Someone who knows
// ProPresenter knows these at a glance, which is the reason for having them. Where
// they are from, and how the files in icons/ were made from them, is in
// icons/README.md.
//
// QML asks for one by name, and optionally in a colour:
//
//   image://icon/Looks            the picture as it is (for one with colours of its own)
//   image://icon/Looks/e6e6e6     its shape in that colour ("rrggbb", or "aarrggbb")
//
// at the size it will be drawn (see ProIcon.qml, which is how QML uses this). Most of
// the pictures are a shape and nothing more, white and more or less see-through, and
// are meant to be given a colour: the app's buttons say what state they are in by the
// colour of what is on them (lit, dimmed, an accent), which a picture of one fixed
// colour could not.
//
// Why a provider, and not a picture with an effect over it. What comes out of here is
// an ordinary small texture, made once for each name, size and colour that is asked for
// and then kept by QML's own store of pictures: drawing it costs what drawing any
// picture costs, the scene graph can batch it with the others, and nothing is run for
// each frame. Tinting in a shader would have been one more thing drawn separately for
// every icon on the screen. The price is that a colour cannot fade into another; the
// buttons change colour at once, as they did.
//
// The files are four times the size of the drawing in them (see icons/make.py), so
// every size asked for is a scaling down, which is what keeps the edges clean on a
// screen of any density.
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();

    // (This can be called from more than one thread, so it keeps nothing between calls.)
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
