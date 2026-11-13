#pragma once

#include <QByteArray>
#include <QSize>
#include <QString>

#include <optional>

// Pictures of slides, as ProPresenter's API gives them (GET .../thumbnail/{index}): the
// plain rules, in C++ with nothing of the windows or the processes in it. What draws the
// pictures is slidepictureworker.h; what asks for them, slidepictureservice.h.
//
// What ProPresenter does, from its documentation and as was seen of a ProPresenter 22 on
// 2026-11-13 (ProPresenter Remote shows every slide by one):
//
//  - A picture of a cue of a presentation (by its UUID, or an item of a playlist), the
//    cue counted in the arrangement the presentation is shown with (a playlist's item
//    brings its own). `quality` is the length in pixels of the picture's longer side,
//    400 unless asked; `thumbnail_type` is jpeg or png, and before the Accept header;
//    JPEG unless one asks for PNG.
//  - The slide as it is drawn, its media behind it, and where it is transparent a
//    checkerboard of 8-pixel squares (at 400 wide) of #222222 and #0a0a0a.
//  - A cue that is not there (-1, or past the last) is 404.
namespace slidepictures {

// What a picture is asked of: a presentation's file (with a playlist's choice of
// arrangement, or none for the presentation's own), the cue, the longer side, and PNG
// rather than JPEG
struct Asked
{
    QString file;
    std::optional<QString> arrangement;
    int index = 0;
    int quality = 400;
    bool png = false;

    bool operator==(const Asked &other) const = default;
};

// The longer side asked for, from the query's `quality` (as text): 400 if not given,
// and kept to 16..1920 (the app's own bounds: ProPresenter's are not known)
int qualityOf(const QString &asked);
// Whether PNG is asked for: thumbnail_type png, or with none, an Accept header that
// names image/png and not image/jpeg first
bool pngAsked(const QString &thumbnailType, const QByteArray &accept);
// The picture's size: its longer side `quality`, in the slide's own proportions
QSize sizeFor(double slideWidth, double slideHeight, int quality);
// The checkerboard's square for a picture this wide (8 at 400)
int squareFor(int width);

// What goes between the app and the worker: a request, one line of JSON; and the head of
// an answer, one line of JSON followed by that many bytes of the picture
QByteArray askedLine(qint64 id, const Asked &asked);
bool readAsked(const QByteArray &line, qint64 *id, Asked *asked);
struct Answer
{
    qint64 id = 0;
    // 200 with a picture; 404 for a cue or a file that is not there; 500 for one that
    // could not be drawn
    int status = 500;
    QByteArray type;
    qint64 length = 0;
    QString error;
};
QByteArray answerLine(const Answer &answer);
bool readAnswer(const QByteArray &line, Answer *answer);

// The name a picture is kept under, from everything it is drawn from: the request, and
// the times the presentation's file and the slide's media were last changed (so a change
// to either is a new picture), and the app's version (so a change to how slides are drawn
// is too)
QString cacheName(const Asked &asked, qint64 fileChanged, const QString &mediaPath, qint64 mediaChanged, const QString &version);

}
