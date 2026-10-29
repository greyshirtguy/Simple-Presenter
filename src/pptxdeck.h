#pragma once

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QMarginsF>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>

#include <optional>
#include <variant>

// What goes on the slides of a PowerPoint file made from a presentation (Export to
// PowerPoint), said without PowerPoint's XML and without ProPresenter's protobufs.
//
// This, and the rest of the export (pptximport.h, pptxwriter.h), is a port of the
// maintainer's own ProToPPTX (github.com/greyshirtguy/ProToPPTX, MIT), a Mac app that
// does the same: its reading of ProPresenter's files and its writing of PowerPoint's,
// and what it found out by measuring ProPresenter's exported slide pictures and
// PowerPoint's own pictures of its decks (its docs/NOTES.md). Its parts keep their
// names here, so the two can be read side by side; what the Mac did with its own
// libraries (CoreText, ImageIO, AVFoundation) is done here with Qt, fontconfig and
// FFmpeg, and media and fonts are found as the rest of this app finds them.
//
// Why there is a middle: the reader of ProPresenter's files and the writer of
// PowerPoint's are each hard enough without knowing about the other. pptximport turns a
// presentation into a Deck; pptxwriter turns a Deck into a .pptx.
//
// Units: every length is in the units of the ProPresenter slide it came from: its
// pixels, 1920 across on a 1080p slide. A font size is in the same units, because a
// ProPresenter font size of 72 is 72 of those pixels tall on screen. The writer scales
// all of it to the size of the PowerPoint slide in one place (pptx::Units).
//
// Colours are sRGB with alpha. Angles are degrees, clockwise.
//
// Everything here is a plain value: nothing in it reads a file (a medium's bytes are
// read when the deck is written), so a Deck can be made, looked at and thrown away
// cheaply, and on any thread.

namespace pptx {

// How a picture or video is fitted into the box it is given.
enum class ScaleMode {
    // All of it shows, as large as fits; the rest of the box is left empty.
    Fit,
    // The box is filled; what does not fit is cut off.
    Fill,
    // Stretched to the box, whatever its shape.
    Stretch,
};

// The largest box of `content`'s shape that fits inside `box`, centred (Fit), or the
// smallest that covers it (Fill), or the box itself (Stretch).
QRectF placing(const QRectF &box, const QSizeF &content, ScaleMode mode);

// A picture, video or sound used by a slide, and where its bytes are.
struct Media
{
    enum Kind { Image, Video, Audio };

    Kind kind = Image;
    // The file on this machine; empty if it could not be found, or the bytes are in hand
    QString path;
    // Bytes made here (a video's still, a picture made for an effect PowerPoint has
    // not got), with a name that tells one such apart from another
    QByteArray data;
    QString dataId;
    // The file's own name, as ProPresenter knew it ("Background.png")
    QString fileName;
    // Its size in pixels, when known
    QSizeF naturalSize;
    // A video's still (a PNG), shown before it plays and wherever it cannot be: made
    // before the deck is written (pptximport)
    QByteArray still;
    // A video's length in seconds, when known: PowerPoint wants it to start a video by
    // itself
    double duration = 0;

    bool isMissing() const { return path.isEmpty() && data.isEmpty(); }
    // The file's extension, lower case ("png")
    QString fileExtension() const;
    // What tells one medium from another, for putting each into the file once
    QString key() const;
    // A video's still as a picture of its own (null if it has none)
    std::optional<Media> stillPicture() const;

    bool operator==(const Media &other) const = default;
};

struct Gradient
{
    struct Stop
    {
        QColor color;
        // 0...1 along the gradient
        double position = 0;

        bool operator==(const Stop &other) const = default;
    };

    // Linear, at `angle` (degrees, clockwise from the right, as PowerPoint counts), or
    // radial
    bool radial = false;
    double angle = 0;
    QList<Stop> stops;

    bool operator==(const Gradient &other) const = default;
};

// A shape's fill: a colour, a gradient, or a picture (a picture fill keeps how it is
// fitted, for the importer; PowerPoint's stretches it to the shape)
struct PictureFill
{
    Media media;
    ScaleMode scale = ScaleMode::Stretch;

    bool operator==(const PictureFill &other) const = default;
};
using Fill = std::variant<QColor, Gradient, PictureFill>;

struct Line
{
    double width = 0;
    QColor color;
    // Dash and gap lengths as multiples of the width; empty for a solid line
    QList<double> dashes;

    bool operator==(const Line &other) const = default;
};

struct Shadow
{
    QColor color;
    // The direction the shadow falls, in degrees, clockwise from the right (so 45 is
    // down and to the right, as PowerPoint counts)
    double direction = 0;
    double distance = 0;
    double blurRadius = 0;

    bool operator==(const Shadow &other) const = default;
};

// What fills the letters: a colour, a gradient, or a picture (the stand-in for a blur
// or invert of what is behind them, which PowerPoint cannot do: see pptximport.h)
using TextFill = std::variant<QColor, Gradient, Media>;

struct RunStyle
{
    // (TitleCase, each word's first letter a capital, is ProPresenter's, which PowerPoint
    // has no setting for: the importer puts it into the words, and the writer never sees it)
    enum Capitalization { NoCapitalization, AllCaps, SmallCaps, TitleCase };

    // The PostScript name of the font ("HelveticaNeue-Bold"), as the presentation names it
    QString fontName = QStringLiteral("Helvetica");
    // The family PowerPoint is to find it by (pptx::officeFont), when known
    QString fontFamily = QStringLiteral("Helvetica");
    double size = 12;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    std::optional<QColor> underlineColor;
    bool strikethrough = false;
    TextFill fill = QColor(Qt::black);
    // An outline drawn round each letter
    std::optional<Line> outline;
    std::optional<Shadow> shadow;
    // Extra space after each character (negative to tighten)
    double characterSpacing = 0;
    // Raised (positive) or lowered (negative) from the baseline
    double baselineOffset = 0;
    // 1 for superscript, -1 for subscript, 0 for neither
    int superscript = 0;
    Capitalization capitalization = NoCapitalization;
    // A highlight behind the letters
    std::optional<QColor> highlight;

    bool operator==(const RunStyle &other) const = default;
};

struct Run
{
    QString text;
    RunStyle style;
};

struct Paragraph
{
    enum Alignment { Left, Center, Right, Justified };

    struct LineSpacing
    {
        enum Kind {
            // A multiple of the font's own line height (1 is single). As ProPresenter
            // lays it out: the lines are that far apart, and the extra space is below
            // each line, the last one included, not above it.
            Multiple,
            // A fixed line height
            Exactly,
            // At least this line height
            AtLeast,
        };
        Kind kind = Multiple;
        double value = 1;
    };

    struct Bullet
    {
        // A character ("•"), or a number in a style PowerPoint knows ("arabicPeriod",
        // "alphaLcParenR", ...), starting at `startAt`
        QString character;
        QString numberScheme;
        int startAt = 1;
    };

    Alignment alignment = Left;
    std::optional<LineSpacing> lineSpacing;
    // Extra space between lines beyond `lineSpacing` (macOS's lineSpacing; PowerPoint
    // has no such thing and the writer folds it in)
    double extraLeading = 0;
    double spaceBefore = 0;
    double spaceAfter = 0;
    // Where wrapped lines start, from the box's left inset
    double leftIndent = 0;
    // Where the first line starts, relative to `leftIndent` (negative for a hanging indent)
    double firstLineIndent = 0;
    double rightIndent = 0;
    std::optional<Bullet> bullet;
    // Tab stops, from the left inset
    QList<double> tabStops;
    // The distance between tab stops after the last of `tabStops`; 0 for the default of
    // whatever draws it
    double defaultTabInterval = 0;
    // The runs of text. A line break inside a paragraph is U+2028 in a run's text.
    QList<Run> runs;
    // The style of the paragraph's end, which gives an empty paragraph its height
    RunStyle endStyle;
};

struct TextBody
{
    enum VerticalAlignment { Top, Middle, Bottom };
    enum Autofit {
        // The text keeps its size and may run out of the box
        NoAutofit,
        // The text is drawn smaller to fit the box, by `fontScale`; PowerPoint's "Shrink
        // text on overflow"
        Shrink,
        // The box grows to fit the text; PowerPoint's "Resize shape to fit text"
        ResizeShape,
    };

    QList<Paragraph> paragraphs;
    QMarginsF insets;
    VerticalAlignment verticalAlignment = Top;
    Autofit autofit = NoAutofit;
    double fontScale = 1;
    // Whether lines wrap at the box's edge
    bool wraps = true;

    // The words, with paragraphs on lines of their own
    QString plainText() const;
};

struct Shape
{
    enum Geometry { Rectangle, RoundedRectangle, Ellipse, Polygon };

    QString name;
    QRectF frame;
    // Degrees, clockwise, about the centre
    double rotation = 0;
    bool flipHorizontal = false;
    bool flipVertical = false;
    Geometry geometry = Rectangle;
    // Corner radius as a fraction of the shorter side (0...0.5), for a rounded rectangle
    double cornerFraction = 0;
    // A closed path of points in 0...1 of the frame (straight lines between them)
    QList<QPointF> points;
    std::optional<Fill> fill;
    std::optional<Line> line;
    std::optional<Shadow> shadow;
    // The width of a soft edge; 0 for none
    double softEdge = 0;
    // 0...1, applied to the whole shape
    double opacity = 1;
    std::optional<TextBody> text;
};

struct Picture
{
    QString name;
    QRectF frame;
    double rotation = 0;
    Media media;
    ScaleMode scale = ScaleMode::Stretch;
    // Fractions cut from each side of the picture before it is fitted (0...1)
    QMarginsF crop;
    double opacity = 1;
    bool flipHorizontal = false;
    bool flipVertical = false;
};

struct Video
{
    QString name;
    QRectF frame;
    Media media;
    ScaleMode scale = ScaleMode::Fit;
    bool loops = false;
    // Starts by itself when the slide comes up, as it would in ProPresenter
    bool playsAutomatically = true;
    bool muted = false;
};

using Item = std::variant<Shape, Picture, Video>;

// The media behind a slide, and why it is there
struct BackgroundMedia
{
    Media media;
    ScaleMode scale = ScaleMode::Fit;
    // True when this slide had no media cue of its own and the media is the one left on
    // screen by an earlier slide (the "keep background media" option)
    bool isCarriedOver = false;
};

struct Slide
{
    // The colour under everything, if the slide has one of its own
    std::optional<QColor> backgroundColor;
    // What fills the slide from behind: the media ProPresenter would have on its
    // background layer while this slide is up
    std::optional<BackgroundMedia> background;
    // What is drawn on the slide, back to front
    QList<Item> items;
    // The speaker notes, as plain text
    QString notes;
};

struct Section
{
    QString name;
    int firstSlide = 0;
};

struct Deck
{
    // The size of every slide (1920 x 1080 for a 1080p presentation)
    QSizeF size = QSizeF(1920, 1080);
    // The title written into the file's properties
    QString title;
    QList<Slide> slides;
    // PowerPoint's sections: a name and the slide each starts at, where ProPresenter's
    // groups go
    QList<Section> sections;
};

// Every choice the person can make about an export, as ProToPPTX has them (its
// ConversionOptions and docs/OPTIONS.md), with its defaults: slide for slide as
// ProPresenter shows them, media cues kept, and background media carried on to the
// slides after it, as ProPresenter does when they are clicked through in order.
struct Options
{
    enum BackgroundPlacement {
        // As the slide's own background (Format Background): it cannot be clicked on or
        // moved by mistake while the text is edited. Videos cannot go there and are
        // always put in as a picture at the back.
        SlideBackground,
        // As a picture at the back of the slide, which can be selected, moved and
        // replaced like anything else
        PictureAtBack,
    };
    enum VideoHandling {
        // The video file is put into the .pptx and plays when the slide comes up
        Embed,
        // Only a still from the video is put in: a smaller file
        StillOnly,
        // Left out altogether
        LeaveOut,
    };

    // Keep background media on following slides that have no media cue of their own
    bool carryBackgroundForward = true;
    BackgroundPlacement backgroundPlacement = SlideBackground;
    VideoHandling videos = Embed;
    // Leave out slides that are turned off in ProPresenter (they cannot be shown there)
    bool skipDisabledSlides = true;
    // Put each slide's notes into PowerPoint's speaker notes
    bool includeNotes = true;
    // Make ProPresenter's groups into PowerPoint sections
    bool groupsAsSections = true;
    // Put the slides in the chosen arrangement's order (slides repeated as often as it
    // repeats their group); otherwise every group once, in the order the presentation
    // lists them
    bool followArrangement = true;
};

// Something the person should know about an export: what could not be carried over, or
// was carried over by a guess, and on which slides. One note is made of many: "The font
// X is not installed" once, with every slide it is on.
struct Note
{
    enum Kind { MissingMedia, MissingFont, Macro, Audio, Simplified };

    Kind kind = Simplified;
    QString message;
    // Slide numbers, from 1, in the deck as written
    QList<int> slides;

    // "slide 3", "slides 2, 5 and 9"
    QString slidesText() const;
};

// Gathers notes, merging those that say the same thing.
class NoteCollector
{
public:
    void add(Note::Kind kind, const QString &message, int slide = 0);
    const QList<Note> &all() const { return m_notes; }

private:
    QList<Note> m_notes;
};

} // namespace pptx
