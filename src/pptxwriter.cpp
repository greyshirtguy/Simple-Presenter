#include "pptxwriter.h"

#include "pptxtext.h"
#include "pptxzip.h"

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace pptx {

namespace {

// ---------------------------------------------------------------------------------------
// XML: the little needed to write Office Open XML as text.

const QString xmlHeader = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
const QString nsA = QStringLiteral("http://schemas.openxmlformats.org/drawingml/2006/main");
const QString nsR = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");
const QString nsP = QStringLiteral("http://schemas.openxmlformats.org/presentationml/2006/main");
const QString nsP14 = QStringLiteral("http://schemas.microsoft.com/office/powerpoint/2010/main");
const QString nsRelationships = QStringLiteral("http://schemas.openxmlformats.org/package/2006/relationships");

// `xmlns:a="…" xmlns:r="…" xmlns:p="…"`, for a part's root element
QString presentationNamespaces()
{
    return QStringLiteral("xmlns:a=\"%1\" xmlns:r=\"%2\" xmlns:p=\"%3\"").arg(nsA, nsR, nsP);
}

// Text made safe for an attribute or an element's content
QString esc(const QString &s)
{
    QString out;
    out.reserve(s.size());
    for (int k = 0; k < s.size(); ++k) {
        const QChar c = s.at(k);
        switch (c.unicode()) {
        case '&':
            out += QStringLiteral("&amp;");
            break;
        case '<':
            out += QStringLiteral("&lt;");
            break;
        case '>':
            out += QStringLiteral("&gt;");
            break;
        case '"':
            out += QStringLiteral("&quot;");
            break;
        case '\t':
        case '\n':
        case '\r':
            out += c;
            break;
        default:
            // Not allowed in XML 1.0 at all: the other control characters, a surrogate
            // without its pair, and U+FFFE and U+FFFF
            if (c.unicode() < 0x20 || c.unicode() == 0xFFFE || c.unicode() == 0xFFFF)
                break;
            if (c.isHighSurrogate() && k + 1 < s.size() && s.at(k + 1).isLowSurrogate()) {
                out += c;
                out += s.at(++k);
                break;
            }
            if (c.isSurrogate())
                break;
            out += c;
        }
    }
    return out;
}

struct Relationship
{
    QString id;
    QString type;
    QString target;
};

QString relationshipsPart(const QList<Relationship> &list)
{
    QString s = xmlHeader + QStringLiteral("<Relationships xmlns=\"%1\">").arg(nsRelationships);
    for (const Relationship &rel : list)
        s += QStringLiteral("<Relationship Id=\"%1\" Type=\"%2\" Target=\"%3\"/>").arg(rel.id, rel.type, esc(rel.target));
    return s + QStringLiteral("</Relationships>");
}

namespace RelType {
const QString officeDocument = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument");
const QString coreProperties = QStringLiteral("http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties");
const QString extendedProperties = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties");
const QString slideMaster = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster");
const QString slideLayout = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout");
const QString slide = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide");
const QString notesMaster = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/notesMaster");
const QString notesSlide = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/notesSlide");
const QString theme = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme");
const QString presProps = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/presProps");
const QString viewProps = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/viewProps");
const QString tableStyles = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/tableStyles");
const QString image = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/image");
const QString video = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/video");
const QString media = QStringLiteral("http://schemas.microsoft.com/office/2007/relationships/media");
} // namespace RelType

// ---------------------------------------------------------------------------------------
// Units: the deck's slide units turned into PowerPoint's.
//
// PowerPoint measures in EMUs (914400 to the inch, 12700 to the point) and font sizes in
// hundredths of a point. The PowerPoint slide is made 7.5 inches tall, the height of
// PowerPoint's own standard slides, and as wide as the presentation's shape asks: a 1920
// x 1080 presentation becomes PowerPoint's standard widescreen slide, 13.333 x 7.5
// inches, and a 4:3 one its standard 10 x 7.5. Everything is scaled by the same factor,
// so a ProPresenter font size of 72 on a 1080p slide becomes 36 points, which is the
// same size on the slide.
struct Units
{
    static constexpr double slideHeightEMU = 6858000;
    double emuPerUnit = 1;

    explicit Units(const QSizeF &deckSize) : emuPerUnit(slideHeightEMU / std::max(deckSize.height(), 1.0)) {}

    qint64 emu(double units) const { return qint64(std::llround(units * emuPerUnit)); }
    // Points, for a length in slide units
    double points(double units) const { return units * emuPerUnit / 12700; }
    // Hundredths of a point (font sizes, character spacing, spacing in points)
    qint64 centipoints(double units) const { return qint64(std::llround(points(units) * 100)); }
    // A font size as PowerPoint takes it: hundredths of a point, 1 to 4000 points
    qint64 fontSize(double units) const { return std::clamp<qint64>(centipoints(units), 100, 400000); }
    // Degrees in PowerPoint's 60000ths
    static qint64 angle(double degrees)
    {
        double d = std::fmod(degrees, 360);
        if (d < 0)
            d += 360;
        return std::llround(d * 60000) % 21600000;
    }
    // A fraction as PowerPoint's thousandths of a percent (1 is 100000)
    static qint64 percent(double fraction) { return std::llround(fraction * 100000); }
};

QString number(qint64 v)
{
    return QString::number(v);
}

// "RRGGBB", as PowerPoint writes a colour
QString hex(const QColor &colour)
{
    const QColor c = colour.toRgb();
    return QString::asprintf("%02X%02X%02X", c.red(), c.green(), c.blue());
}

// ---------------------------------------------------------------------------------------
// MediaRegistry: the media files of a deck, each put into the .pptx once.
//
// A background carried over to twenty slides is one picture in the file, which twenty
// slides point to: the registry gives each medium its part name ("image3.png") the first
// time a slide asks for it, and the same name every time after.
//
// Pictures are looked at when first asked for, to learn their size in pixels (the writer
// needs it to fit a picture as ProPresenter would) and that they can be read at all.
// Pictures in a format PowerPoint may not show (WebP, HEIC, anything that is not PNG,
// JPEG, GIF, BMP or TIFF) are turned into PNG. Videos and sounds are not read until the
// file is written, and are put in as they are, copied in pieces.

struct MediaPart
{
    // The part's file name in ppt/media
    QString name;
    QSizeF pixelSize;
};

class MediaRegistry
{
public:
    std::optional<MediaPart> part(const Media &media)
    {
        const QString key = media.key() + u'|' + media.fileName;
        const auto found = m_byKey.constFind(key);
        if (found != m_byKey.constEnd())
            return found->part;
        if (media.isMissing())
            return std::nullopt;
        Entry entry;
        entry.path = media.path;
        entry.data = media.data;
        if (media.kind == Media::Image) {
            QBuffer buffer;
            QImageReader reader;
            if (!media.data.isEmpty()) {
                buffer.setData(media.data);
                buffer.open(QIODevice::ReadOnly);
                reader.setDevice(&buffer);
            } else {
                reader.setFileName(media.path);
            }
            if (!reader.canRead())
                return std::nullopt;
            QSize size = reader.size();
            // A picture taken on its side says so, and is shown turned.
            if (reader.transformation() & QImageIOHandler::TransformationRotate90)
                size.transpose();
            QString ext = media.data.isEmpty() ? media.fileExtension() : QStringLiteral("png");
            static const QSet<QString> pictureTypes = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                                       QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("tif"),
                                                       QStringLiteral("tiff")};
            if (!pictureTypes.contains(ext)) {
                reader.setAutoTransform(true);
                const QImage image = reader.read();
                if (image.isNull())
                    return std::nullopt;
                QBuffer png(&entry.data);
                png.open(QIODevice::WriteOnly);
                image.save(&png, "PNG");
                entry.path.clear();
                ext = QStringLiteral("png");
                size = image.size();
            }
            if (ext == u"jpeg")
                ext = QStringLiteral("jpg");
            if (ext == u"tif")
                ext = QStringLiteral("tiff");
            ++m_images;
            entry.part = MediaPart {QStringLiteral("image%1.%2").arg(m_images).arg(ext), QSizeF(size)};
        } else {
            ++m_media;
            QString ext = media.fileExtension();
            if (ext.isEmpty())
                ext = media.kind == Media::Video ? QStringLiteral("mov") : QStringLiteral("m4a");
            entry.part = MediaPart {QStringLiteral("media%1.%2").arg(m_media).arg(ext), media.naturalSize};
        }
        m_byKey.insert(key, entry);
        m_order << key;
        return entry.part;
    }

    int count() const { return int(m_order.size()); }

    // The extensions used, for [Content_Types].xml
    QStringList extensions() const
    {
        QSet<QString> all;
        for (const QString &key : m_order)
            all.insert(QFileInfo(m_byKey.value(key).part.name).suffix().toLower());
        QStringList sorted(all.begin(), all.end());
        sorted.sort();
        return sorted;
    }

    // Writes every medium into the archive, as it is (they are compressed already).
    bool write(PptxZipWriter *zip, QString *error, const std::function<bool()> &step)
    {
        for (const QString &key : std::as_const(m_order)) {
            const Entry entry = m_byKey.value(key);
            const QString target = QStringLiteral("ppt/media/") + entry.part.name;
            const bool written = entry.data.isEmpty() ? zip->addFile(target, entry.path, error)
                                                      : zip->add(target, entry.data, PptxZipWriter::Store, error);
            if (!written || !step())
                return false;
        }
        return true;
    }

    static QString contentType(const QString &ext)
    {
        static const QHash<QString, QString> types = {
            {QStringLiteral("png"), QStringLiteral("image/png")},
            {QStringLiteral("jpg"), QStringLiteral("image/jpeg")},
            {QStringLiteral("jpeg"), QStringLiteral("image/jpeg")},
            {QStringLiteral("gif"), QStringLiteral("image/gif")},
            {QStringLiteral("bmp"), QStringLiteral("image/bmp")},
            {QStringLiteral("tif"), QStringLiteral("image/tiff")},
            {QStringLiteral("tiff"), QStringLiteral("image/tiff")},
            {QStringLiteral("mov"), QStringLiteral("video/quicktime")},
            {QStringLiteral("mp4"), QStringLiteral("video/mp4")},
            {QStringLiteral("m4v"), QStringLiteral("video/mp4")},
            {QStringLiteral("wmv"), QStringLiteral("video/x-ms-wmv")},
            {QStringLiteral("avi"), QStringLiteral("video/x-msvideo")},
            {QStringLiteral("webm"), QStringLiteral("video/webm")},
            {QStringLiteral("mkv"), QStringLiteral("video/x-matroska")},
            {QStringLiteral("mpg"), QStringLiteral("video/mpeg")},
            {QStringLiteral("mpeg"), QStringLiteral("video/mpeg")},
            {QStringLiteral("mp3"), QStringLiteral("audio/mpeg")},
            {QStringLiteral("m4a"), QStringLiteral("audio/mp4")},
            {QStringLiteral("wav"), QStringLiteral("audio/wav")},
        };
        return types.value(ext, QStringLiteral("application/octet-stream"));
    }

private:
    struct Entry
    {
        MediaPart part;
        // The file, or the bytes in hand (made here, or a picture turned into PNG)
        QString path;
        QByteArray data;
    };

    QHash<QString, Entry> m_byKey;
    QStringList m_order;
    int m_images = 0;
    int m_media = 0;
};

// ---------------------------------------------------------------------------------------
// TextFit: where PowerPoint will put a text box's lines, and what to write so that it puts
// them where ProPresenter does.
//
// What ProToPPTX measured (2026-10-18), with a deck of text boxes in four fonts, two
// sizes, four line spacings and two vertical alignments, drawn by PowerPoint 16 for the
// Mac and by its own drawing (which lays text out as ProPresenter does, pixel for pixel
// against ProPresenter's exported slides):
//
// - ProPresenter spaces lines by each font's own line height (1.2 x the size for
//   Helvetica, 1.22 for CMG Sans, 1.14 for Georgia), times the paragraph's multiple, with
//   the extra space below each line; the first baseline is the font's own distance down.
// - PowerPoint, given its line spacing as a percentage, spaces lines 1.2 x the size x the
//   percentage, whatever the font, and puts the first baseline 0.9 x the size x the
//   percentage down. (Given none, it uses the font's metrics in a way that differs from
//   macOS's by a few pixels either way; so a percentage is always written.) Centred or
//   bottom-aligned, the block it centres is its lines' spacing added up, less 0.4 x the
//   size x (the percentage - 1) when that is more than nothing: found by fitting, true to
//   within 4 pixels of a 1080-pixel slide over the fonts and sizes tried.
//
// So each paragraph's percentage is chosen to give ProPresenter's distance between its
// lines exactly, and the difference that remains, where the first line starts, is made up
// by moving the text area: the top inset grows by it and the bottom one shrinks by it, so
// the area keeps its height and the box keeps its place, its size and its alignment for
// whoever edits it in PowerPoint. (The difference can be more than the inset; PowerPoint
// takes an inset less than nought, and keeps it.)
//
// Here ProPresenter's lines are measured with this app's layout of them (pptxtext.h), not
// macOS's: the fonts' metrics come from the same files, but this has not been measured
// against ProPresenter's pictures as ProToPPTX's was.
struct TextFit
{
    // For each paragraph, PowerPoint's line spacing as a fraction of its "single" (1.2 x
    // the font size)
    QList<double> spacing;
    // How far to move the text down (up, if less than nought)
    double shift = 0;

    TextFit(const TextBody &body, const QRectF &frame)
    {
        const double fontScale = body.autofit == TextBody::Shrink ? body.fontScale : 1;
        for (const Paragraph &p : body.paragraphs) {
            const RunStyle *largest = &p.endStyle;
            for (const Run &run : p.runs) {
                if (largest == &p.endStyle || run.style.size > largest->size)
                    largest = &run.style;
            }
            const double size = std::max(largest->size, 0.1);
            const double natural = lineHeightRatio(*largest) * size;
            double pitch = natural;
            if (p.lineSpacing) {
                switch (p.lineSpacing->kind) {
                case Paragraph::LineSpacing::Multiple:
                    pitch = p.lineSpacing->value * natural;
                    break;
                case Paragraph::LineSpacing::Exactly:
                    pitch = p.lineSpacing->value;
                    break;
                case Paragraph::LineSpacing::AtLeast:
                    pitch = std::max(p.lineSpacing->value, natural);
                    break;
                }
            }
            pitch += p.extraLeading;
            spacing << pitch / (1.2 * size);
        }

        const double width = frame.width() - body.insets.left() - body.insets.right();
        const double available = frame.height() - body.insets.top() - body.insets.bottom();
        const LaidText laid = layoutLines(body, width, fontScale);
        if (laid.lines.isEmpty())
            return;
        const LaidLine &first = laid.lines.first();
        const LaidLine &last = laid.lines.last();

        // Where ProPresenter puts the first baseline, from the top of the text area
        double pro = first.baseline;
        if (body.verticalAlignment == TextBody::Middle)
            pro = (available - laid.height) / 2 + first.baseline;
        else if (body.verticalAlignment == TextBody::Bottom)
            pro = available - laid.height + first.baseline;

        // Where PowerPoint will
        const auto m = [this](const LaidLine &line) { return spacing.value(line.paragraph, 1); };
        const double top = 0.9 * m(first) * first.largestSize;
        double height = 0;
        for (const LaidLine &line : laid.lines)
            height += 1.2 * m(line) * line.largestSize;
        height -= 0.4 * std::max(0.0, m(last) - 1) * last.largestSize;
        for (int k = 0; k < body.paragraphs.size(); ++k) {
            if (k > 0)
                height += body.paragraphs.at(k).spaceBefore;
            if (k < body.paragraphs.size() - 1)
                height += body.paragraphs.at(k).spaceAfter;
        }
        double powerPoint = top;
        if (body.verticalAlignment == TextBody::Middle)
            powerPoint = (available - height) / 2 + top;
        else if (body.verticalAlignment == TextBody::Bottom)
            powerPoint = available - height + top;
        shift = pro - powerPoint;
    }
};

// ---------------------------------------------------------------------------------------
// FixedParts: the parts every .pptx written here has, whatever is on its slides.
//
// A presentation PowerPoint will open without offering to "repair" it needs more than its
// slides: a slide master, a layout for the slides to be "of", a theme for the master, the
// presentation's own properties, and the package's list of what each part is. These are
// as small as PowerPoint accepts, and plain (ProToPPTX's, as they are): one master with
// no placeholders, one blank layout, the Office theme's colours with Helvetica Neue as
// its fonts. The notes master and its theme are written only when a slide has notes.

QString themePart(const QString &name)
{
    return xmlHeader + QStringLiteral(R"X(<a:theme xmlns:a="%1" name="%2"><a:themeElements>)X").arg(nsA, esc(name))
           + QStringLiteral(R"X(<a:clrScheme name="Office"><a:dk1><a:sysClr val="windowText" lastClr="000000"/></a:dk1><a:lt1><a:sysClr val="window" lastClr="FFFFFF"/></a:lt1>)X"
                            R"X(<a:dk2><a:srgbClr val="0E2841"/></a:dk2><a:lt2><a:srgbClr val="E8E8E8"/></a:lt2><a:accent1><a:srgbClr val="156082"/></a:accent1>)X"
                            R"X(<a:accent2><a:srgbClr val="E97132"/></a:accent2><a:accent3><a:srgbClr val="196B24"/></a:accent3><a:accent4><a:srgbClr val="0F9ED5"/></a:accent4>)X"
                            R"X(<a:accent5><a:srgbClr val="A02B93"/></a:accent5><a:accent6><a:srgbClr val="4EA72E"/></a:accent6><a:hlink><a:srgbClr val="467886"/></a:hlink>)X"
                            R"X(<a:folHlink><a:srgbClr val="96607D"/></a:folHlink></a:clrScheme>)X"
                            R"X(<a:fontScheme name="Simple Presenter"><a:majorFont><a:latin typeface="Helvetica Neue"/><a:ea typeface=""/><a:cs typeface=""/></a:majorFont>)X"
                            R"X(<a:minorFont><a:latin typeface="Helvetica Neue"/><a:ea typeface=""/><a:cs typeface=""/></a:minorFont></a:fontScheme>)X"
                            R"X(<a:fmtScheme name="Office"><a:fillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill>)X"
                            R"X(<a:solidFill><a:schemeClr val="phClr"><a:tint val="50000"/></a:schemeClr></a:solidFill>)X"
                            R"X(<a:solidFill><a:schemeClr val="phClr"><a:shade val="80000"/></a:schemeClr></a:solidFill></a:fillStyleLst>)X"
                            R"X(<a:lnStyleLst><a:ln w="12700" cap="flat" cmpd="sng" algn="ctr"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:prstDash val="solid"/><a:miter lim="800000"/></a:ln>)X"
                            R"X(<a:ln w="19050" cap="flat" cmpd="sng" algn="ctr"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:prstDash val="solid"/><a:miter lim="800000"/></a:ln>)X"
                            R"X(<a:ln w="25400" cap="flat" cmpd="sng" algn="ctr"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:prstDash val="solid"/><a:miter lim="800000"/></a:ln></a:lnStyleLst>)X"
                            R"X(<a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle><a:effectStyle><a:effectLst/></a:effectStyle>)X"
                            R"X(<a:effectStyle><a:effectLst><a:outerShdw blurRad="57150" dist="19050" dir="5400000" algn="ctr" rotWithShape="0"><a:srgbClr val="000000"><a:alpha val="63000"/></a:srgbClr></a:outerShdw></a:effectLst></a:effectStyle></a:effectStyleLst>)X"
                            R"X(<a:bgFillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:solidFill><a:schemeClr val="phClr"><a:tint val="95000"/></a:schemeClr></a:solidFill>)X"
                            R"X(<a:solidFill><a:schemeClr val="phClr"><a:shade val="80000"/></a:schemeClr></a:solidFill></a:bgFillStyleLst></a:fmtScheme>)X"
                            R"X(</a:themeElements><a:objectDefaults/><a:extraClrSchemeLst/></a:theme>)X");
}

const QString groupShapeStart = QStringLiteral(R"X(<p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>)X"
                                               R"X(<p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>)X");
const QString colourMap = QStringLiteral(R"X(<p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/>)X");

QString slideMasterPart()
{
    return xmlHeader + QStringLiteral("<p:sldMaster %1>").arg(presentationNamespaces())
           + QStringLiteral(R"X(<p:cSld><p:bg><p:bgRef idx="1001"><a:schemeClr val="bg1"/></p:bgRef></p:bg><p:spTree>)X") + groupShapeStart
           + QStringLiteral("</p:spTree></p:cSld>") + colourMap
           + QStringLiteral(R"X(<p:sldLayoutIdLst><p:sldLayoutId id="2147483649" r:id="rId1"/></p:sldLayoutIdLst>)X"
                            R"X(<p:txStyles><p:titleStyle><a:lvl1pPr><a:defRPr sz="4400"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mj-lt"/></a:defRPr></a:lvl1pPr></p:titleStyle>)X"
                            R"X(<p:bodyStyle><a:lvl1pPr><a:defRPr sz="2800"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mn-lt"/></a:defRPr></a:lvl1pPr></p:bodyStyle>)X"
                            R"X(<p:otherStyle><a:lvl1pPr><a:defRPr sz="1800"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mn-lt"/></a:defRPr></a:lvl1pPr></p:otherStyle></p:txStyles></p:sldMaster>)X");
}

QString slideLayoutPart()
{
    return xmlHeader + QStringLiteral(R"X(<p:sldLayout %1 type="blank" preserve="1"><p:cSld name="Blank"><p:spTree>)X").arg(presentationNamespaces())
           + groupShapeStart + QStringLiteral("</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sldLayout>");
}

// The notes page: the slide's picture above, its notes below, on a page 7.5 x 10 inches
QString notesMasterPart()
{
    return xmlHeader + QStringLiteral("<p:notesMaster %1>").arg(presentationNamespaces())
           + QStringLiteral(R"X(<p:cSld><p:bg><p:bgRef idx="1001"><a:schemeClr val="bg1"/></p:bgRef></p:bg><p:spTree>)X") + groupShapeStart
           + QStringLiteral(R"X(<p:sp><p:nvSpPr><p:cNvPr id="2" name="Slide Image Placeholder 1"/><p:cNvSpPr><a:spLocks noGrp="1" noRot="1" noChangeAspect="1"/></p:cNvSpPr><p:nvPr><p:ph type="sldImg" idx="2"/></p:nvPr></p:nvSpPr>)X"
                            R"X(<p:spPr><a:xfrm><a:off x="685800" y="1143000"/><a:ext cx="5486400" cy="3086100"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom><a:noFill/><a:ln w="12700"><a:solidFill><a:prstClr val="black"/></a:solidFill></a:ln></p:spPr></p:sp>)X"
                            R"X(<p:sp><p:nvSpPr><p:cNvPr id="3" name="Notes Placeholder 2"/><p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr><p:nvPr><p:ph type="body" sz="quarter" idx="3"/></p:nvPr></p:nvSpPr>)X"
                            R"X(<p:spPr><a:xfrm><a:off x="685800" y="4400550"/><a:ext cx="5486400" cy="3600450"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></p:spPr>)X"
                            R"X(<p:txBody><a:bodyPr vert="horz" lIns="91440" tIns="45720" rIns="91440" bIns="45720" rtlCol="0"/><a:lstStyle/><a:p><a:endParaRPr lang="en-US"/></a:p></p:txBody></p:sp>)X"
                            R"X(</p:spTree></p:cSld>)X")
           + colourMap
           + QStringLiteral(R"X(<p:notesStyle><a:lvl1pPr marL="0" algn="l" defTabSz="914400" rtl="0" eaLnBrk="1" latinLnBrk="0" hangingPunct="1"><a:defRPr sz="1200" kern="1200"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mn-lt"/><a:ea typeface="+mn-ea"/><a:cs typeface="+mn-cs"/></a:defRPr></a:lvl1pPr></p:notesStyle></p:notesMaster>)X");
}

QString notesSlidePart(const QString &text, const QString &language)
{
    QString paragraphs;
    for (const QString &line : text.split(u'\n')) {
        paragraphs += line.isEmpty() ? QStringLiteral(R"X(<a:p><a:endParaRPr lang="%1" dirty="0"/></a:p>)X").arg(language)
                                     : QStringLiteral(R"X(<a:p><a:r><a:rPr lang="%1" dirty="0"/><a:t>%2</a:t></a:r></a:p>)X").arg(language, esc(line));
    }
    return xmlHeader + QStringLiteral("<p:notes %1><p:cSld><p:spTree>").arg(presentationNamespaces()) + groupShapeStart
           + QStringLiteral(R"X(<p:sp><p:nvSpPr><p:cNvPr id="2" name="Slide Image Placeholder 1"/><p:cNvSpPr><a:spLocks noGrp="1" noRot="1" noChangeAspect="1"/></p:cNvSpPr><p:nvPr><p:ph type="sldImg"/></p:nvPr></p:nvSpPr><p:spPr/></p:sp>)X"
                            R"X(<p:sp><p:nvSpPr><p:cNvPr id="3" name="Notes Placeholder 2"/><p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr><p:nvPr><p:ph type="body" idx="1"/></p:nvPr></p:nvSpPr><p:spPr/>)X"
                            R"X(<p:txBody><a:bodyPr/><a:lstStyle/>)X")
           + paragraphs + QStringLiteral("</p:txBody></p:sp></p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:notes>");
}

QString presPropsPart()
{
    return xmlHeader + QStringLiteral("<p:presentationPr %1/>").arg(presentationNamespaces());
}

QString viewPropsPart()
{
    return xmlHeader + QStringLiteral("<p:viewPr %1>").arg(presentationNamespaces())
           + QStringLiteral(R"X(<p:normalViewPr><p:restoredLeft sz="15620"/><p:restoredTop sz="94660"/></p:normalViewPr>)X"
                            R"X(<p:slideViewPr><p:cSldViewPr snapToGrid="0"><p:cViewPr varScale="1"><p:scale><a:sx n="100" d="100"/><a:sy n="100" d="100"/></p:scale><p:origin x="0" y="0"/></p:cViewPr><p:guideLst/></p:cSldViewPr></p:slideViewPr>)X"
                            R"X(<p:gridSpacing cx="76200" cy="76200"/></p:viewPr>)X");
}

QString tableStylesPart()
{
    return xmlHeader + QStringLiteral(R"X(<a:tblStyleLst xmlns:a="%1" def="{5C22544A-7EE6-4342-B048-85BDC9FD1C3A}"/>)X").arg(nsA);
}

QString corePropertiesPart(const QString &title)
{
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return xmlHeader
           + QStringLiteral(R"X(<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" xmlns:dc="http://purl.org/dc/elements/1.1/" )X"
                            R"X(xmlns:dcterms="http://purl.org/dc/terms/" xmlns:dcmitype="http://purl.org/dc/dcmitype/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">)X"
                            R"X(<dc:title>%1</dc:title><dc:creator>Simple Presenter</dc:creator>)X"
                            R"X(<dcterms:created xsi:type="dcterms:W3CDTF">%2</dcterms:created><dcterms:modified xsi:type="dcterms:W3CDTF">%2</dcterms:modified></cp:coreProperties>)X")
                 .arg(esc(title), now);
}

QString appPropertiesPart(int slides)
{
    return xmlHeader
           + QStringLiteral(R"X(<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties" xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes">)X"
                            R"X(<Application>Simple Presenter</Application><Slides>%1</Slides><PresentationFormat>Custom</PresentationFormat></Properties>)X")
                 .arg(slides);
}

// ---------------------------------------------------------------------------------------
// SlideXml: one deck slide as a PowerPoint slide part.
//
// What goes where, back to front:
//
//   - The slide's background (p:bg): its colour, or, with the "as the slide's background"
//     option, its background picture, where it cannot be clicked on while the words are
//     edited. A picture whose shape is not the slide's, set to fit, would be stretched
//     there, so it goes in as a picture at the back instead (as do videos, which a
//     background cannot hold).
//   - Each item as a shape (p:sp: shapes and text boxes), a picture (p:pic), or a video
//     (a p:pic with the video attached, its still as the picture).
//
// Text: every run carries its own font, size, colour and the rest, so nothing depends on
// the theme. Where ProPresenter and PowerPoint lay text out differently, it is made up
// here (TextFit above).
class SlideXml
{
public:
    SlideXml(const Units &units, const QSizeF &deckSize, const Options &options, const QString &language,
             MediaRegistry *media, NoteCollector *notes, int slideNumber)
        : m_units(units), m_deckSize(deckSize), m_options(options), m_language(language), m_media(media),
          m_notes(notes), m_slideNumber(slideNumber)
    {
        m_relationships << Relationship {QStringLiteral("rId1"), RelType::slideLayout, QStringLiteral("../slideLayouts/slideLayout1.xml")};
    }

    QList<Relationship> relationships() const { return m_relationships; }

    QString addRelationship(const QString &type, const QString &target)
    {
        const QString id = QStringLiteral("rId%1").arg(m_relationships.size() + 1);
        m_relationships << Relationship {id, type, target};
        return id;
    }

    QString xml(const Slide &slide)
    {
        QString tree;
        const QRectF whole(0, 0, m_deckSize.width(), m_deckSize.height());
        QString background = QStringLiteral("<a:solidFill>%1</a:solidFill>").arg(color(slide.backgroundColor.value_or(QColor(Qt::black))));

        if (slide.background && !slide.background->media.isMissing()) {
            const BackgroundMedia &bg = *slide.background;
            if (bg.media.kind == Media::Image) {
                if (const std::optional<MediaPart> part = m_media->part(bg.media)) {
                    const std::optional<QString> fill = m_options.backgroundPlacement == Options::SlideBackground
                                                            ? backgroundFill(*part, bg.scale)
                                                            : std::nullopt;
                    if (fill) {
                        const QString rid = imageRelationship(*part);
                        background = QString(*fill).replace(QLatin1String("RID"), rid);
                    } else {
                        Picture picture;
                        picture.name = bg.isCarriedOver ? QStringLiteral("Background (carried over)") : QStringLiteral("Background");
                        picture.frame = whole;
                        picture.media = bg.media;
                        picture.scale = bg.scale;
                        tree += this->picture(picture);
                    }
                } else {
                    m_notes->add(Note::MissingMedia, QStringLiteral("The media file %1 could not be read").arg(bg.media.fileName), m_slideNumber);
                }
            } else if (bg.media.kind == Media::Video && m_options.videos != Options::LeaveOut) {
                Video v;
                v.name = QStringLiteral("Background video");
                v.frame = whole;
                v.media = bg.media;
                v.scale = bg.scale;
                v.loops = true;
                tree += video(v);
            }
        }

        for (const Item &item : slide.items) {
            if (const Shape *shape = std::get_if<Shape>(&item))
                tree += this->shape(*shape);
            else if (const Picture *picture = std::get_if<Picture>(&item))
                tree += this->picture(*picture);
            else if (const Video *v = std::get_if<Video>(&item); v && m_options.videos != Options::LeaveOut)
                tree += video(*v);
        }

        return xmlHeader + QStringLiteral("<p:sld %1><p:cSld><p:bg><p:bgPr>%2<a:effectLst/></p:bgPr></p:bg><p:spTree>").arg(presentationNamespaces(), background)
               + groupShapeStart + tree + QStringLiteral("</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>")
               + timing() + QStringLiteral("</p:sld>");
    }

private:
    int shapeId() { return m_nextShapeId++; }

    // One relationship for each picture a slide uses, however often it uses it
    QString imageRelationship(const MediaPart &part)
    {
        const auto found = m_imageRelationships.constFind(part.name);
        if (found != m_imageRelationships.constEnd())
            return *found;
        const QString rid = addRelationship(RelType::image, QStringLiteral("../media/") + part.name);
        m_imageRelationships.insert(part.name, rid);
        return rid;
    }

    // The slide background's picture fill, with "RID" where its relationship goes, or none
    // if the picture would be stretched out of shape there.
    std::optional<QString> backgroundFill(const MediaPart &part, ScaleMode mode) const
    {
        QString source;
        const QSizeF size = part.pixelSize;
        if (size.width() > 0 && size.height() > 0) {
            const double pictureAspect = size.width() / size.height();
            const double slideAspect = m_deckSize.width() / m_deckSize.height();
            const bool near = std::abs(pictureAspect / slideAspect - 1) < 0.01;
            if (mode == ScaleMode::Fit && !near)
                return std::nullopt;
            if (mode == ScaleMode::Fill && !near) {
                // Cut what spills over, from both sides equally.
                if (pictureAspect > slideAspect) {
                    const double cut = (1 - slideAspect / pictureAspect) / 2;
                    source = QStringLiteral(R"X(<a:srcRect l="%1" r="%1"/>)X").arg(Units::percent(cut));
                } else {
                    const double cut = (1 - pictureAspect / slideAspect) / 2;
                    source = QStringLiteral(R"X(<a:srcRect t="%1" b="%1"/>)X").arg(Units::percent(cut));
                }
            }
        }
        return QStringLiteral(R"X(<a:blipFill dpi="0" rotWithShape="1"><a:blip r:embed="RID"/>%1<a:stretch><a:fillRect/></a:stretch></a:blipFill>)X").arg(source);
    }

    // ----- Shapes

    QString shape(const Shape &s)
    {
        const int id = shapeId();
        QString spPr = xfrm(s.frame, s.rotation, s.flipHorizontal, s.flipVertical) + geometry(s);
        if (!s.fill) {
            spPr += QStringLiteral("<a:noFill/>");
        } else if (const QColor *c = std::get_if<QColor>(&*s.fill)) {
            spPr += QStringLiteral("<a:solidFill>%1</a:solidFill>").arg(color(*c, s.opacity));
        } else if (const Gradient *g = std::get_if<Gradient>(&*s.fill)) {
            spPr += gradientFill(*g, s.opacity);
        } else if (const PictureFill *p = std::get_if<PictureFill>(&*s.fill)) {
            if (const std::optional<MediaPart> part = m_media->part(p->media)) {
                const QString rid = imageRelationship(*part);
                const QString alpha = s.opacity < 1 ? QStringLiteral(R"X(<a:alphaModFix amt="%1"/>)X").arg(Units::percent(s.opacity)) : QString();
                spPr += QStringLiteral(R"X(<a:blipFill dpi="0" rotWithShape="1"><a:blip r:embed="%1">%2</a:blip><a:stretch><a:fillRect/></a:stretch></a:blipFill>)X").arg(rid, alpha);
            } else {
                spPr += QStringLiteral("<a:noFill/>");
            }
        }
        if (s.line) {
            spPr += QStringLiteral(R"X(<a:ln w="%1"><a:solidFill>%2</a:solidFill>%3</a:ln>)X").arg(number(lineWidth(s.line->width)), color(s.line->color, s.opacity), dash(*s.line));
        } else {
            spPr += QStringLiteral("<a:ln><a:noFill/></a:ln>");
        }
        QString effects;
        if (s.shadow)
            effects += outerShadow(*s.shadow, s.opacity);
        if (s.softEdge > 0)
            effects += QStringLiteral(R"X(<a:softEdge rad="%1"/>)X").arg(number(m_units.emu(s.softEdge)));
        if (!effects.isEmpty())
            spPr += QStringLiteral("<a:effectLst>%1</a:effectLst>").arg(effects);

        const bool isTextBox = s.text && !s.fill && !s.line;
        QString xml = QStringLiteral(R"X(<p:sp><p:nvSpPr><p:cNvPr id="%1" name="%2"/><p:cNvSpPr%3/><p:nvPr/></p:nvSpPr>)X")
                          .arg(id)
                          .arg(esc(s.name), isTextBox ? QStringLiteral(" txBox=\"1\"") : QString());
        xml += QStringLiteral("<p:spPr>%1</p:spPr>").arg(spPr);
        if (s.text)
            xml += textBody(*s.text, s.frame, s.opacity);
        return xml + QStringLiteral("</p:sp>");
    }

    QString xfrm(const QRectF &r, double rotation, bool flipH, bool flipV) const
    {
        QString attributes;
        if (rotation != 0)
            attributes += QStringLiteral(" rot=\"%1\"").arg(Units::angle(rotation));
        if (flipH)
            attributes += QStringLiteral(" flipH=\"1\"");
        if (flipV)
            attributes += QStringLiteral(" flipV=\"1\"");
        return QStringLiteral(R"X(<a:xfrm%1><a:off x="%2" y="%3"/><a:ext cx="%4" cy="%5"/></a:xfrm>)X")
            .arg(attributes, number(m_units.emu(r.x())), number(m_units.emu(r.y())),
                 number(std::max<qint64>(0, m_units.emu(r.width()))), number(std::max<qint64>(0, m_units.emu(r.height()))));
    }

    static QString geometry(const Shape &s)
    {
        switch (s.geometry) {
        case Shape::Ellipse:
            return QStringLiteral(R"X(<a:prstGeom prst="ellipse"><a:avLst/></a:prstGeom>)X");
        case Shape::RoundedRectangle:
            return QStringLiteral(R"X(<a:prstGeom prst="roundRect"><a:avLst><a:gd name="adj" fmla="val %1"/></a:avLst></a:prstGeom>)X")
                .arg(Units::percent(s.cornerFraction));
        case Shape::Polygon:
            if (s.points.size() >= 3) {
                const int size = 100000;
                QString path;
                for (int k = 0; k < s.points.size(); ++k) {
                    const QString pt = QStringLiteral(R"X(<a:pt x="%1" y="%2"/>)X")
                                           .arg(std::llround(s.points.at(k).x() * size))
                                           .arg(std::llround(s.points.at(k).y() * size));
                    path += k == 0 ? QStringLiteral("<a:moveTo>%1</a:moveTo>").arg(pt) : QStringLiteral("<a:lnTo>%1</a:lnTo>").arg(pt);
                }
                return QStringLiteral(R"X(<a:custGeom><a:avLst/><a:gdLst/><a:ahLst/><a:cxnLst/><a:rect l="0" t="0" r="r" b="b"/><a:pathLst><a:path w="%1" h="%1">%2<a:close/></a:path></a:pathLst></a:custGeom>)X")
                    .arg(size)
                    .arg(path);
            }
            [[fallthrough]];
        case Shape::Rectangle:
            break;
        }
        return QStringLiteral(R"X(<a:prstGeom prst="rect"><a:avLst/></a:prstGeom>)X");
    }

    static QString dash(const Line &line)
    {
        if (line.dashes.size() < 2)
            return {};
        const double ratio = line.dashes.at(0) / std::max(line.dashes.at(1), 0.01);
        const QString preset = ratio >= 2.5 ? QStringLiteral("lgDash") : ratio >= 1.5 ? QStringLiteral("dash") : QStringLiteral("sysDash");
        return QStringLiteral(R"X(<a:prstDash val="%1"/>)X").arg(preset);
    }

    // ----- Text

    QString textBody(const TextBody &body, const QRectF &frame, double opacity)
    {
        const QString anchor = body.verticalAlignment == TextBody::Middle ? QStringLiteral("ctr")
                             : body.verticalAlignment == TextBody::Bottom ? QStringLiteral("b")
                                                                          : QStringLiteral("t");
        QString fit;
        switch (body.autofit) {
        case TextBody::NoAutofit:
            fit = QStringLiteral("<a:noAutofit/>");
            break;
        case TextBody::Shrink:
            fit = body.fontScale < 0.999 ? QStringLiteral(R"X(<a:normAutofit fontScale="%1"/>)X").arg(std::max<qint64>(1000, Units::percent(body.fontScale)))
                                         : QStringLiteral("<a:normAutofit/>");
            break;
        case TextBody::ResizeShape:
            fit = QStringLiteral("<a:spAutoFit/>");
            break;
        }
        // PowerPoint's lines put where ProPresenter's are (TextFit says how).
        const TextFit fitted(body, frame);
        // PowerPoint allows no left margin less than nought, and ProPresenter's text can
        // reach left of its margin into the padding (RTF \li-100, seen in the maintainer's
        // files; written as is, PowerPoint offers to "repair" the deck). So the box's left
        // inset moves left by as much as any line reaches, and every paragraph's margin
        // and tab stops move right by the same: each line stays where it was.
        double reach = 0;
        for (const Paragraph &p : body.paragraphs)
            reach = std::min({reach, p.leftIndent, p.leftIndent + p.firstLineIndent});
        const double hang = std::max(0.0, -reach);
        const QMarginsF &i = body.insets;
        QString xml = QStringLiteral(R"X(<p:txBody><a:bodyPr wrap="%1" lIns="%2" tIns="%3" rIns="%4" bIns="%5" rtlCol="0" anchor="%6">%7</a:bodyPr><a:lstStyle/>)X")
                          .arg(body.wraps ? QStringLiteral("square") : QStringLiteral("none"), number(m_units.emu(i.left() - hang)),
                               number(m_units.emu(i.top() + fitted.shift)), number(m_units.emu(i.right())),
                               number(m_units.emu(i.bottom() - fitted.shift)), anchor, fit);
        for (int k = 0; k < body.paragraphs.size(); ++k)
            xml += paragraph(body.paragraphs.at(k), fitted.spacing.value(k, 1), hang, numberingStart(body.paragraphs, k), opacity);
        if (body.paragraphs.isEmpty())
            xml += QStringLiteral(R"X(<a:p><a:endParaRPr lang="%1"/></a:p>)X").arg(m_language);
        return xml + QStringLiteral("</p:txBody>");
    }

    // Where a numbered paragraph's list starts: the number of the first in the run of
    // numbered paragraphs it is in. Every paragraph of a run is written with the same
    // start, which is how PowerPoint knows them for one list and numbers them on.
    static int numberingStart(const QList<Paragraph> &paragraphs, int k)
    {
        const std::optional<Paragraph::Bullet> &bullet = paragraphs.at(k).bullet;
        if (!bullet || bullet->numberScheme.isEmpty())
            return 1;
        int first = bullet->startAt;
        for (int j = k - 1; j >= 0; --j) {
            const std::optional<Paragraph::Bullet> &before = paragraphs.at(j).bullet;
            if (!before || before->numberScheme != bullet->numberScheme || before->startAt != first - 1)
                break;
            first = before->startAt;
        }
        return std::clamp(first, 1, 32767);
    }

    QString paragraph(const Paragraph &p, double spacing, double hang, int numberFrom, double opacity)
    {
        QString attributes;
        switch (p.alignment) {
        case Paragraph::Left:
            attributes += QStringLiteral(" algn=\"l\"");
            break;
        case Paragraph::Center:
            attributes += QStringLiteral(" algn=\"ctr\"");
            break;
        case Paragraph::Right:
            attributes += QStringLiteral(" algn=\"r\"");
            break;
        case Paragraph::Justified:
            attributes += QStringLiteral(" algn=\"just\"");
            break;
        }
        if (p.leftIndent + hang > 0)
            attributes += QStringLiteral(" marL=\"%1\"").arg(number(m_units.emu(p.leftIndent + hang)));
        if (p.firstLineIndent != 0)
            attributes += QStringLiteral(" indent=\"%1\"").arg(number(m_units.emu(p.firstLineIndent)));
        if (p.rightIndent > 0)
            attributes += QStringLiteral(" marR=\"%1\"").arg(number(m_units.emu(p.rightIndent)));
        if (p.defaultTabInterval > 0)
            attributes += QStringLiteral(" defTabSz=\"%1\"").arg(number(m_units.emu(p.defaultTabInterval)));

        QString inner = QStringLiteral(R"X(<a:lnSpc><a:spcPct val="%1"/></a:lnSpc>)X").arg(std::clamp<qint64>(Units::percent(spacing), 0, 13200000));
        // (PowerPoint takes 0 to 1584 points.)
        if (p.spaceBefore > 0)
            inner += QStringLiteral(R"X(<a:spcBef><a:spcPts val="%1"/></a:spcBef>)X").arg(std::min<qint64>(m_units.centipoints(p.spaceBefore), 158400));
        if (p.spaceAfter > 0)
            inner += QStringLiteral(R"X(<a:spcAft><a:spcPts val="%1"/></a:spcAft>)X").arg(std::min<qint64>(m_units.centipoints(p.spaceAfter), 158400));
        if (p.bullet && !p.bullet->numberScheme.isEmpty()) {
            inner += QStringLiteral(R"X(<a:buFontTx/><a:buAutoNum type="%1"%2/>)X")
                         .arg(p.bullet->numberScheme, numberFrom != 1 ? QStringLiteral(" startAt=\"%1\"").arg(numberFrom) : QString());
        } else if (p.bullet) {
            inner += QStringLiteral(R"X(<a:buFontTx/><a:buChar char="%1"/>)X").arg(esc(p.bullet->character));
        } else {
            inner += QStringLiteral("<a:buNone/>");
        }
        if (!p.tabStops.isEmpty()) {
            inner += QStringLiteral("<a:tabLst>");
            for (double stop : p.tabStops)
                inner += QStringLiteral(R"X(<a:tab pos="%1" algn="l"/>)X").arg(number(m_units.emu(stop + hang)));
            inner += QStringLiteral("</a:tabLst>");
        }

        QString xml = QStringLiteral("<a:p><a:pPr%1>%2</a:pPr>").arg(attributes, inner);
        for (const Run &run : p.runs) {
            // A line break inside a paragraph is its own element in PowerPoint. (A
            // vertical tab is PowerPoint's own line break, which slides ProPresenter made
            // from a PowerPoint may still have.)
            QString text = run.text;
            text.replace(QChar(0x0B), QChar(0x2028));
            const QStringList pieces = text.split(QChar(0x2028));
            for (int k = 0; k < pieces.size(); ++k) {
                if (k > 0)
                    xml += QStringLiteral("<a:br>%1</a:br>").arg(runProperties(run.style, opacity, QStringLiteral("a:rPr")));
                if (!pieces.at(k).isEmpty())
                    xml += QStringLiteral("<a:r>%1<a:t>%2</a:t></a:r>").arg(runProperties(run.style, opacity, QStringLiteral("a:rPr")), esc(pieces.at(k)));
            }
        }
        xml += runProperties(p.runs.isEmpty() ? p.endStyle : p.runs.last().style, opacity, QStringLiteral("a:endParaRPr"));
        return xml + QStringLiteral("</a:p>");
    }

    QString runProperties(const RunStyle &s, double opacity, const QString &tag)
    {
        QString attributes = QStringLiteral(" lang=\"%1\" sz=\"%2\"").arg(m_language, number(m_units.fontSize(s.size)));
        attributes += s.bold ? QStringLiteral(" b=\"1\"") : QStringLiteral(" b=\"0\"");
        attributes += s.italic ? QStringLiteral(" i=\"1\"") : QStringLiteral(" i=\"0\"");
        if (s.underline)
            attributes += QStringLiteral(" u=\"sng\"");
        if (s.strikethrough)
            attributes += QStringLiteral(" strike=\"sngStrike\"");
        if (s.capitalization == RunStyle::AllCaps)
            attributes += QStringLiteral(" cap=\"all\"");
        else if (s.capitalization == RunStyle::SmallCaps)
            attributes += QStringLiteral(" cap=\"small\"");
        if (s.characterSpacing != 0)
            attributes += QStringLiteral(" spc=\"%1\"").arg(number(std::clamp<qint64>(m_units.centipoints(s.characterSpacing), -400000, 400000)));
        if (s.superscript != 0)
            attributes += QStringLiteral(" baseline=\"%1\"").arg(s.superscript > 0 ? 30000 : -25000);
        else if (s.baselineOffset != 0 && s.size > 0)
            attributes += QStringLiteral(" baseline=\"%1\"").arg(number(Units::percent(s.baselineOffset / s.size)));
        attributes += QStringLiteral(" dirty=\"0\"");

        QString inner;
        if (s.outline)
            inner += QStringLiteral(R"X(<a:ln w="%1"><a:solidFill>%2</a:solidFill></a:ln>)X").arg(number(lineWidth(s.outline->width)), color(s.outline->color, opacity));
        if (const QColor *c = std::get_if<QColor>(&s.fill)) {
            inner += QStringLiteral("<a:solidFill>%1</a:solidFill>").arg(color(*c, opacity));
        } else if (const Gradient *g = std::get_if<Gradient>(&s.fill)) {
            inner += gradientFill(*g, opacity);
        } else if (const Media *picture = std::get_if<Media>(&s.fill)) {
            // The picture behind the words, filling them (see pptxwriter.h)
            if (const std::optional<MediaPart> part = m_media->part(*picture)) {
                const QString alpha = opacity < 1 ? QStringLiteral(R"X(<a:alphaModFix amt="%1"/>)X").arg(Units::percent(opacity)) : QString();
                inner += QStringLiteral(R"X(<a:blipFill dpi="0" rotWithShape="1"><a:blip r:embed="%1">%2</a:blip><a:stretch><a:fillRect/></a:stretch></a:blipFill>)X")
                             .arg(imageRelationship(*part), alpha);
            } else {
                inner += QStringLiteral("<a:solidFill>%1</a:solidFill>").arg(color(QColor(Qt::white), opacity));
            }
        }
        if (s.shadow)
            inner += QStringLiteral("<a:effectLst>%1</a:effectLst>").arg(outerShadow(*s.shadow, opacity));
        if (s.highlight)
            inner += QStringLiteral("<a:highlight>%1</a:highlight>").arg(color(*s.highlight));
        if (s.underline && s.underlineColor)
            inner += QStringLiteral("<a:uFill><a:solidFill>%1</a:solidFill></a:uFill>").arg(color(*s.underlineColor, opacity));
        const QString face = esc(s.fontFamily.isEmpty() ? s.fontName : s.fontFamily);
        inner += QStringLiteral(R"X(<a:latin typeface="%1"/><a:cs typeface="%1"/>)X").arg(face);
        return QStringLiteral("<%1%2>%3</%1>").arg(tag, attributes, inner);
    }

    // ----- Colours and effects

    static QString color(const QColor &c, double opacity = 1)
    {
        const double alpha = std::clamp(c.alphaF() * opacity, 0.0, 1.0);
        if (alpha >= 0.9995)
            return QStringLiteral(R"X(<a:srgbClr val="%1"/>)X").arg(hex(c));
        return QStringLiteral(R"X(<a:srgbClr val="%1"><a:alpha val="%2"/></a:srgbClr>)X").arg(hex(c)).arg(Units::percent(alpha));
    }

    static QString gradientFill(const Gradient &g, double opacity)
    {
        QList<Gradient::Stop> stops = g.stops;
        if (stops.size() == 1)
            stops << Gradient::Stop {stops.first().color, 1};
        if (stops.isEmpty())
            stops = {Gradient::Stop {QColor(Qt::white), 0}, Gradient::Stop {QColor(Qt::white), 1}};
        QString list;
        for (const Gradient::Stop &stop : stops)
            list += QStringLiteral(R"X(<a:gs pos="%1">%2</a:gs>)X").arg(std::clamp<qint64>(Units::percent(stop.position), 0, 100000)).arg(color(stop.color, opacity));
        const QString path = g.radial ? QStringLiteral(R"X(<a:path path="circle"><a:fillToRect l="50000" t="50000" r="50000" b="50000"/></a:path>)X")
                                      : QStringLiteral(R"X(<a:lin ang="%1" scaled="0"/>)X").arg(Units::angle(g.angle));
        return QStringLiteral(R"X(<a:gradFill flip="none" rotWithShape="1"><a:gsLst>%1</a:gsLst>%2</a:gradFill>)X").arg(list, path);
    }

    // A shadow. PowerPoint takes no distance or blur less than nought: a shadow cast a
    // negative distance one way is cast the same distance the other way.
    QString outerShadow(const Shadow &s, double opacity) const
    {
        const double direction = s.distance < 0 ? s.direction + 180 : s.direction;
        return QStringLiteral(R"X(<a:outerShdw blurRad="%1" dist="%2" dir="%3" algn="ctr" rotWithShape="0">%4</a:outerShdw>)X")
            .arg(number(std::max<qint64>(0, m_units.emu(s.blurRadius))), number(std::abs(m_units.emu(s.distance))),
                 number(Units::angle(direction)), color(s.color, opacity));
    }

    // A line's width, within what PowerPoint takes (0 to 1584 points)
    qint64 lineWidth(double units) const { return std::clamp<qint64>(m_units.emu(units), 0, 20116800); }

    // ----- Pictures and video

    QString picture(const Picture &pic)
    {
        const std::optional<MediaPart> part = pic.media.isMissing() ? std::nullopt : m_media->part(pic.media);
        if (!part) {
            if (!pic.media.isMissing())
                m_notes->add(Note::MissingMedia, QStringLiteral("The media file %1 could not be read").arg(pic.media.fileName), m_slideNumber);
            return {};
        }
        const QString rid = imageRelationship(*part);
        const int id = shapeId();
        const QSizeF size = part->pixelSize.isValid() && !part->pixelSize.isEmpty() ? part->pixelSize : pic.media.naturalSize;
        QRectF frame = pic.frame;
        QMarginsF cropped = pic.crop;
        if (size.width() > 0 && size.height() > 0) {
            const QSizeF visible(size.width() * (1 - cropped.left() - cropped.right()), size.height() * (1 - cropped.top() - cropped.bottom()));
            if (pic.scale == ScaleMode::Fit) {
                frame = placing(pic.frame, visible, ScaleMode::Fit);
            } else if (pic.scale == ScaleMode::Fill) {
                // Cut what spills over the frame, half from each side.
                const QRectF shown = placing(pic.frame, visible, ScaleMode::Fill);
                const double cutX = (shown.width() - pic.frame.width()) / shown.width() / 2 * (1 - cropped.left() - cropped.right());
                const double cutY = (shown.height() - pic.frame.height()) / shown.height() / 2 * (1 - cropped.top() - cropped.bottom());
                cropped = QMarginsF(cropped.left() + cutX, cropped.top() + cutY, cropped.right() + cutX, cropped.bottom() + cutY);
            }
        }
        QString source;
        if (!cropped.isNull()) {
            source = QStringLiteral(R"X(<a:srcRect l="%1" t="%2" r="%3" b="%4"/>)X")
                         .arg(Units::percent(cropped.left()))
                         .arg(Units::percent(cropped.top()))
                         .arg(Units::percent(cropped.right()))
                         .arg(Units::percent(cropped.bottom()));
        }
        const QString alpha = pic.opacity < 1 ? QStringLiteral(R"X(<a:alphaModFix amt="%1"/>)X").arg(Units::percent(pic.opacity)) : QString();
        return QStringLiteral(R"X(<p:pic><p:nvPicPr><p:cNvPr id="%1" name="%2" descr="%3"/><p:cNvPicPr><a:picLocks noChangeAspect="1"/></p:cNvPicPr><p:nvPr/></p:nvPicPr>)X")
                   .arg(id)
                   .arg(esc(pic.name), esc(pic.media.fileName))
               + QStringLiteral(R"X(<p:blipFill><a:blip r:embed="%1">%2</a:blip>%3<a:stretch><a:fillRect/></a:stretch></p:blipFill>)X").arg(rid, alpha, source)
               + QStringLiteral(R"X(<p:spPr>%1<a:prstGeom prst="rect"><a:avLst/></a:prstGeom></p:spPr></p:pic>)X")
                     .arg(xfrm(frame, pic.rotation, pic.flipHorizontal, pic.flipVertical));
    }

    QString video(const Video &v)
    {
        if (v.media.isMissing())
            return {};
        // Fitted as a picture is: placed whole, or filling the frame with what spills over
        // cut off (PowerPoint crops a video as it does a picture).
        QRectF frame = v.frame;
        QString crop;
        const QSizeF size = v.media.naturalSize;
        if (size.width() > 0 && size.height() > 0) {
            if (v.scale == ScaleMode::Fit) {
                frame = placing(v.frame, size, ScaleMode::Fit);
            } else if (v.scale == ScaleMode::Fill) {
                const QRectF shown = placing(v.frame, size, ScaleMode::Fill);
                const double cutX = (shown.width() - v.frame.width()) / shown.width() / 2;
                const double cutY = (shown.height() - v.frame.height()) / shown.height() / 2;
                if (cutX > 0.0005 || cutY > 0.0005) {
                    crop = QStringLiteral(R"X(<a:srcRect l="%1" t="%2" r="%1" b="%2"/>)X").arg(Units::percent(cutX)).arg(Units::percent(cutY));
                }
            }
        }

        // Only a still, by choice or for want of the video's length
        const std::optional<Media> still = v.media.stillPicture();
        if (m_options.videos == Options::StillOnly) {
            if (!still)
                return {};
            Picture picture;
            picture.name = v.name;
            picture.frame = v.frame;
            picture.media = *still;
            picture.scale = v.scale;
            return this->picture(picture);
        }
        const std::optional<MediaPart> posterPart = still ? m_media->part(*still) : std::nullopt;
        const std::optional<MediaPart> videoPart = posterPart ? m_media->part(v.media) : std::nullopt;
        if (!posterPart || !videoPart) {
            m_notes->add(Note::MissingMedia, QStringLiteral("The video %1 could not be read").arg(v.media.fileName), m_slideNumber);
            return {};
        }
        const QString linkId = addRelationship(RelType::video, QStringLiteral("../media/") + videoPart->name);
        const QString mediaId = addRelationship(RelType::media, QStringLiteral("../media/") + videoPart->name);
        const QString posterId = imageRelationship(*posterPart);
        const int id = shapeId();
        if (v.playsAutomatically)
            m_videoTimings << std::make_pair(id, v);
        return QStringLiteral(R"X(<p:pic><p:nvPicPr><p:cNvPr id="%1" name="%2" descr="%3"><a:hlinkClick r:id="" action="ppaction://media"/></p:cNvPr>)X")
                   .arg(id)
                   .arg(esc(v.name), esc(v.media.fileName))
               + QStringLiteral(R"X(<p:cNvPicPr><a:picLocks noChangeAspect="1"/></p:cNvPicPr><p:nvPr><a:videoFile r:link="%1"/>)X").arg(linkId)
               + QStringLiteral(R"X(<p:extLst><p:ext uri="{DAA4B4D4-6D71-4841-9C94-3DE7FCFB9230}"><p14:media xmlns:p14="%1" r:embed="%2"/></p:ext></p:extLst></p:nvPr></p:nvPicPr>)X").arg(nsP14, mediaId)
               + QStringLiteral(R"X(<p:blipFill><a:blip r:embed="%1"/>%2<a:stretch><a:fillRect/></a:stretch></p:blipFill>)X").arg(posterId, crop)
               + QStringLiteral(R"X(<p:spPr>%1<a:prstGeom prst="rect"><a:avLst/></a:prstGeom></p:spPr></p:pic>)X").arg(xfrm(frame, 0, false, false));
    }

    // Starts each video by itself when the slide comes up ("Start: Automatically"), and
    // loops those that loop: the timing PowerPoint itself writes for that. Its nodes are
    // numbered in the order they are written, as PowerPoint numbers them.
    QString timing() const
    {
        if (m_videoTimings.isEmpty())
            return {};
        // 1 and 2 are the root and the main sequence; 3 and 4 the click it waits for.
        int id = 5;
        QString calls;
        for (int k = 0; k < m_videoTimings.size(); ++k) {
            const auto &[shape, v] = m_videoTimings.at(k);
            const qint64 ms = std::llround(v.media.duration * 1000);
            const QString duration = ms > 0 ? QString::number(ms) : QStringLiteral("indefinite");
            const QString node = k == 0 ? QStringLiteral("afterEffect") : QStringLiteral("withEffect");
            calls += QStringLiteral(R"X(<p:par><p:cTn id="%1" presetID="1" presetClass="mediacall" presetSubtype="0" fill="hold" nodeType="%2"><p:stCondLst><p:cond delay="0"/></p:stCondLst>)X").arg(id).arg(node)
                     + QStringLiteral(R"X(<p:childTnLst><p:cmd type="call" cmd="playFrom(0.0)"><p:cBhvr><p:cTn id="%1" dur="%2" fill="hold"/><p:tgtEl><p:spTgt spid="%3"/></p:tgtEl></p:cBhvr></p:cmd></p:childTnLst></p:cTn></p:par>)X")
                           .arg(id + 1)
                           .arg(duration)
                           .arg(shape);
            id += 2;
        }
        QString nodes;
        for (const auto &[shape, v] : m_videoTimings) {
            const QString repeat = v.loops ? QStringLiteral(" repeatCount=\"indefinite\"") : QString();
            const QString mute = v.muted ? QStringLiteral(" mute=\"1\"") : QString();
            nodes += QStringLiteral(R"X(<p:video><p:cMediaNode vol="80000"%1><p:cTn id="%2"%3 fill="hold" display="0"><p:stCondLst><p:cond delay="indefinite"/></p:stCondLst></p:cTn>)X").arg(mute).arg(id).arg(repeat)
                     + QStringLiteral(R"X(<p:tgtEl><p:spTgt spid="%1"/></p:tgtEl></p:cMediaNode></p:video>)X").arg(shape);
            ++id;
        }
        return QStringLiteral(R"X(<p:timing><p:tnLst><p:par><p:cTn id="1" dur="indefinite" restart="never" nodeType="tmRoot"><p:childTnLst>)X"
                              R"X(<p:seq concurrent="1" nextAc="seek"><p:cTn id="2" dur="indefinite" nodeType="mainSeq"><p:childTnLst>)X"
                              R"X(<p:par><p:cTn id="3" fill="hold"><p:stCondLst><p:cond delay="indefinite"/><p:cond evt="onBegin" delay="0"><p:tn val="2"/></p:cond></p:stCondLst>)X"
                              R"X(<p:childTnLst><p:par><p:cTn id="4" fill="hold"><p:stCondLst><p:cond delay="0"/></p:stCondLst><p:childTnLst>)X")
               + calls
               + QStringLiteral(R"X(</p:childTnLst></p:cTn></p:par></p:childTnLst></p:cTn></p:par>)X"
                                R"X(</p:childTnLst></p:cTn><p:prevCondLst><p:cond evt="onPrev" delay="0"><p:tgtEl><p:sldTgt/></p:tgtEl></p:cond></p:prevCondLst>)X"
                                R"X(<p:nextCondLst><p:cond evt="onNext" delay="0"><p:tgtEl><p:sldTgt/></p:tgtEl></p:cond></p:nextCondLst></p:seq>)X")
               + nodes + QStringLiteral("</p:childTnLst></p:cTn></p:par></p:tnLst></p:timing>");
    }

    const Units m_units;
    const QSizeF m_deckSize;
    const Options m_options;
    const QString m_language;
    MediaRegistry *m_media;
    NoteCollector *m_notes;
    const int m_slideNumber;
    QList<Relationship> m_relationships;
    QHash<QString, QString> m_imageRelationships;
    int m_nextShapeId = 2;
    QList<std::pair<int, Video>> m_videoTimings;
};

// ---------------------------------------------------------------------------------------

QString contentTypesPart(const Deck &deck, const QList<bool> &hasNotes, bool withNotes, const QStringList &extensions)
{
    const QString ct = QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml");
    QString s = xmlHeader + QStringLiteral(R"X(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)X");
    s += QStringLiteral(R"X(<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>)X");
    s += QStringLiteral(R"X(<Default Extension="xml" ContentType="application/xml"/>)X");
    for (const QString &ext : extensions)
        s += QStringLiteral(R"X(<Default Extension="%1" ContentType="%2"/>)X").arg(ext, MediaRegistry::contentType(ext));
    s += QStringLiteral(R"X(<Override PartName="/ppt/presentation.xml" ContentType="%1.presentation.main+xml"/>)X").arg(ct);
    s += QStringLiteral(R"X(<Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="%1.slideMaster+xml"/>)X").arg(ct);
    s += QStringLiteral(R"X(<Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="%1.slideLayout+xml"/>)X").arg(ct);
    for (int k = 0; k < deck.slides.size(); ++k) {
        s += QStringLiteral(R"X(<Override PartName="/ppt/slides/slide%1.xml" ContentType="%2.slide+xml"/>)X").arg(k + 1).arg(ct);
        if (hasNotes.value(k))
            s += QStringLiteral(R"X(<Override PartName="/ppt/notesSlides/notesSlide%1.xml" ContentType="%2.notesSlide+xml"/>)X").arg(k + 1).arg(ct);
    }
    if (withNotes) {
        s += QStringLiteral(R"X(<Override PartName="/ppt/notesMasters/notesMaster1.xml" ContentType="%1.notesMaster+xml"/>)X").arg(ct);
        s += QStringLiteral(R"X(<Override PartName="/ppt/theme/theme2.xml" ContentType="application/vnd.openxmlformats-officedocument.theme+xml"/>)X");
    }
    s += QStringLiteral(R"X(<Override PartName="/ppt/theme/theme1.xml" ContentType="application/vnd.openxmlformats-officedocument.theme+xml"/>)X");
    s += QStringLiteral(R"X(<Override PartName="/ppt/presProps.xml" ContentType="%1.presProps+xml"/>)X").arg(ct);
    s += QStringLiteral(R"X(<Override PartName="/ppt/viewProps.xml" ContentType="%1.viewProps+xml"/>)X").arg(ct);
    s += QStringLiteral(R"X(<Override PartName="/ppt/tableStyles.xml" ContentType="%1.tableStyles+xml"/>)X").arg(ct);
    s += QStringLiteral(R"X(<Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>)X");
    s += QStringLiteral(R"X(<Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>)X");
    return s + QStringLiteral("</Types>");
}

// PowerPoint's sections, each slide in exactly one, the first starting at slide 1
QString sectionList(const Deck &deck)
{
    QList<Section> sections;
    for (const Section &section : deck.sections) {
        if (section.firstSlide < deck.slides.size())
            sections << section;
    }
    std::stable_sort(sections.begin(), sections.end(), [](const Section &a, const Section &b) { return a.firstSlide < b.firstSlide; });
    if (sections.isEmpty() || deck.slides.isEmpty())
        return {};
    QString list;
    for (int k = 0; k < sections.size(); ++k) {
        const int start = k == 0 ? 0 : sections.at(k).firstSlide;
        const int end = k + 1 < sections.size() ? sections.at(k + 1).firstSlide : int(deck.slides.size());
        QString ids;
        for (int slide = start; slide < std::max(start, end); ++slide)
            ids += QStringLiteral(R"X(<p14:sldId id="%1"/>)X").arg(256 + slide);
        list += QStringLiteral(R"X(<p14:section name="%1" id="%2"><p14:sldIdLst>%3</p14:sldIdLst></p14:section>)X")
                    .arg(esc(sections.at(k).name), QUuid::createUuid().toString(QUuid::WithBraces).toUpper(), ids);
    }
    return QStringLiteral(R"X(<p:extLst><p:ext uri="{521415D9-36F7-43E2-AB2F-B90AF26B5E84}"><p14:sectionLst xmlns:p14="%1">%2</p14:sectionLst></p:ext></p:extLst>)X").arg(nsP14, list);
}

// The language words are marked as being in, for PowerPoint's spelling checker: this
// computer's ("en-AU")
QString language()
{
    const QString name = QLocale::system().name();
    if (name.size() < 2 || name == u"C")
        return QStringLiteral("en-US");
    return QString(name).replace(u'_', u'-');
}

} // namespace

bool writeDeck(const Deck &deck, const QString &path, const Options &options, NoteCollector *notes, QString *error,
               const std::function<bool(int, int)> &progress)
{
    const Units units(deck.size);
    const QString lang = language();
    MediaRegistry media;
    bool withNotes = false;
    if (options.includeNotes) {
        for (const Slide &slide : deck.slides)
            withNotes = withNotes || !slide.notes.isEmpty();
    }
    int done = 0;
    int total = int(deck.slides.size());
    const auto step = [&]() { return !progress || progress(++done, total); };

    struct SlidePart
    {
        QString xml;
        QString rels;
        QString notes;
    };
    QList<SlidePart> slideParts;
    for (int k = 0; k < deck.slides.size(); ++k) {
        const Slide &slide = deck.slides.at(k);
        SlideXml writer(units, deck.size, options, lang, &media, notes, k + 1);
        SlidePart part;
        part.xml = writer.xml(slide);
        QList<Relationship> rels = writer.relationships();
        if (withNotes && !slide.notes.isEmpty()) {
            part.notes = notesSlidePart(slide.notes, lang);
            rels << Relationship {QStringLiteral("rId%1").arg(rels.size() + 1), RelType::notesSlide, QStringLiteral("../notesSlides/notesSlide%1.xml").arg(k + 1)};
        }
        part.rels = relationshipsPart(rels);
        slideParts << part;
        if (!step()) {
            *error = QStringLiteral("The export was stopped");
            return false;
        }
    }
    total = int(deck.slides.size()) + media.count();

    // Written next to the destination, then moved over it
    const QFileInfo destination(path);
    const QString temporary = destination.absolutePath() + QStringLiteral("/.") + destination.fileName() + u'.'
                              + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".partial");
    PptxZipWriter zip;
    if (!zip.open(temporary, error))
        return false;
    const auto fail = [&temporary]() {
        QFile::remove(temporary);
        return false;
    };
    const auto add = [&zip, error](const QString &name, const QString &text) {
        return zip.add(name, text.toUtf8(), PptxZipWriter::Deflate, error);
    };

    QList<bool> hasNotes;
    for (const SlidePart &part : std::as_const(slideParts))
        hasNotes << !part.notes.isEmpty();
    bool ok = add(QStringLiteral("[Content_Types].xml"), contentTypesPart(deck, hasNotes, withNotes, media.extensions()))
              && add(QStringLiteral("_rels/.rels"), relationshipsPart({
                                                         {QStringLiteral("rId1"), RelType::officeDocument, QStringLiteral("ppt/presentation.xml")},
                                                         {QStringLiteral("rId2"), RelType::coreProperties, QStringLiteral("docProps/core.xml")},
                                                         {QStringLiteral("rId3"), RelType::extendedProperties, QStringLiteral("docProps/app.xml")},
                                                     }))
              && add(QStringLiteral("docProps/core.xml"), corePropertiesPart(deck.title))
              && add(QStringLiteral("docProps/app.xml"), appPropertiesPart(int(deck.slides.size())));

    // The presentation and what it points to
    QList<Relationship> presentationRels = {{QStringLiteral("rId1"), RelType::slideMaster, QStringLiteral("slideMasters/slideMaster1.xml")}};
    QString slideList;
    for (int k = 0; k < deck.slides.size(); ++k) {
        const QString rid = QStringLiteral("rId%1").arg(presentationRels.size() + 1);
        presentationRels << Relationship {rid, RelType::slide, QStringLiteral("slides/slide%1.xml").arg(k + 1)};
        slideList += QStringLiteral(R"X(<p:sldId id="%1" r:id="%2"/>)X").arg(256 + k).arg(rid);
    }
    QString notesMasterList;
    if (withNotes) {
        const QString rid = QStringLiteral("rId%1").arg(presentationRels.size() + 1);
        presentationRels << Relationship {rid, RelType::notesMaster, QStringLiteral("notesMasters/notesMaster1.xml")};
        notesMasterList = QStringLiteral(R"X(<p:notesMasterIdLst><p:notesMasterId r:id="%1"/></p:notesMasterIdLst>)X").arg(rid);
    }
    for (const auto &[type, target] : {std::make_pair(RelType::presProps, QStringLiteral("presProps.xml")),
                                       std::make_pair(RelType::viewProps, QStringLiteral("viewProps.xml")),
                                       std::make_pair(RelType::theme, QStringLiteral("theme/theme1.xml")),
                                       std::make_pair(RelType::tableStyles, QStringLiteral("tableStyles.xml"))})
        presentationRels << Relationship {QStringLiteral("rId%1").arg(presentationRels.size() + 1), type, target};
    const QString presentation = xmlHeader + QStringLiteral(R"X(<p:presentation %1 saveSubsetFonts="1">)X").arg(presentationNamespaces())
                                 + QStringLiteral(R"X(<p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst>)X") + notesMasterList
                                 + (slideList.isEmpty() ? QString() : QStringLiteral("<p:sldIdLst>%1</p:sldIdLst>").arg(slideList))
                                 + QStringLiteral(R"X(<p:sldSz cx="%1" cy="%2"/><p:notesSz cx="6858000" cy="9144000"/>)X")
                                       .arg(number(units.emu(deck.size.width())), number(qint64(Units::slideHeightEMU)))
                                 + sectionList(deck) + QStringLiteral("</p:presentation>");
    ok = ok && add(QStringLiteral("ppt/presentation.xml"), presentation)
         && add(QStringLiteral("ppt/_rels/presentation.xml.rels"), relationshipsPart(presentationRels))
         && add(QStringLiteral("ppt/presProps.xml"), presPropsPart())
         && add(QStringLiteral("ppt/viewProps.xml"), viewPropsPart())
         && add(QStringLiteral("ppt/tableStyles.xml"), tableStylesPart())
         && add(QStringLiteral("ppt/theme/theme1.xml"), themePart(QStringLiteral("Simple Presenter")))
         && add(QStringLiteral("ppt/slideMasters/slideMaster1.xml"), slideMasterPart())
         && add(QStringLiteral("ppt/slideMasters/_rels/slideMaster1.xml.rels"), relationshipsPart({
                                                                                    {QStringLiteral("rId1"), RelType::slideLayout, QStringLiteral("../slideLayouts/slideLayout1.xml")},
                                                                                    {QStringLiteral("rId2"), RelType::theme, QStringLiteral("../theme/theme1.xml")},
                                                                                }))
         && add(QStringLiteral("ppt/slideLayouts/slideLayout1.xml"), slideLayoutPart())
         && add(QStringLiteral("ppt/slideLayouts/_rels/slideLayout1.xml.rels"), relationshipsPart({
                                                                                    {QStringLiteral("rId1"), RelType::slideMaster, QStringLiteral("../slideMasters/slideMaster1.xml")},
                                                                                }));
    if (ok && withNotes) {
        ok = add(QStringLiteral("ppt/notesMasters/notesMaster1.xml"), notesMasterPart())
             && add(QStringLiteral("ppt/notesMasters/_rels/notesMaster1.xml.rels"), relationshipsPart({
                                                                                        {QStringLiteral("rId1"), RelType::theme, QStringLiteral("../theme/theme2.xml")},
                                                                                    }))
             && add(QStringLiteral("ppt/theme/theme2.xml"), themePart(QStringLiteral("Notes")));
    }
    for (int k = 0; ok && k < slideParts.size(); ++k) {
        const SlidePart &part = slideParts.at(k);
        ok = add(QStringLiteral("ppt/slides/slide%1.xml").arg(k + 1), part.xml)
             && add(QStringLiteral("ppt/slides/_rels/slide%1.xml.rels").arg(k + 1), part.rels);
        if (ok && !part.notes.isEmpty()) {
            ok = add(QStringLiteral("ppt/notesSlides/notesSlide%1.xml").arg(k + 1), part.notes)
                 && add(QStringLiteral("ppt/notesSlides/_rels/notesSlide%1.xml.rels").arg(k + 1), relationshipsPart({
                                                                                                    {QStringLiteral("rId1"), RelType::notesMaster, QStringLiteral("../notesMasters/notesMaster1.xml")},
                                                                                                    {QStringLiteral("rId2"), RelType::slide, QStringLiteral("../slides/slide%1.xml").arg(k + 1)},
                                                                                                }));
        }
    }
    bool stopped = false;
    ok = ok && media.write(&zip, error, [&]() {
             stopped = !step();
             return !stopped;
         });
    if (stopped)
        *error = QStringLiteral("The export was stopped");
    if (!ok) {
        QString ignored;
        zip.finish(&ignored);
        return fail();
    }
    if (!zip.finish(error))
        return fail();
    // (rename() puts the new file in the old one's place in one step, where it is there)
    if (std::rename(QFile::encodeName(temporary).constData(), QFile::encodeName(path).constData()) != 0) {
        *error = QStringLiteral("%1 could not be written").arg(destination.fileName());
        return fail();
    }
    return true;
}

QStringList outOfRange(const QString &xml)
{
    struct Rule
    {
        const char *attribute;
        const char *element;
        qint64 low;
        qint64 high;
    };
    // The ranges of the ECMA-376 schema's types (ST_TextMargin, ST_TextIndent,
    // ST_TextFontSize and so on), for the attributes written here
    static const Rule rules[] = {
        {"marL", "a:pPr", 0, 51206400},
        {"marR", "a:pPr", 0, 51206400},
        {"indent", "a:pPr", -51206400, 51206400},
        {"defTabSz", "a:pPr", 0, 51206400},
        {"startAt", "a:buAutoNum", 1, 32767},
        {"sz", "a:rPr", 100, 400000},
        {"sz", "a:endParaRPr", 100, 400000},
        {"spc", "a:rPr", -400000, 400000},
        {"blurRad", "a:outerShdw", 0, std::numeric_limits<qint64>::max()},
        {"dist", "a:outerShdw", 0, std::numeric_limits<qint64>::max()},
        {"dir", "a:outerShdw", 0, 21599999},
        {"rad", "a:softEdge", 0, std::numeric_limits<qint64>::max()},
        {"w", "a:ln", 0, 20116800},
        {"val", "a:alpha", 0, 100000},
        {"amt", "a:alphaModFix", 0, 100000},
        {"val", "a:spcPct", 0, 13200000},
        {"val", "a:spcPts", 0, 158400},
        {"pos", "a:gs", 0, 100000},
        {"ang", "a:lin", 0, 21599999},
        {"rot", "a:xfrm", -2147483647, 2147483647},
        {"fontScale", "a:normAutofit", 1000, 100000},
        {"cx", "a:ext", 0, 27273042316900},
        {"cy", "a:ext", 0, 27273042316900},
    };
    static const QRegularExpression elementPattern(QStringLiteral(R"X(<([a-z0-9]+:[A-Za-z0-9]+)((?:\s+[A-Za-z0-9:]+="[^"]*")*)\s*/?>)X"));
    static const QRegularExpression attributePattern(QStringLiteral(R"X(([A-Za-z0-9:]+)="([^"]*)")X"));
    QStringList found;
    auto elements = elementPattern.globalMatch(xml);
    while (elements.hasNext()) {
        const QRegularExpressionMatch element = elements.next();
        const QString name = element.captured(1);
        auto attributes = attributePattern.globalMatch(element.captured(2));
        while (attributes.hasNext()) {
            const QRegularExpressionMatch attribute = attributes.next();
            const QString key = attribute.captured(1);
            const QString value = attribute.captured(2);
            for (const Rule &rule : rules) {
                if (key != QLatin1String(rule.attribute) || name != QLatin1String(rule.element))
                    continue;
                bool ok = false;
                const qint64 n = value.toLongLong(&ok);
                if (!ok)
                    found << QStringLiteral("%1 %2=%3 (not a whole number)").arg(name, key, value);
                else if (n < rule.low || n > rule.high)
                    found << QStringLiteral("%1 %2=%3").arg(name, key, value);
            }
        }
    }
    return found;
}

} // namespace pptx
