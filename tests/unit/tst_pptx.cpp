// Export to PowerPoint (src/pptx*.h), tried on files and text made for the purpose: the
// zip a PowerPoint file is, written and read back; ProPresenter's RTF read as ProToPPTX
// reads it (fonts, colours with alpha, indents, spacing, tabs, lists, Unicode); a font's
// names as Office goes by them, from a name table made here and from a font that is
// installed; and a deck made by hand written as a PowerPoint file: every part good XML,
// every value within the standard's ranges, and what is on its slides where it should be.

#include "pptxdeck.h"
#include "pptxfixtures.h"
#include "pptxfonts.h"
#include "pptximport.h"
#include "pptxrtf.h"
#include "pptxwriter.h"
#include "pptxzip.h"
#include "zipreader.h"

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGuiApplication>
#include <QUrl>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QXmlStreamReader>

#include "presentation.pb.h"

#include <google/protobuf/text_format.h>

#include <fontconfig/fontconfig.h>

using namespace pptx;

namespace {

QByteArray utf16be(const QString &text)
{
    QByteArray out;
    for (QChar c : text) {
        out.append(char(c.unicode() >> 8));
        out.append(char(c.unicode() & 0xFF));
    }
    return out;
}

void be16(QByteArray *out, int v)
{
    out->append(char((v >> 8) & 0xFF));
    out->append(char(v & 0xFF));
}

// A name table with Windows' English family and subfamily, and a Mac family that is
// not the one Office goes by
QByteArray madeNameTable(const QString &family, const QString &subfamily)
{
    const QByteArray f = utf16be(family);
    const QByteArray s = utf16be(subfamily);
    const QByteArray mac = "Other Family";
    QByteArray table;
    be16(&table, 0);              // format
    be16(&table, 3);              // count
    be16(&table, 6 + 3 * 12);     // where the strings start
    const auto record = [&table](int platform, int encoding, int language, int id, int length, int offset) {
        be16(&table, platform);
        be16(&table, encoding);
        be16(&table, language);
        be16(&table, id);
        be16(&table, length);
        be16(&table, offset);
    };
    record(1, 0, 0, 1, int(mac.size()), 0);
    record(3, 1, 0x409, 1, int(f.size()), int(mac.size()));
    record(3, 1, 0x409, 2, int(s.size()), int(mac.size() + f.size()));
    return table + mac + f + s;
}

// A Cocoa RTF of two paragraphs, as macOS writes one (made up words)
QByteArray cocoaRtf()
{
    return QByteArray(R"({\rtf1\ansi\ansicpg1252\cocoartf2822
\cocoatextscaling0\cocoaplatform0{\fonttbl\f0\fswiss\fcharset0 Helvetica;\f1\fnil\fcharset0 CMGSans-Light;}
{\colortbl;\red255\green255\blue255;\red255\green0\blue0;}
{\*\expandedcolortbl;;\cssrgb\c100000\c0\c0\c50000;}
\pard\li200\fi-100\sa300\sl360\slmult1\tx720\tx-100\qc\partightenfactor0

\f1\fs144 \cf2 Made up words\
\pard\qr\f0\fs72 \cf1 Second {\b bold} caf\uc0\u233 \'93})");
}

QByteArray png(const QSize &size, const QColor &colour)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(colour);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

// Every part of a PowerPoint file, by name
QHash<QString, QByteArray> partsOf(const QString &path)
{
    QHash<QString, QByteArray> parts;
    ZipReader reader;
    QString error;
    if (!reader.open(path, &error))
        return parts;
    for (const ZipReader::Entry &entry : reader.entries()) {
        QByteArray bytes;
        if (reader.read(entry, &bytes, &error))
            parts.insert(entry.name, bytes);
    }
    return parts;
}

RunStyle style(double size, const QColor &colour)
{
    RunStyle s;
    s.fontName = QStringLiteral("Helvetica");
    s.fontFamily = QStringLiteral("Helvetica");
    s.size = size;
    s.fill = colour;
    return s;
}

} // namespace

class TestPptx : public QObject
{
    Q_OBJECT

private slots:
    void aZipIsWrittenThatReadsBack()
    {
        QTemporaryDir folder;
        const QString path = folder.filePath("deck.pptx");
        const QString source = folder.filePath("picture.png");
        {
            QFile picture(source);
            QVERIFY(picture.open(QIODevice::WriteOnly));
            picture.write(QByteArray(300000, 'p'));
        }
        PptxZipWriter zip;
        QString error;
        QVERIFY(zip.open(path, &error));
        const QByteArray xml = QByteArray("<?xml version=\"1.0\"?>") + QByteArray(5000, 'x');
        QVERIFY(zip.add(QStringLiteral("[Content_Types].xml"), xml, PptxZipWriter::Deflate, &error));
        QVERIFY(zip.add(QStringLiteral("ppt/media/empty.bin"), QByteArray(), PptxZipWriter::Deflate, &error));
        QVERIFY(zip.addFile(QStringLiteral("ppt/media/image1.png"), source, &error));
        QVERIFY(zip.finish(&error));

        ZipReader reader;
        QVERIFY2(reader.open(path, &error), qPrintable(error));
        QCOMPARE(reader.entries().size(), 3);
        QCOMPARE(reader.entries().at(0).name, "[Content_Types].xml");
        // The XML deflated, the media stored, as PowerPoint writes them
        QCOMPARE(reader.entries().at(0).method, 8);
        QVERIFY(reader.entries().at(0).compressedSize < quint64(xml.size()));
        QCOMPARE(reader.entries().at(2).method, 0);
        QByteArray back;
        QVERIFY2(reader.read(reader.entries().at(0), &back, &error), qPrintable(error));
        QCOMPARE(back, xml);
        QVERIFY2(reader.read(reader.entries().at(2), &back, &error), qPrintable(error));
        QCOMPARE(back, QByteArray(300000, 'p'));
        // (Into a buffer of its own: the reader writes over what is there, and leaves
        // the rest)
        QByteArray empty;
        QVERIFY(reader.read(reader.entries().at(1), &empty, &error));
        QVERIFY(empty.isEmpty());
    }

    void cocoaRtfIsReadWithItsParagraphSettingsAndColours()
    {
        const RtfDocument document = readRtf(cocoaRtf());
        QCOMPARE(document.fontNames, QStringList({"Helvetica", "CMGSans-Light"}));
        QCOMPARE(document.paragraphs.size(), 2);
        const Paragraph &first = document.paragraphs.at(0);
        QCOMPARE(first.alignment, Paragraph::Center);
        QCOMPARE(first.leftIndent, 10.0);
        QCOMPARE(first.firstLineIndent, -5.0);
        QCOMPARE(first.spaceAfter, 15.0);
        QVERIFY(first.lineSpacing.has_value());
        QCOMPARE(first.lineSpacing->kind, Paragraph::LineSpacing::Multiple);
        QCOMPARE(first.lineSpacing->value, 1.5);
        // (A stop left of the margin is left out, as macOS ignores it)
        QCOMPARE(first.tabStops, QList<double>({36.0}));
        QCOMPARE(first.runs.size(), 1);
        QCOMPARE(first.runs.at(0).text, "Made up words");
        QCOMPARE(first.runs.at(0).style.fontName, "CMGSans-Light");
        QCOMPARE(first.runs.at(0).style.size, 72.0);
        // The expanded table's colour, alpha and all, over the plain table's
        const QColor red = std::get<QColor>(first.runs.at(0).style.fill);
        QCOMPARE(red.red(), 255);
        QCOMPARE(red.green(), 0);
        QVERIFY(qAbs(red.alphaF() - 0.5) < 0.01);

        const Paragraph &second = document.paragraphs.at(1);
        QCOMPARE(second.alignment, Paragraph::Right);
        QCOMPARE(second.leftIndent, 0.0);
        QCOMPARE(second.runs.size(), 3);
        QCOMPARE(second.runs.at(0).text, "Second ");
        QCOMPARE(second.runs.at(0).style.size, 36.0);
        // An entry the expanded table leaves empty is the plain table's
        QCOMPARE(std::get<QColor>(second.runs.at(0).style.fill), QColor(255, 255, 255));
        QCOMPARE(second.runs.at(1).text, "bold");
        QVERIFY(second.runs.at(1).style.bold);
        // A Unicode character and a Windows-1252 byte (a left double quote)
        QCOMPARE(second.runs.at(2).text, QString(QStringLiteral(" caf") + QChar(0xE9) + QChar(0x201C)));
        QVERIFY(!second.runs.at(2).style.bold);
        // Counted as ProPresenter counts the text: a paragraph break is one
        QCOMPARE(document.paragraphStarts(), QList<int>({0, 14}));
    }

    void aLineBreakAListAndAnEmptyLastLine()
    {
        // Built from pieces: a backslash, a u and four digits in one piece would not
        // survive being typed into this file through a tool. (The space after a control
        // word is its end, not a space in the text.)
        const QByteArray rtf = QByteArray(R"({\rtf1\ansi{\fonttbl\f0 Helvetica;}\f0\fs48 \ls1\ilvl0{\listtext	\'95	}One\)") + "\n"
                               + QByteArray(R"({\listtext	\'95	}Two\uc0\)") + "u8232" + QByteArray(R"( more\)") + "\n}";
        const RtfDocument document = readRtf(rtf);
        QCOMPARE(document.paragraphs.size(), 3);
        QVERIFY(document.paragraphs.at(0).bullet.has_value());
        QCOMPARE(document.paragraphs.at(0).bullet->character, QString(QChar(0x2022)));
        QCOMPARE(document.markerLengths.value(0), 3);
        QCOMPARE(document.paragraphs.at(0).runs.at(0).text, "One");
        QCOMPARE(document.paragraphs.at(1).runs.at(0).text, QString(QStringLiteral("Two") + QChar(0x2028) + QStringLiteral("more")));
        // What follows the last break is a paragraph of its own, if an empty one, with
        // the size in force at the end
        QVERIFY(document.paragraphs.at(2).runs.isEmpty());
        QCOMPARE(document.paragraphs.at(2).endStyle.size, 24.0);
    }

    void numberedMarkersAreNumberedLists()
    {
        QCOMPARE(bulletFromMarker(QStringLiteral("\t3.\t"))->numberScheme, "arabicPeriod");
        QCOMPARE(bulletFromMarker(QStringLiteral("\t3.\t"))->startAt, 3);
        QCOMPARE(bulletFromMarker(QStringLiteral("(2)"))->numberScheme, "arabicParenBoth");
        QCOMPARE(bulletFromMarker(QStringLiteral("c."))->numberScheme, "alphaLcPeriod");
        QCOMPARE(bulletFromMarker(QStringLiteral("c."))->startAt, 3);
        QCOMPARE(bulletFromMarker(QStringLiteral("\t-\t"))->character, "-");
        QVERIFY(!bulletFromMarker(QStringLiteral("\t\t")).has_value());
    }

    void textThatIsNotRtfIsPlainWords()
    {
        const RtfDocument document = readRtf("First line\nSecond");
        QCOMPARE(document.paragraphs.size(), 2);
        QCOMPARE(document.paragraphs.at(1).runs.at(0).text, "Second");
    }

    void officeGoesByTheLegacyFamilyAndItsFourFaces()
    {
        const auto names = legacyNames(madeNameTable(QStringLiteral("Test Sans Light"), QStringLiteral("Regular")));
        QVERIFY(names.has_value());
        // Windows' English names over the Mac's
        QCOMPARE(names->first, "Test Sans Light");
        QCOMPARE(names->second, "Regular");

        FontFace face;
        face.fullName = QStringLiteral("Test Sans Light");
        face.legacyFamily = QStringLiteral("Test Sans Light");
        face.legacySubfamily = QStringLiteral("Bold Italic");
        QCOMPARE(face.officeTypeface(), "Test Sans Light");
        QVERIFY(face.officeBold());
        QVERIFY(face.officeItalic());
        // A face whose names do not follow Office's rule goes by its full name, plain
        face.fullName = QStringLiteral("Test Sans Condensed Black");
        face.legacySubfamily = QStringLiteral("Condensed Black");
        QCOMPARE(face.officeTypeface(), "Test Sans Condensed Black");
        QVERIFY(!face.officeBold());
    }

    void anInstalledFontIsFoundByItsPostScriptName()
    {
        // Whichever font fontconfig lists first that has a PostScript name and a name
        // table: what is installed differs from one computer to the next
        FcObjectSet *wanted = FcObjectSetBuild(FC_POSTSCRIPT_NAME, FC_FILE, FC_INDEX, nullptr);
        FcPattern *any = FcPatternCreate();
        FcFontSet *set = FcFontList(nullptr, any, wanted);
        QString name;
        QString file;
        for (int k = 0; set && k < set->nfont && name.isEmpty(); ++k) {
            FcChar8 *ps = nullptr;
            FcChar8 *path = nullptr;
            int index = 0;
            if (FcPatternGetString(set->fonts[k], FC_POSTSCRIPT_NAME, 0, &ps) == FcResultMatch
                && FcPatternGetString(set->fonts[k], FC_FILE, 0, &path) == FcResultMatch) {
                FcPatternGetInteger(set->fonts[k], FC_INDEX, 0, &index);
                if (!nameTable(QString::fromUtf8(reinterpret_cast<char *>(path)), index & 0xFFFF).isEmpty()) {
                    name = QString::fromUtf8(reinterpret_cast<char *>(ps));
                    file = QString::fromUtf8(reinterpret_cast<char *>(path));
                }
            }
        }
        if (set)
            FcFontSetDestroy(set);
        FcPatternDestroy(any);
        FcObjectSetDestroy(wanted);
        if (name.isEmpty())
            QSKIP("no font with a name table is installed here");
        const std::optional<FontFace> face = installedFace(name);
        QVERIFY2(face.has_value(), qPrintable(name));
        QVERIFY(!face->legacyFamily.isEmpty());
        QVERIFY(!face->officeTypeface().isEmpty());
        QCOMPARE(face->postScriptName, name);
        // And one that is not installed anywhere is not found
        QVERIFY(!installedFace(QStringLiteral("NoSuchFont-MadeUpForTheTest")).has_value());
    }

    void aDeckIsWrittenAsAPowerPointFile()
    {
        QTemporaryDir folder;
        const QString backgroundFile = folder.filePath("Background.png");
        const QString pictureFile = folder.filePath("Blue.png");
        {
            QFile out(backgroundFile);
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write(png(QSize(192, 108), Qt::red));
            QFile blue(pictureFile);
            QVERIFY(blue.open(QIODevice::WriteOnly));
            blue.write(png(QSize(200, 100), Qt::blue));
        }
        Deck deck;
        deck.title = QStringLiteral("A made-up deck");

        // The first slide: a picture behind it, words, a shape and a picture
        Slide first;
        BackgroundMedia background;
        background.media.kind = Media::Image;
        background.media.path = backgroundFile;
        background.media.fileName = QStringLiteral("Background.png");
        background.scale = ScaleMode::Fill;
        first.background = background;
        Shape words;
        words.name = QStringLiteral("Words");
        words.frame = QRectF(160, 340, 1600, 400);
        TextBody body;
        body.insets = QMarginsF(5, 5, 5, 5);
        body.verticalAlignment = TextBody::Middle;
        Paragraph paragraph;
        paragraph.alignment = Paragraph::Center;
        paragraph.runs << Run {QStringLiteral("Made up & <words>"), style(72, Qt::white)};
        paragraph.endStyle = style(72, Qt::white);
        body.paragraphs << paragraph;
        words.text = body;
        first.items << words;
        Shape box;
        box.name = QStringLiteral("Box");
        box.frame = QRectF(100, 100, 300, 200);
        box.geometry = Shape::RoundedRectangle;
        box.cornerFraction = 0.2;
        Gradient gradient;
        gradient.angle = 90;
        gradient.stops = {Gradient::Stop {Qt::red, 0}, Gradient::Stop {QColor(0, 0, 255, 128), 1}};
        box.fill = gradient;
        box.line = Line {4, Qt::white, {}};
        box.shadow = Shadow {QColor(0, 0, 0, 200), 45, 8, 12};
        first.items << box;
        Picture picture;
        picture.name = QStringLiteral("Picture");
        picture.frame = QRectF(1400, 600, 400, 400);
        picture.media.path = pictureFile;
        picture.media.fileName = QStringLiteral("Blue.png");
        picture.scale = ScaleMode::Fit;
        first.items << picture;
        deck.slides << first;

        // The second: notes, and words filled with a picture (the stand-in for a blur)
        Slide second;
        second.backgroundColor = QColor(10, 20, 30);
        second.notes = QStringLiteral("A note\nAnd its second line");
        Shape frosted;
        frosted.name = QStringLiteral("Frosted");
        frosted.frame = QRectF(0, 0, 1920, 540);
        TextBody frostedBody;
        Paragraph frostedLine;
        RunStyle filled = style(100, Qt::white);
        Media blurred;
        blurred.kind = Media::Image;
        blurred.data = png(QSize(96, 27), Qt::gray);
        blurred.dataId = QStringLiteral("blur:test");
        blurred.fileName = QStringLiteral("Behind.png");
        filled.fill = blurred;
        frostedLine.runs << Run {QStringLiteral("Frosted"), filled};
        frostedLine.endStyle = filled;
        frostedBody.paragraphs << frostedLine;
        frosted.text = frostedBody;
        second.items << frosted;
        deck.slides << second;
        deck.sections = {Section {QStringLiteral("Verse"), 0}, Section {QStringLiteral("Chorus"), 1}};

        const QString path = folder.filePath("Deck.pptx");
        NoteCollector notes;
        QString error;
        int progressed = 0;
        QVERIFY2(writeDeck(deck, path, Options(), &notes, &error, [&progressed](int done, int) {
                     progressed = done;
                     return true;
                 }),
                 qPrintable(error));
        QVERIFY(progressed >= 2);
        // (Kept where PPTX_KEEP says, to be opened by hand: in LibreOffice or PowerPoint)
        if (!qEnvironmentVariableIsEmpty("PPTX_KEEP"))
            QFile::copy(path, QDir(qEnvironmentVariable("PPTX_KEEP")).filePath("Deck.pptx"));
        QVERIFY2(notes.all().isEmpty(), qPrintable(notes.all().value(0).message));
        // Nothing is left beside it
        QCOMPARE(QDir(folder.path()).entryList(QDir::Files | QDir::Hidden).filter(".partial").size(), 0);

        const QHash<QString, QByteArray> parts = partsOf(path);
        for (const QString &name : {"[Content_Types].xml", "_rels/.rels", "docProps/core.xml", "docProps/app.xml",
                                    "ppt/presentation.xml", "ppt/_rels/presentation.xml.rels", "ppt/slides/slide1.xml",
                                    "ppt/slides/slide2.xml", "ppt/slides/_rels/slide2.xml.rels", "ppt/notesSlides/notesSlide2.xml",
                                    "ppt/notesMasters/notesMaster1.xml", "ppt/slideMasters/slideMaster1.xml",
                                    "ppt/slideLayouts/slideLayout1.xml", "ppt/theme/theme1.xml", "ppt/theme/theme2.xml"})
            QVERIFY2(parts.contains(name), qPrintable(name));
        // Every part good XML, and every value within the standard's ranges
        for (auto part = parts.cbegin(); part != parts.cend(); ++part) {
            if (!part.key().endsWith(".xml") && !part.key().endsWith(".rels"))
                continue;
            QXmlStreamReader xml(part.value());
            while (!xml.atEnd())
                xml.readNext();
            QVERIFY2(!xml.hasError(), qPrintable(part.key() + ": " + xml.errorString()));
            const QStringList problems = outOfRange(QString::fromUtf8(part.value()));
            QVERIFY2(problems.isEmpty(), qPrintable(part.key() + ": " + problems.join(", ")));
        }
        // The media once each, as it was
        QCOMPARE(parts.value("ppt/media/image2.png"), png(QSize(200, 100), Qt::blue));
        QVERIFY(parts.contains("ppt/media/image3.png"));
        const QString types = QString::fromUtf8(parts.value("[Content_Types].xml"));
        QVERIFY(types.contains(R"(<Default Extension="png" ContentType="image/png"/>)"));
        QVERIFY(types.contains("/ppt/notesSlides/notesSlide2.xml"));
        QVERIFY(!types.contains("/ppt/notesSlides/notesSlide1.xml"));

        // A 1080p presentation is PowerPoint's widescreen slide, 13.333 by 7.5 inches
        const QString presentation = QString::fromUtf8(parts.value("ppt/presentation.xml"));
        QVERIFY(presentation.contains(R"(<p:sldSz cx="12192000" cy="6858000"/>)"));
        QVERIFY(presentation.contains(R"(<p14:section name="Verse")"));
        QVERIFY(presentation.contains(R"(<p14:section name="Chorus")"));

        const QString slide1 = QString::fromUtf8(parts.value("ppt/slides/slide1.xml"));
        // The background picture as the slide's own background, cut to the slide's shape
        QVERIFY(slide1.contains(R"(<p:bg><p:bgPr><a:blipFill dpi="0" rotWithShape="1"><a:blip r:embed="rId2"/>)"));
        // Words escaped, 72 slide units as 36 points, the font named
        QVERIFY(slide1.contains("<a:t>Made up &amp; &lt;words&gt;</a:t>"));
        QVERIFY(slide1.contains(R"(sz="3600")"));
        QVERIFY(slide1.contains(R"(<a:latin typeface="Helvetica"/>)"));
        QVERIFY(slide1.contains(R"(anchor="ctr")"));
        QVERIFY(slide1.contains(R"(<a:lnSpc><a:spcPct val=)"));
        // The shape's gradient, line and shadow
        QVERIFY(slide1.contains(R"(<a:prstGeom prst="roundRect"><a:avLst><a:gd name="adj" fmla="val 20000"/>)"));
        QVERIFY(slide1.contains(R"(<a:gs pos="100000"><a:srgbClr val="0000FF"><a:alpha val="50196"/>)"));
        QVERIFY(slide1.contains(R"(<a:lin ang="5400000" scaled="0"/>)"));
        QVERIFY(slide1.contains(R"(<a:outerShdw blurRad="76200" dist="50800" dir="2700000")"));
        // The picture fitted to its frame: 200 by 100 in a square of 400, 400 by 200 in
        // the middle of it (6350 EMUs to a unit of a 1080p slide)
        QVERIFY(slide1.contains(QString(R"(<a:off x="%1" y="%2"/><a:ext cx="%3" cy="%4"/>)").arg(1400 * 6350).arg(700 * 6350).arg(400 * 6350).arg(200 * 6350)));

        const QString slide2 = QString::fromUtf8(parts.value("ppt/slides/slide2.xml"));
        QVERIFY(slide2.contains(R"(<a:srgbClr val="0A141E"/>)"));
        // Words filled with a picture
        QVERIFY(slide2.contains(R"(<a:rPr lang=)"));
        QVERIFY(slide2.contains(R"(<a:blipFill dpi="0" rotWithShape="1"><a:blip r:embed=)"));
        QVERIFY(QString::fromUtf8(parts.value("ppt/slides/_rels/slide2.xml.rels")).contains("../notesSlides/notesSlide2.xml"));
        QVERIFY(QString::fromUtf8(parts.value("ppt/notesSlides/notesSlide2.xml")).contains("<a:t>And its second line</a:t>"));
    }

    // A presentation of someone's own, exported, only when one is given: PPTX_PRESENTATION
    // names the .pro, PPTX_MEDIA the folder its media is in (found by file name), and
    // PPTX_KEEP where the .pptx goes. What it prints is numbers and the report's notes,
    // never words.
    void aGivenPresentationIsExported()
    {
        const QString given = qEnvironmentVariable("PPTX_PRESENTATION");
        if (given.isEmpty())
            QSKIP("no presentation given (PPTX_PRESENTATION)");
        QFile file(given);
        QVERIFY(file.open(QIODevice::ReadOnly));
        rv::data::Presentation presentation;
        const QByteArray bytes = file.readAll();
        QVERIFY(presentation.ParseFromArray(bytes.constData(), int(bytes.size())));
        // (Media by its file's name anywhere under PPTX_MEDIA, as the app looks for a file
        // that is not where its presentation says)
        QHash<QString, QString> byName;
        QDirIterator files(qEnvironmentVariable("PPTX_MEDIA"), QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QString path = files.next();
            byName.insert(files.fileName().toLower(), path);
        }
        ImportContext context;
        context.findMedia = [&byName](const rv::data::URL &url) {
            const QString name = QFileInfo(QUrl(QString::fromStdString(url.absolute_string())).path()).fileName();
            return byName.value(name.toLower());
        };
        const ImportResult imported = importPresentation(presentation, QFileInfo(given).completeBaseName(), QString(), Options(), context);
        qInfo("%d slides", int(imported.deck.slides.size()));
        for (const Note &note : imported.notes)
            qInfo("note: %s (%s)", qPrintable(note.message), qPrintable(note.slidesText()));
        NoteCollector notes;
        QString error;
        const QString out = QDir(qEnvironmentVariable("PPTX_KEEP", QDir::tempPath())).filePath(QFileInfo(given).completeBaseName() + ".pptx");
        QVERIFY2(writeDeck(imported.deck, out, Options(), &notes, &error), qPrintable(error));
        for (const Note &note : notes.all())
            qInfo("note: %s (%s)", qPrintable(note.message), qPrintable(note.slidesText()));
        const QHash<QString, QByteArray> parts = partsOf(out);
        for (auto part = parts.cbegin(); part != parts.cend(); ++part) {
            if (part.key().endsWith(".xml"))
                QVERIFY2(outOfRange(QString::fromUtf8(part.value())).isEmpty(), qPrintable(part.key()));
        }
    }

    void aPresentationIsReadSlideForSlideAsProPresenterShowsIt()
    {
        QTemporaryDir folder;
        {
            QFile back(folder.filePath("Back.png"));
            QVERIFY(back.open(QIODevice::WriteOnly));
            back.write(png(QSize(192, 108), Qt::red));
        }
        const rv::data::Presentation presentation = madePresentation();
        QCOMPARE(presentation.cues_size(), 4);
        ImportContext context;
        context.findMedia = [&folder](const rv::data::URL &url) {
            const QString path = folder.filePath(QFileInfo(QUrl(QString::fromStdString(url.absolute_string())).path()).fileName());
            return QFile::exists(path) ? path : QString();
        };

        // Master: every group once, in its order; the slide turned off left out, and the
        // one no group lists
        Options options;
        ImportResult result = importPresentation(presentation, "Fallback", QString(), options, context);
        QCOMPARE(result.deck.title, "Made up");
        QCOMPARE(result.deck.slides.size(), 2);
        QVERIFY(result.arrangement.isEmpty());
        const Slide &first = result.deck.slides.at(0);
        QVERIFY(first.background.has_value());
        QVERIFY(!first.background->isCarriedOver);
        QCOMPARE(first.notes, "A note");
        // The picture behind kept on the next slide, which has none of its own
        const Slide &second = result.deck.slides.at(1);
        QVERIFY(second.background.has_value());
        QVERIFY(second.background->isCarriedOver);
        QCOMPARE(result.deck.sections.size(), 2);
        QCOMPARE(result.deck.sections.at(1).name, "Chorus");
        QCOMPARE(result.deck.sections.at(1).firstSlide, 1);

        // The words: the face not installed named as Office knows such a face, by its
        // family and its style spelt out (A GUESS at its full name)
        const Shape *words = std::get_if<Shape>(&first.items.at(0));
        QVERIFY(words && words->text);
        QCOMPARE(words->text->plainText(), "Words one");
        QCOMPARE(words->text->paragraphs.at(0).runs.at(0).style.size, 72.0);
        // (Helvetica is the RTF's font here; the made-up one is the box's record's)
        QVERIFY(words->text->insets.left() >= 5);

        // Cut out of a black box: the box keeps its black, and its letters are filled with
        // a picture of what is behind it, the red background
        const Shape *cut = std::get_if<Shape>(&second.items.at(0));
        QVERIFY(cut && cut->fill && cut->text);
        QCOMPARE(std::get<QColor>(*cut->fill), QColor(Qt::black));
        const Media *behind = std::get_if<Media>(&cut->text->paragraphs.at(0).runs.at(0).style.fill);
        QVERIFY(behind);
        const QImage picture = QImage::fromData(behind->data);
        QVERIFY(!picture.isNull());
        QCOMPARE(QColor(picture.pixel(picture.width() / 2, picture.height() / 2)), QColor(Qt::red));
        QVERIFY(std::any_of(result.notes.begin(), result.notes.end(), [](const Note &n) { return n.message.contains("cut out"); }));

        // An arrangement followed: Chorus first, which has no picture behind it yet
        result = importPresentation(presentation, "Fallback", "Chorus First", options, context);
        QCOMPARE(result.arrangement, "Chorus First");
        QCOMPARE(result.deck.slides.size(), 2);
        QVERIFY(!result.deck.slides.at(0).background.has_value());
        QVERIFY(result.deck.slides.at(1).background.has_value());

        // Turned-off slides put in, and said so
        options.skipDisabledSlides = false;
        options.carryBackgroundForward = false;
        options.includeNotes = false;
        result = importPresentation(presentation, "Fallback", QString(), options, context);
        QCOMPARE(result.deck.slides.size(), 3);
        QVERIFY(!result.deck.slides.at(1).background.has_value());
        QVERIFY(result.deck.slides.at(0).notes.isEmpty());
        QVERIFY(std::any_of(result.notes.begin(), result.notes.end(), [](const Note &n) { return n.message.contains("turned off"); }));

        // And media not found is said, by its file's name
        ImportContext nowhere;
        nowhere.findMedia = [](const rv::data::URL &) { return QString(); };
        result = importPresentation(presentation, "Fallback", QString(), Options(), nowhere);
        QVERIFY(std::any_of(result.notes.begin(), result.notes.end(), [](const Note &n) { return n.message.contains("Back.png"); }));
    }

    void aFaceThatIsNotInstalledIsNamedAsOfficeKnowsIt()
    {
        // The box's record names the family, and the RTF the face
        const QString text = QStringLiteral(R"(
cue_groups { group { uuid { string: "G" } name: "Group" } cue_identifiers { string: "A" } }
)") + QString(cueText("A", "One", true, QString(), QString(), QString(), "Some words")).replace("\\\\f0\\\\fs144", "\\\\f1\\\\b\\\\fs144");
        rv::data::Presentation presentation;
        QVERIFY(google::protobuf::TextFormat::ParseFromString(text.toStdString(), &presentation));
        const ImportResult result = importPresentation(presentation, "One", QString(), Options(), ImportContext());
        QCOMPARE(result.deck.slides.size(), 1);
        // (Groups called "Group" say nothing: no sections)
        QVERIFY(result.deck.sections.isEmpty());
        const Shape *shape = std::get_if<Shape>(&result.deck.slides.at(0).items.at(0));
        QVERIFY(shape && shape->text);
        const RunStyle &face = shape->text->paragraphs.at(0).runs.at(0).style;
        QCOMPARE(face.fontName, "MadeUpFont-CondensedBlack");
        // Its full name, and not made bolder by RTF's \b: Black is heavy already
        QCOMPARE(face.fontFamily, "Made Up Font Condensed Black");
        QVERIFY(!face.bold);
    }

    void titleCaseIsPutIntoTheWords()
    {
        const QString text = QStringLiteral(R"(
cue_groups { group { uuid { string: "G" } name: "Verse" } cue_identifiers { string: "A" } }
)") + cueText("A", "One", true, QString(), "capitalization: CAPITALIZATION_TITLE_CASE", QString(), "amazing words here");
        rv::data::Presentation presentation;
        QVERIFY(google::protobuf::TextFormat::ParseFromString(text.toStdString(), &presentation));
        const ImportResult result = importPresentation(presentation, "One", QString(), Options(), ImportContext());
        const Shape *shape = std::get_if<Shape>(&result.deck.slides.at(0).items.at(0));
        QVERIFY(shape && shape->text);
        QCOMPARE(shape->text->plainText(), "Amazing Words Here");
        QCOMPARE(shape->text->paragraphs.at(0).runs.at(0).style.capitalization, RunStyle::NoCapitalization);
    }

    void aBlurAndAnInvertOfWhatIsBehind()
    {
        // Black on the left, white on the right: blurred, the middle is grey, the edges
        // as they were; inverted, the other way round
        QImage behind(200, 20, QImage::Format_ARGB32_Premultiplied);
        behind.fill(Qt::black);
        for (int y = 0; y < 20; ++y) {
            for (int x = 100; x < 200; ++x)
                behind.setPixel(x, y, qRgb(255, 255, 255));
        }
        const QImage blurred = backgroundEffect(behind, 6.9, 1, false);
        QVERIFY(qAbs(qGray(blurred.pixel(100, 10)) - 128) < 30);
        QCOMPARE(qGray(blurred.pixel(5, 10)), 0);
        QCOMPARE(qGray(blurred.pixel(195, 10)), 255);
        const QImage inverted = backgroundEffect(behind, 0, 1, true);
        QCOMPARE(qGray(inverted.pixel(5, 10)), 255);
        QCOMPARE(qGray(inverted.pixel(195, 10)), 0);
        // Saturation: none is grey
        QImage red(4, 4, QImage::Format_ARGB32_Premultiplied);
        red.fill(Qt::red);
        const QRgb grey = backgroundEffect(red, 0, 0, false).pixel(1, 1);
        QCOMPARE(qRed(grey), qGreen(grey));
    }

    void aStoppedExportLeavesNothingBehind()
    {
        QTemporaryDir folder;
        Deck deck;
        deck.slides << Slide() << Slide() << Slide();
        const QString path = folder.filePath("Stopped.pptx");
        NoteCollector notes;
        QString error;
        QVERIFY(!writeDeck(deck, path, Options(), &notes, &error, [](int done, int) { return done < 2; }));
        QCOMPARE(error, "The export was stopped");
        QVERIFY(!QFile::exists(path));
        QCOMPARE(QDir(folder.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
        // And a file that was there before is left as it was
        {
            QFile before(path);
            QVERIFY(before.open(QIODevice::WriteOnly));
            before.write("what was there");
        }
        QVERIFY(!writeDeck(deck, path, Options(), &notes, &error, [](int, int) { return false; }));
        QFile after(path);
        QVERIFY(after.open(QIODevice::ReadOnly));
        QCOMPARE(after.readAll(), QByteArray("what was there"));
    }

    void valuesOutOfRangeAreFound()
    {
        QCOMPARE(outOfRange(R"(<a:pPr algn="l" marL="-100"/><a:rPr sz="50"/><a:rPr sz="3600"/>)"),
                 QStringList({"a:pPr marL=-100", "a:rPr sz=50"}));
    }

    void notesSayWhichSlides()
    {
        NoteCollector notes;
        notes.add(Note::MissingFont, QStringLiteral("The font X is not installed"), 2);
        notes.add(Note::MissingFont, QStringLiteral("The font X is not installed"), 9);
        notes.add(Note::MissingFont, QStringLiteral("The font X is not installed"), 5);
        notes.add(Note::Audio, QStringLiteral("A sound"));
        QCOMPARE(notes.all().size(), 2);
        QCOMPARE(notes.all().at(0).slidesText(), "slides 2, 5 and 9");
        QCOMPARE(notes.all().at(1).slidesText(), "");
    }
};

// Text is measured with Qt's fonts, which want an application with a platform: the
// offscreen one, so that the test runs where there is no desktop.
int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestPptx test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_pptx.moc"
