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
#include "pptxread.h"
#include "pptxrtf.h"
#include "pptxxml.h"
#include "pptxwriter.h"
#include "pptxzip.h"
#include "zipreader.h"

#include <QBuffer>
#include <QDebug>
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


// A PowerPoint file as PowerPoint writes one, made up for reading back (pptxread.h): a
// master with its text styles and two placeholders, a layout that moves the title, a theme,
// and three slides. The first has a coloured background, the title and body placeholders
// (the body with bullets, one run bold and in a darkened theme colour), a rounded box
// styled by the theme with its own red line and centred words in the theme's light colour,
// and a cropped picture, with speaker notes; the second is hidden, has the picture as its
// background and a green rectangle inside a group that halves its children; the third has
// an empty placeholder (which shows nothing) and a text box with no insets, bottom-aligned,
// at a line and a half, with capitals, an underline, a line break and a superscript.
QString madePowerPointFile(const QTemporaryDir &folder)
{
    const QString path = folder.filePath("Made.pptx");
    const QString ns = QString::fromUtf8(R"X(xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main")X");
    const QString relsNs = QString::fromUtf8(R"X(xmlns="http://schemas.openxmlformats.org/package/2006/relationships")X");
    const QString relType = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/");
    const auto rel = [&](const QString &id, const QString &type, const QString &target) {
        return QString::fromUtf8(R"X(<Relationship Id="%1" Type="%2%3" Target="%4"/>)X").arg(id, relType, type, target);
    };
    const auto rels = [&](const QString &inner) { return QString::fromUtf8(R"X(<?xml version="1.0" encoding="UTF-8"?><Relationships %1>%2</Relationships>)X").arg(relsNs, inner); };
    const auto sp = [](const QString &name, const QString &nvPr, const QString &spPr, const QString &style, const QString &txBody) {
        return QString::fromUtf8(R"X(<p:sp><p:nvSpPr><p:cNvPr id="2" name="%1"/><p:cNvSpPr/>%2</p:nvSpPr><p:spPr>%3</p:spPr>%4%5</p:sp>)X")
            .arg(name, nvPr.isEmpty() ? QStringLiteral("<p:nvPr/>") : QStringLiteral("<p:nvPr>%1</p:nvPr>").arg(nvPr), spPr, style, txBody);
    };
    const auto xfrm = [](qint64 x, qint64 y, qint64 cx, qint64 cy) {
        return QString::fromUtf8(R"X(<a:xfrm><a:off x="%1" y="%2"/><a:ext cx="%3" cy="%4"/></a:xfrm>)X").arg(x).arg(y).arg(cx).arg(cy);
    };
    const QString rect = QString::fromUtf8(R"X(<a:prstGeom prst="rect"><a:avLst/></a:prstGeom>)X");

    QHash<QString, QByteArray> parts;
    parts.insert("[Content_Types].xml", QByteArray(R"X(<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Default Extension="png" ContentType="image/png"/><Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/></Types>)X"));
    parts.insert("_rels/.rels", rels(rel("rId1", "officeDocument", "ppt/presentation.xml")).toUtf8());
    parts.insert("ppt/presentation.xml", QString::fromUtf8(R"X(<?xml version="1.0" encoding="UTF-8"?><p:presentation %1 xmlns:p14="http://schemas.microsoft.com/office/powerpoint/2010/main"><p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst><p:sldIdLst><p:sldId id="256" r:id="rId2"/><p:sldId id="257" r:id="rId3"/><p:sldId id="258" r:id="rId4"/></p:sldIdLst><p:sldSz cx="12192000" cy="6858000"/><p:notesSz cx="6858000" cy="9144000"/><p:defaultTextStyle><a:lvl1pPr algn="l"><a:defRPr sz="1800"/></a:lvl1pPr></p:defaultTextStyle><p:extLst><p:ext uri="{521415D9-36F7-43E2-AB2F-B90AF26B5E84}"><p14:sectionLst><p14:section name="Opening" id="{A}"><p14:sldIdLst><p14:sldId id="256"/><p14:sldId id="257"/></p14:sldIdLst></p14:section><p14:section name="Close" id="{B}"><p14:sldIdLst><p14:sldId id="258"/></p14:sldIdLst></p14:section></p14:sectionLst></p:ext></p:extLst></p:presentation>)X").arg(ns).toUtf8());
    parts.insert("ppt/_rels/presentation.xml.rels", rels(rel("rId1", "slideMaster", "slideMasters/slideMaster1.xml") + rel("rId2", "slide", "slides/slide1.xml")
                                                           + rel("rId3", "slide", "slides/slide2.xml") + rel("rId4", "slide", "slides/slide3.xml")).toUtf8());
    parts.insert("ppt/slideMasters/slideMaster1.xml", QString::fromUtf8(R"X(<?xml version="1.0" encoding="UTF-8"?><p:sldMaster %1><p:cSld><p:bg><p:bgRef idx="1001"><a:schemeClr val="bg1"/></p:bgRef></p:bg><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>%2%3</p:spTree></p:cSld><p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/><p:txStyles><p:titleStyle><a:lvl1pPr algn="l"><a:defRPr sz="4400" kern="1200"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mj-lt"/></a:defRPr></a:lvl1pPr></p:titleStyle><p:bodyStyle><a:lvl1pPr marL="228600" indent="-228600" algn="l"><a:buFont typeface="Arial"/><a:buChar char="&#8226;"/><a:defRPr sz="2800"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mn-lt"/></a:defRPr></a:lvl1pPr><a:lvl2pPr marL="685800" indent="-228600" algn="l"><a:buFont typeface="Arial"/><a:buChar char="&#8211;"/><a:defRPr sz="2400"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mn-lt"/></a:defRPr></a:lvl2pPr></p:bodyStyle><p:otherStyle><a:lvl1pPr><a:defRPr sz="1800"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill><a:latin typeface="+mn-lt"/></a:defRPr></a:lvl1pPr></p:otherStyle></p:txStyles></p:sldMaster>)X")
                                                      .arg(ns, sp("Title Placeholder 1", R"X(<p:ph type="title"/>)X", xfrm(838200, 365125, 10515600, 1325563) + rect, QString(),
                                                              R"X(<p:txBody><a:bodyPr anchor="ctr"/><a:lstStyle/><a:p><a:r><a:rPr lang="en-US"/><a:t>Click to edit</a:t></a:r></a:p></p:txBody>)X"),
                                                           sp("Text Placeholder 2", R"X(<p:ph type="body" idx="1"/>)X", xfrm(838200, 1825625, 10515600, 4351338) + rect, QString(),
                                                              R"X(<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang="en-US"/><a:t>Click to edit</a:t></a:r></a:p></p:txBody>)X")).toUtf8());
    parts.insert("ppt/slideMasters/_rels/slideMaster1.xml.rels", rels(rel("rId1", "slideLayout", "../slideLayouts/slideLayout1.xml") + rel("rId2", "theme", "../theme/theme1.xml")).toUtf8());
    parts.insert("ppt/slideLayouts/slideLayout1.xml", QString::fromUtf8(R"X(<?xml version="1.0" encoding="UTF-8"?><p:sldLayout %1 type="obj"><p:cSld name="Title and Content"><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>%2%3</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sldLayout>)X")
                                                      .arg(ns, sp("Title 1", R"X(<p:ph type="title"/>)X", xfrm(914400, 457200, 10363200, 1143000), QString(),
                                                              R"X(<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang="en-US"/><a:t>Click to edit</a:t></a:r></a:p></p:txBody>)X"),
                                                           sp("Content Placeholder 2", R"X(<p:ph idx="1"/>)X", QString(), QString(),
                                                              R"X(<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang="en-US"/><a:t>Click to edit</a:t></a:r></a:p></p:txBody>)X")).toUtf8());
    parts.insert("ppt/slideLayouts/_rels/slideLayout1.xml.rels", rels(rel("rId1", "slideMaster", "../slideMasters/slideMaster1.xml")).toUtf8());
    parts.insert("ppt/theme/theme1.xml", QString::fromUtf8(R"X(<?xml version="1.0" encoding="UTF-8"?><a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" name="Office"><a:themeElements><a:clrScheme name="Office"><a:dk1><a:sysClr val="windowText" lastClr="000000"/></a:dk1><a:lt1><a:sysClr val="window" lastClr="FFFFFF"/></a:lt1><a:dk2><a:srgbClr val="44546A"/></a:dk2><a:lt2><a:srgbClr val="E7E6E6"/></a:lt2><a:accent1><a:srgbClr val="4472C4"/></a:accent1><a:accent2><a:srgbClr val="ED7D31"/></a:accent2><a:accent3><a:srgbClr val="A5A5A5"/></a:accent3><a:accent4><a:srgbClr val="FFC000"/></a:accent4><a:accent5><a:srgbClr val="5B9BD5"/></a:accent5><a:accent6><a:srgbClr val="70AD47"/></a:accent6><a:hlink><a:srgbClr val="0563C1"/></a:hlink><a:folHlink><a:srgbClr val="954F72"/></a:folHlink></a:clrScheme><a:fontScheme name="Office"><a:majorFont><a:latin typeface="Calibri Light"/></a:majorFont><a:minorFont><a:latin typeface="Calibri"/></a:minorFont></a:fontScheme><a:fmtScheme name="Office"><a:fillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:gradFill><a:gsLst><a:gs pos="0"><a:schemeClr val="phClr"><a:lumMod val="110000"/></a:schemeClr></a:gs><a:gs pos="100000"><a:schemeClr val="phClr"><a:lumMod val="105000"/></a:schemeClr></a:gs></a:gsLst><a:lin ang="5400000"/></a:gradFill><a:gradFill><a:gsLst><a:gs pos="0"><a:schemeClr val="phClr"/></a:gs></a:gsLst></a:gradFill></a:fillStyleLst><a:lnStyleLst><a:ln w="6350"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln><a:ln w="12700"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln><a:ln w="19050"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln></a:lnStyleLst><a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle><a:effectStyle><a:effectLst/></a:effectStyle><a:effectStyle><a:effectLst/></a:effectStyle></a:effectStyleLst><a:bgFillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:solidFill><a:schemeClr val="phClr"/></a:solidFill><a:gradFill><a:gsLst><a:gs pos="0"><a:schemeClr val="phClr"/></a:gs></a:gsLst></a:gradFill></a:bgFillStyleLst></a:fmtScheme></a:themeElements></a:theme>)X").toUtf8());

    const QString boxStyle = QString::fromUtf8(R"X(<p:style><a:lnRef idx="2"><a:schemeClr val="accent1"><a:shade val="50000"/></a:schemeClr></a:lnRef><a:fillRef idx="1"><a:schemeClr val="accent1"/></a:fillRef><a:effectRef idx="0"><a:schemeClr val="accent1"/></a:effectRef><a:fontRef idx="minor"><a:schemeClr val="lt1"/></a:fontRef></p:style>)X");
    parts.insert("ppt/slides/slide1.xml", QString::fromUtf8(R"X(<?xml version="1.0" encoding="UTF-8"?><p:sld %1><p:cSld><p:bg><p:bgPr><a:solidFill><a:srgbClr val="112233"/></a:solidFill><a:effectLst/></p:bgPr></p:bg><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>%2%3%4%5</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sld>)X")
                                              .arg(ns,
                                                   sp("Title 1", R"X(<p:ph type="title"/>)X", QString(), QString(),
                                                      R"X(<p:txBody><a:bodyPr><a:normAutofit fontScale="90000"/></a:bodyPr><a:lstStyle/><a:p><a:r><a:rPr lang="en-US" dirty="0"/><a:t>Welcome</a:t></a:r></a:p></p:txBody>)X"),
                                                   sp("Content Placeholder 2", R"X(<p:ph idx="1"/>)X", QString(), QString(),
                                                      R"X(<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang="en-US" b="1" dirty="0"><a:solidFill><a:schemeClr val="accent1"><a:lumMod val="75000"/></a:schemeClr></a:solidFill></a:rPr><a:t>First point</a:t></a:r></a:p><a:p><a:pPr lvl="1"/><a:r><a:rPr lang="en-US" dirty="0"/><a:t>Sub point</a:t></a:r></a:p></p:txBody>)X"),
                                                   sp("Box 3", QString(), xfrm(6096000, 3429000, 3048000, 1524000) + R"X(<a:prstGeom prst="roundRect"><a:avLst><a:gd name="adj" fmla="val 20000"/></a:avLst></a:prstGeom><a:ln w="38100"><a:solidFill><a:srgbClr val="FF0000"/></a:solidFill></a:ln>)X", boxStyle,
                                                      R"X(<p:txBody><a:bodyPr rtlCol="0" anchor="ctr"/><a:lstStyle/><a:p><a:pPr algn="ctr"/><a:r><a:rPr lang="en-US" sz="2000"/><a:t>Box</a:t></a:r></a:p></p:txBody>)X"),
                                                   QString::fromUtf8(R"X(<p:pic><p:nvPicPr><p:cNvPr id="5" name="Picture 4"/><p:cNvPicPr><a:picLocks noChangeAspect="1"/></p:cNvPicPr><p:nvPr/></p:nvPicPr><p:blipFill><a:blip r:embed="rId2"/><a:srcRect l="10000" r="10000" b="20000"/><a:stretch><a:fillRect/></a:stretch></p:blipFill><p:spPr>%1%2</p:spPr></p:pic>)X").arg(xfrm(9144000, 457200, 2286000, 1143000), rect)).toUtf8());
    parts.insert("ppt/slides/_rels/slide1.xml.rels", rels(rel("rId1", "slideLayout", "../slideLayouts/slideLayout1.xml") + rel("rId2", "image", "../media/image1.png")
                                                          + rel("rId3", "notesSlide", "../notesSlides/notesSlide1.xml")).toUtf8());
    parts.insert("ppt/notesSlides/notesSlide1.xml", QString::fromUtf8(R"X(<?xml version="1.0" encoding="UTF-8"?><p:notes %1><p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>%2%3</p:spTree></p:cSld></p:notes>)X")
                                                   .arg(ns, sp("Slide Image Placeholder 1", R"X(<p:ph type="sldImg"/>)X", QString(), QString(), QString()),
                                                        sp("Notes Placeholder 2", R"X(<p:ph type="body" idx="1"/>)X", QString(), QString(),
                                                           R"X(<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang="en-US"/><a:t>Speaker notes here</a:t></a:r></a:p></p:txBody>)X")).toUtf8());
    parts.insert("ppt/slides/slide2.xml", QString::fromUtf8(R"X(<?xml version="1.0" encoding="UTF-8"?><p:sld %1 show="0"><p:cSld><p:bg><p:bgPr><a:blipFill dpi="0" rotWithShape="1"><a:blip r:embed="rId2"/><a:stretch><a:fillRect/></a:stretch></a:blipFill><a:effectLst/></p:bgPr></p:bg><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/><p:grpSp><p:nvGrpSpPr><p:cNvPr id="4" name="Group 3"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="1524000" y="1524000"/><a:ext cx="3048000" cy="1524000"/><a:chOff x="0" y="0"/><a:chExt cx="6096000" cy="3048000"/></a:xfrm></p:grpSpPr>%2</p:grpSp></p:spTree></p:cSld></p:sld>)X")
                                              .arg(ns, sp("Inner", QString(), xfrm(1524000, 0, 3048000, 1524000) + rect + R"X(<a:solidFill><a:srgbClr val="00FF00"/></a:solidFill>)X", QString(),
                                                          R"X(<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang="en-US"/><a:t>Inner</a:t></a:r></a:p></p:txBody>)X")).toUtf8());
    parts.insert("ppt/slides/_rels/slide2.xml.rels", rels(rel("rId1", "slideLayout", "../slideLayouts/slideLayout1.xml") + rel("rId2", "image", "../media/image1.png")).toUtf8());
    parts.insert("ppt/slides/slide3.xml", QString::fromUtf8(R"X(<?xml version="1.0" encoding="UTF-8"?><p:sld %1><p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>%2%3</p:spTree></p:cSld></p:sld>)X")
                                              .arg(ns, sp("Content Placeholder 2", R"X(<p:ph idx="1"/>)X", QString(), QString(), R"X(<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:endParaRPr lang="en-US"/></a:p></p:txBody>)X"),
                                                   QString::fromUtf8(R"X(<p:sp><p:nvSpPr><p:cNvPr id="3" name="TextBox 2"/><p:cNvSpPr txBox="1"/><p:nvPr/></p:nvSpPr><p:spPr>%1%2</p:spPr><p:txBody><a:bodyPr wrap="square" lIns="0" tIns="0" rIns="0" bIns="0" anchor="b"/><a:lstStyle/><a:p><a:pPr><a:lnSpc><a:spcPct val="150000"/></a:lnSpc><a:spcBef><a:spcPts val="600"/></a:spcBef></a:pPr><a:r><a:rPr lang="en-US" sz="2400" u="sng" cap="all"/><a:t>Loud</a:t></a:r><a:br><a:rPr lang="en-US" sz="2400"/></a:br><a:r><a:rPr lang="en-US" sz="2400" baseline="30000"/><a:t>Soft</a:t></a:r><a:endParaRPr lang="en-US" sz="2400"/></a:p></p:txBody></p:sp>)X").arg(xfrm(1000000, 1000000, 4000000, 2000000), rect)).toUtf8());
    parts.insert("ppt/slides/_rels/slide3.xml.rels", rels(rel("rId1", "slideLayout", "../slideLayouts/slideLayout1.xml")).toUtf8());
    parts.insert("ppt/media/image1.png", png(QSize(40, 20), Qt::green));

    PptxZipWriter zip;
    QString error;
    if (!zip.open(path, &error))
        return {};
    for (auto it = parts.cbegin(); it != parts.cend(); ++it) {
        if (!zip.add(it.key(), it.value(), it.key().endsWith(".png") ? PptxZipWriter::Store : PptxZipWriter::Deflate, &error))
            return {};
    }
    if (!zip.finish(&error))
        return {};
    return path;
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

    // ---- Import PowerPoint: the reader (pptxread.h)

    void aPowerPointFileIsReadIntoADeck()
    {
        QTemporaryDir folder;
        const QString path = madePowerPointFile(folder);
        QVERIFY(!path.isEmpty());
        const double u = 1080.0 / 6858000; // units an EMU
        ReadResult read = readDeck(path);
        QVERIFY2(read.error.isEmpty(), qPrintable(read.error));
        const Deck &deck = read.deck;
        QCOMPARE(deck.title, "Made");
        QCOMPARE(deck.size, QSizeF(1920, 1080));
        QCOMPARE(deck.slides.size(), 3);
        QCOMPARE(deck.sections.size(), 2);
        QCOMPARE(deck.sections.at(0).name, "Opening");
        QCOMPARE(deck.sections.at(0).firstSlide, 0);
        QCOMPARE(deck.sections.at(1).name, "Close");
        QCOMPARE(deck.sections.at(1).firstSlide, 2);
        QVERIFY(!deck.slides.at(0).hidden);
        QVERIFY(deck.slides.at(1).hidden);

        // The first slide: its own colour behind, four things on it, its notes
        const Slide &first = deck.slides.at(0);
        QVERIFY(first.backgroundColor.has_value());
        QCOMPARE(first.backgroundColor->name(), "#112233");
        QVERIFY(!first.background.has_value());
        QCOMPARE(first.notes, "Speaker notes here");
        QCOMPARE(first.items.size(), 4);

        // The title: its frame from the layout's placeholder (the master's is moved there),
        // its size from the master's title style, shrunk as PowerPoint shrank it; the
        // theme's major font; the theme's dark text colour
        const Shape *title = std::get_if<Shape>(&first.items.at(0));
        QVERIFY(title && title->text);
        QCOMPARE(title->name, "Title 1");
        QCOMPARE(title->frame, QRectF(914400 * u, 457200 * u, 10363200 * u, 1143000 * u));
        QCOMPARE(title->text->plainText(), "Welcome");
        QCOMPARE(title->text->autofit, TextBody::Shrink);
        const RunStyle &titleStyle = title->text->paragraphs.at(0).runs.at(0).style;
        QVERIFY2(qAbs(titleStyle.size - 44 * 2 * 0.9) < 0.01, qPrintable(QString::number(titleStyle.size)));
        QCOMPARE(titleStyle.fontFamily, "Calibri Light");
        QCOMPARE(std::get<QColor>(titleStyle.fill).name(), "#000000");
        QCOMPARE(title->text->paragraphs.at(0).alignment, Paragraph::Left);
        QVERIFY(!title->text->paragraphs.at(0).bullet.has_value());
        // (PowerPoint's insets where nothing says: a tenth of an inch, a twentieth)
        QCOMPARE(title->text->insets, QMarginsF(91440 * u, 45720 * u, 91440 * u, 45720 * u));
        QCOMPARE(title->text->verticalAlignment, TextBody::Middle);

        // The body: the master's frame (the layout's placeholder has none of its own), the
        // master's bullets at each level, a run bold and in the darkened theme colour
        const Shape *body = std::get_if<Shape>(&first.items.at(1));
        QVERIFY(body && body->text);
        QCOMPARE(body->frame, QRectF(838200 * u, 1825625 * u, 10515600 * u, 4351338 * u));
        QCOMPARE(body->text->paragraphs.size(), 2);
        const Paragraph &point = body->text->paragraphs.at(0);
        QVERIFY(point.bullet.has_value());
        QCOMPARE(point.bullet->character, QString(QChar(0x2022)));
        QCOMPARE(point.runs.at(0).text, "First point");
        QVERIFY(point.runs.at(0).style.bold);
        QCOMPARE(point.runs.at(0).style.size, 56.0);
        QCOMPARE(point.runs.at(0).style.fontFamily, "Calibri");
        QColor accent(0x44, 0x72, 0xc4);
        float h = 0, s = 0, l = 0;
        accent.getHslF(&h, &s, &l);
        accent.setHslF(h, s, l * 0.75f);
        QCOMPARE(std::get<QColor>(point.runs.at(0).style.fill).name(), accent.name());
        QCOMPARE(point.leftIndent, 228600 * u);
        const Paragraph &sub = body->text->paragraphs.at(1);
        QCOMPARE(sub.bullet->character, QString(QChar(0x2013)));
        QCOMPARE(sub.runs.at(0).style.size, 48.0);
        QVERIFY(!sub.runs.at(0).style.bold);

        // The box: rounded by a fifth, filled by the theme's style in its own colour, its
        // own red line over the style's, centred words in the style's light colour
        const Shape *box = std::get_if<Shape>(&first.items.at(2));
        QVERIFY(box && box->text);
        QCOMPARE(box->geometry, Shape::RoundedRectangle);
        QCOMPARE(box->cornerFraction, 0.2);
        QCOMPARE(box->frame, QRectF(960, 540, 480, 240));
        QVERIFY(box->fill.has_value());
        QCOMPARE(std::get<QColor>(*box->fill).name(), "#4472c4");
        QVERIFY(box->line.has_value());
        QCOMPARE(box->line->color.name(), "#ff0000");
        QCOMPARE(box->line->width, 6.0);
        QCOMPARE(box->text->plainText(), "Box");
        QCOMPARE(box->text->paragraphs.at(0).alignment, Paragraph::Center);
        QCOMPARE(box->text->paragraphs.at(0).runs.at(0).style.size, 40.0);
        QCOMPARE(std::get<QColor>(box->text->paragraphs.at(0).runs.at(0).style.fill).name(), "#ffffff");
        QCOMPARE(box->text->paragraphs.at(0).runs.at(0).style.fontFamily, "Calibri");

        // The picture: cropped, placed, its file taken out
        const Picture *picture = std::get_if<Picture>(&first.items.at(3));
        QVERIFY(picture);
        QCOMPARE(picture->name, "Picture 4");
        QCOMPARE(picture->frame, QRectF(1440, 72, 360, 180));
        QCOMPARE(picture->crop, QMarginsF(0.1, 0, 0.1, 0.2));
        QCOMPARE(picture->media.fileName, "image1.png");
        QVERIFY(QFile::exists(picture->media.path));
        QCOMPARE(picture->media.naturalSize, QSizeF(40, 20));
        QCOMPARE(QImage(picture->media.path).pixelColor(5, 5).name(), "#00ff00");

        // The second slide: the picture behind it, and the grouped rectangle halved and
        // moved with its group
        const Slide &second = deck.slides.at(1);
        QVERIFY(second.background.has_value());
        QCOMPARE(second.background->media.fileName, "image1.png");
        QCOMPARE(second.background->media.path, picture->media.path);
        QCOMPARE(second.items.size(), 1);
        const Shape *inner = std::get_if<Shape>(&second.items.at(0));
        QVERIFY(inner);
        QCOMPARE(inner->frame, QRectF(360, 240, 240, 120));
        QCOMPARE(std::get<QColor>(*inner->fill).name(), "#00ff00");

        // The third: the empty placeholder shows nothing; the text box with no insets,
        // bottom-aligned, at a line and a half (as a multiple of the font's own line
        // height), with space before, capitals, an underline, a break and a superscript
        const Slide &third = deck.slides.at(2);
        QVERIFY(!third.backgroundColor.has_value() || third.backgroundColor->name() == "#ffffff");
        QCOMPARE(third.items.size(), 1);
        const Shape *textBox = std::get_if<Shape>(&third.items.at(0));
        QVERIFY(textBox && textBox->text);
        QVERIFY(!textBox->fill.has_value());
        QCOMPARE(textBox->text->insets, QMarginsF(0, 0, 0, 0));
        QCOMPARE(textBox->text->verticalAlignment, TextBody::Bottom);
        const Paragraph &loud = textBox->text->paragraphs.at(0);
        QVERIFY(loud.lineSpacing.has_value());
        QCOMPARE(loud.lineSpacing->kind, Paragraph::LineSpacing::Multiple);
        QVERIFY2(loud.lineSpacing->value > 1.3 && loud.lineSpacing->value < 1.9, qPrintable(QString::number(loud.lineSpacing->value)));
        QCOMPARE(loud.spaceBefore, 12.0);
        QCOMPARE(loud.runs.size(), 2);
        QCOMPARE(loud.runs.at(0).text, QStringLiteral("Loud") + QChar(0x2028));
        QCOMPARE(loud.runs.at(0).style.capitalization, RunStyle::AllCaps);
        QVERIFY(loud.runs.at(0).style.underline);
        QCOMPARE(loud.runs.at(0).style.size, 48.0);
        QCOMPARE(loud.runs.at(1).text, "Soft");
        QCOMPARE(loud.runs.at(1).style.superscript, 1);

        // The master's background, white, where a slide has none of its own
        QVERIFY(third.backgroundColor.has_value());
        QCOMPARE(third.backgroundColor->name(), "#ffffff");

        // A glance says what the file holds without reading the slides
        const Glance glance = glanceAt(path);
        QVERIFY(glance.error.isEmpty());
        QCOMPARE(glance.name, "Made");
        QCOMPARE(glance.slides, 3);
        QCOMPARE(glance.size, QSizeF(1920, 1080));
    }

    void aFileThatIsNotPowerPointIsRefused()
    {
        QTemporaryDir folder;
        const QString path = folder.filePath("Not.pptx");
        PptxZipWriter zip;
        QString error;
        QVERIFY(zip.open(path, &error));
        QVERIFY(zip.add("hello.txt", "hello", PptxZipWriter::Store, &error));
        QVERIFY(zip.finish(&error));
        const ReadResult read = readDeck(path);
        QVERIFY(read.error.contains("not a PowerPoint file"));
        QVERIFY(glanceAt(path).error.contains("not a PowerPoint file"));
        QVERIFY(!readDeck(folder.filePath("missing.pptx")).error.isEmpty());
    }

    void aDeckWrittenComesBackWhenRead()
    {
        // The export's own file read back: the round trip, slide for slide
        QTemporaryDir folder;
        {
            QFile back(folder.filePath("Back.png"));
            QVERIFY(back.open(QIODevice::WriteOnly));
            back.write(png(QSize(192, 108), Qt::red));
        }
        const rv::data::Presentation presentation = madePresentation();
        ImportContext context;
        context.findMedia = [&folder](const rv::data::URL &url) {
            const QString path = folder.filePath(QFileInfo(QUrl(QString::fromStdString(url.absolute_string())).path()).fileName());
            return QFile::exists(path) ? path : QString();
        };
        Options options;
        const ImportResult imported = importPresentation(presentation, "Fallback", QString(), options, context);
        QCOMPARE(imported.deck.slides.size(), 2);
        const QString path = folder.filePath("Round.pptx");
        NoteCollector notes;
        QString error;
        QVERIFY2(writeDeck(imported.deck, path, options, &notes, &error), qPrintable(error));

        const ReadResult read = readDeck(path);
        QVERIFY2(read.error.isEmpty(), qPrintable(read.error));
        QCOMPARE(read.deck.slides.size(), 2);
        QCOMPARE(read.deck.size, imported.deck.size);
        QCOMPARE(read.deck.sections.size(), 2);
        QCOMPARE(read.deck.sections.at(1).name, "Chorus");
        QCOMPARE(read.deck.sections.at(1).firstSlide, 1);
        // The picture behind the first slide, as the slide's background, comes back as media
        QVERIFY(read.deck.slides.at(0).background.has_value());
        QVERIFY(QFile::exists(read.deck.slides.at(0).background->media.path));
        QCOMPARE(QImage(read.deck.slides.at(0).background->media.path).size(), QSize(192, 108));
        // The words, where they were, at their size
        const Shape *before = std::get_if<Shape>(&imported.deck.slides.at(0).items.at(0));
        const Shape *after = nullptr;
        for (const Item &item : read.deck.slides.at(0).items) {
            if (const Shape *shape = std::get_if<Shape>(&item)) {
                if (shape->text && shape->text->plainText() == "Words one")
                    after = shape;
            }
        }
        QVERIFY(before && before->text);
        QVERIFY2(after, "the words were not read back");
        QVERIFY2(qAbs(after->frame.x() - before->frame.x()) < 1 && qAbs(after->frame.y() - before->frame.y()) < 1
                     && qAbs(after->frame.width() - before->frame.width()) < 1 && qAbs(after->frame.height() - before->frame.height()) < 1,
                 qPrintable(QStringLiteral("%1 against %2").arg(QDebug::toString(after->frame), QDebug::toString(before->frame))));
        QVERIFY(qAbs(after->text->paragraphs.at(0).runs.at(0).style.size - 72) < 0.5);
        QCOMPARE(after->text->paragraphs.at(0).alignment, before->text->paragraphs.at(0).alignment);
        // (The only note there can be is of a font not installed here: the fixture's Helvetica)
        for (const Note &note : read.notes)
            QCOMPARE(note.kind, Note::MissingFont);
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
