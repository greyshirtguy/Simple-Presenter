#include "pptxread.h"

#include "fontresolver.h"
#include "pptxtext.h"
#include "pptxxml.h"
#include "zipreader.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QSet>
#include <QtMath>

#include <cmath>

namespace pptx {

namespace {

// ---- The package: the zip, its parts and their relationships

const QString relTypeBase = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships/");

struct Relationship
{
    QString type; // the last part of the type's address: "slide", "slideLayout", "image"
    QString target; // the part's name in the zip, or the address of an external file
    bool external = false;
};

struct Part
{
    QString name;
    XmlNode xml;
    QHash<QString, Relationship> rels;
};

// The name of a part reached from `from` by a relationship's target: "../slideLayouts/x.xml"
// from "ppt/slides/slide1.xml" is "ppt/slideLayouts/x.xml". A target starting with "/" is
// from the root.
QString resolvePartName(const QString &from, const QString &target)
{
    if (target.startsWith(u'/'))
        return QDir::cleanPath(target.mid(1));
    const int slash = from.lastIndexOf(u'/');
    const QString folder = slash < 0 ? QString() : from.left(slash);
    return QDir::cleanPath(folder.isEmpty() ? target : folder + u'/' + target);
}

QString relsNameOf(const QString &partName)
{
    const int slash = partName.lastIndexOf(u'/');
    const QString folder = slash < 0 ? QString() : partName.left(slash + 1);
    return folder + QStringLiteral("_rels/") + partName.mid(slash + 1) + QStringLiteral(".rels");
}

class Package
{
public:
    bool open(const QString &path, QString *error)
    {
        if (!m_zip.open(path, error))
            return false;
        for (const ZipReader::Entry &entry : m_zip.entries())
            m_entries.insert(entry.name, &entry);
        return true;
    }

    bool has(const QString &name) const { return m_entries.contains(name); }

    bool bytes(const QString &name, QByteArray *data, QString *error)
    {
        const ZipReader::Entry *entry = m_entries.value(name);
        if (!entry) {
            *error = QStringLiteral("the file has no part named %1").arg(name);
            return false;
        }
        return m_zip.read(*entry, data, error);
    }

    bool extract(const QString &name, const QString &destination, QString *error)
    {
        const ZipReader::Entry *entry = m_entries.value(name);
        if (!entry) {
            *error = QStringLiteral("the file has no part named %1").arg(name);
            return false;
        }
        return m_zip.extract(*entry, destination, error);
    }

    // A part parsed, with its relationships, kept once read; null with *error set where
    // it cannot be
    const Part *part(const QString &name, QString *error)
    {
        if (m_parts.contains(name))
            return m_parts.value(name).get();
        QByteArray data;
        if (!bytes(name, &data, error))
            return nullptr;
        auto made = std::make_shared<Part>();
        made->name = name;
        QString why;
        if (!parseXml(data, &made->xml, &why)) {
            *error = QStringLiteral("%1 could not be read: %2").arg(name, why);
            return nullptr;
        }
        const QString relsName = relsNameOf(name);
        if (has(relsName)) {
            QByteArray relsData;
            XmlNode rels;
            if (bytes(relsName, &relsData, &why) && parseXml(relsData, &rels, &why)) {
                for (const XmlNode *rel : rels.each("Relationship")) {
                    Relationship r;
                    QString type = rel->attr("Type");
                    r.type = type.mid(type.lastIndexOf(u'/') + 1);
                    r.external = rel->attr("TargetMode") == QLatin1String("External");
                    r.target = r.external ? rel->attr("Target") : resolvePartName(name, rel->attr("Target"));
                    made->rels.insert(rel->attr("Id"), r);
                }
            }
        }
        m_parts.insert(name, made);
        return made.get();
    }

    // The first relationship of a part of this type, or null
    static const Relationship *relOfType(const Part &part, const char *type)
    {
        const QLatin1StringView wanted(type);
        for (auto it = part.rels.cbegin(); it != part.rels.cend(); ++it) {
            if (it.value().type == wanted)
                return &it.value();
        }
        return nullptr;
    }

private:
    ZipReader m_zip;
    QHash<QString, const ZipReader::Entry *> m_entries;
    QHash<QString, std::shared_ptr<Part>> m_parts;
};

// ---- Colours

struct Theme
{
    // The scheme's colours by name ("dk1", "accent1", ...)
    QHash<QString, QColor> colors;
    QString majorFont;
    QString minorFont;
    // The theme's fill, line and background fill styles, as the shapes' style references
    // count them (from 1; backgrounds from 1001)
    QList<const XmlNode *> fillStyles;
    QList<const XmlNode *> lineStyles;
    QList<const XmlNode *> backgroundFillStyles;
    const Part *part = nullptr;
};

struct ColorContext
{
    const Theme *theme = nullptr;
    // The colour map: "bg1" -> "lt1", "tx1" -> "dk1", and so on
    QHash<QString, QString> map;
    // What "phClr" stands for, in a style the theme gives a shape: the shape's own colour
    std::optional<QColor> placeholderColor;
};

// Named colours DrawingML takes (a:prstClr), the ones met in decks
QColor presetColor(const QString &name)
{
    static const QHash<QString, QColor> colors {
        {QStringLiteral("black"), QColor(Qt::black)},       {QStringLiteral("white"), QColor(Qt::white)},
        {QStringLiteral("red"), QColor(Qt::red)},           {QStringLiteral("green"), QColor(0, 128, 0)},
        {QStringLiteral("blue"), QColor(Qt::blue)},         {QStringLiteral("yellow"), QColor(Qt::yellow)},
        {QStringLiteral("gray"), QColor(128, 128, 128)},    {QStringLiteral("grey"), QColor(128, 128, 128)},
        {QStringLiteral("orange"), QColor(255, 165, 0)},    {QStringLiteral("purple"), QColor(128, 0, 128)},
        {QStringLiteral("ltGray"), QColor(211, 211, 211)},  {QStringLiteral("dkGray"), QColor(169, 169, 169)},
    };
    return colors.value(name, QColor(Qt::black));
}

// The colour modifiers a colour's children apply (KNOWN: the standard's a:lumMod and the
// rest; how PowerPoint does tint and shade exactly is not in the standard in plain words,
// and the straight blend towards white or black is A GUESS that is close on screen)
QColor modified(QColor color, const XmlNode &node)
{
    for (const XmlNode &mod : node.children) {
        const double val = mod.number("val") / 100000.0;
        if (mod.name == QLatin1String("alpha")) {
            color.setAlphaF(std::clamp(val, 0.0, 1.0));
        } else if (mod.name == QLatin1String("lumMod") || mod.name == QLatin1String("lumOff")) {
            float h = 0, s = 0, l = 0, a = 1;
            color.getHslF(&h, &s, &l, &a);
            l = mod.name == QLatin1String("lumMod") ? l * float(val) : l + float(val);
            color.setHslF(h, s, std::clamp(l, 0.0f, 1.0f), a);
        } else if (mod.name == QLatin1String("tint")) {
            // Towards white: the colour keeps `val` of itself
            const float a = color.alphaF();
            color = QColor::fromRgbF(1 - (1 - color.redF()) * val, 1 - (1 - color.greenF()) * val, 1 - (1 - color.blueF()) * val, a);
        } else if (mod.name == QLatin1String("shade")) {
            const float a = color.alphaF();
            color = QColor::fromRgbF(color.redF() * val, color.greenF() * val, color.blueF() * val, a);
        } else if (mod.name == QLatin1String("satMod")) {
            float h = 0, s = 0, l = 0, a = 1;
            color.getHslF(&h, &s, &l, &a);
            color.setHslF(h, std::clamp(s * float(val), 0.0f, 1.0f), l, a);
        }
    }
    return color;
}

// A colour element (a:srgbClr, a:schemeClr, a:sysClr, a:prstClr, a:scrgbClr), or none
std::optional<QColor> colorOf(const XmlNode *node, const ColorContext &context)
{
    if (!node)
        return std::nullopt;
    QColor color;
    if (node->name == QLatin1String("srgbClr")) {
        color = QColor(u'#' + node->attr("val"));
    } else if (node->name == QLatin1String("schemeClr")) {
        QString name = node->attr("val");
        if (name == QLatin1String("phClr")) {
            color = context.placeholderColor.value_or(QColor(Qt::black));
        } else {
            name = context.map.value(name, name);
            color = context.theme ? context.theme->colors.value(name, QColor(Qt::black)) : QColor(Qt::black);
        }
    } else if (node->name == QLatin1String("sysClr")) {
        const QString last = node->attr("lastClr");
        color = last.isEmpty() ? (node->attr("val") == QLatin1String("window") ? QColor(Qt::white) : QColor(Qt::black)) : QColor(u'#' + last);
    } else if (node->name == QLatin1String("prstClr")) {
        color = presetColor(node->attr("val"));
    } else if (node->name == QLatin1String("scrgbClr")) {
        color = QColor::fromRgbF(node->number("r") / 100000.0, node->number("g") / 100000.0, node->number("b") / 100000.0);
    } else {
        return std::nullopt;
    }
    if (!color.isValid())
        color = QColor(Qt::black);
    return modified(color, *node);
}

// The colour inside a solid fill (or any element whose child is a colour)
std::optional<QColor> innerColor(const XmlNode *node, const ColorContext &context)
{
    if (!node)
        return std::nullopt;
    for (const XmlNode &child : node->children) {
        if (const std::optional<QColor> color = colorOf(&child, context))
            return color;
    }
    return std::nullopt;
}

Gradient gradientOf(const XmlNode &gradFill, const ColorContext &context)
{
    Gradient gradient;
    if (const XmlNode *stops = gradFill.child("gsLst")) {
        for (const XmlNode *stop : stops->each("gs")) {
            Gradient::Stop made;
            made.position = std::clamp(stop->number("pos") / 100000.0, 0.0, 1.0);
            made.color = innerColor(stop, context).value_or(QColor(Qt::black));
            gradient.stops.append(made);
        }
    }
    if (const XmlNode *lin = gradFill.child("lin")) {
        gradient.angle = lin->number("ang") / 60000.0;
    } else if (gradFill.child("path")) {
        gradient.radial = true;
    }
    if (gradient.stops.isEmpty())
        gradient.stops = {{QColor(Qt::white), 0}, {QColor(Qt::black), 1}};
    return gradient;
}

// ---- Measures

// Lengths: EMUs to deck units, by the slide's height
struct Measure
{
    double unitsPerEmu = 1080.0 / 6858000;

    double length(double emu) const { return emu * unitsPerEmu; }
    double length(const XmlNode &node, const char *attr, double fallbackEmu = 0) const { return length(node.number(attr, fallbackEmu)); }
    // Hundredths of a point, as font sizes and letter spacing are given
    double centipoints(double value) const { return value / 100 * 12700 * unitsPerEmu; }
    QRectF rect(const XmlNode *xfrm) const
    {
        if (!xfrm)
            return {};
        const XmlNode *off = xfrm->child("off");
        const XmlNode *ext = xfrm->child("ext");
        if (!off || !ext)
            return {};
        return QRectF(length(*off, "x"), length(*off, "y"), length(*ext, "cx"), length(*ext, "cy"));
    }
};

// ---- What a slide, a layout and a master have in common: a sheet, with placeholders
// standing in for shapes of the sheets above it

struct Placeholder
{
    QString type; // "title", "body", "obj", "ctrTitle", "subTitle", "dt", "ftr", "sldNum", "pic", ...
    QString idx; // "" where none
    const XmlNode *shape = nullptr;
};

QString placeholderType(const XmlNode &ph)
{
    // (KNOWN: a placeholder with no type is a body one, "obj")
    return ph.attr("type", QStringLiteral("obj"));
}

// Whether two placeholder types stand in for each other, where no index says which
bool typesMatch(const QString &a, const QString &b)
{
    const auto family = [](const QString &type) {
        if (type == QLatin1String("title") || type == QLatin1String("ctrTitle"))
            return QStringLiteral("title");
        if (type == QLatin1String("body") || type == QLatin1String("subTitle") || type == QLatin1String("obj"))
            return QStringLiteral("body");
        return type;
    };
    return family(a) == family(b);
}

struct Sheet
{
    const Part *part = nullptr;
    QList<Placeholder> placeholders;
    const XmlNode *background = nullptr; // p:bg, if the sheet has one
    // The master's text styles (only a master has them)
    const XmlNode *titleStyle = nullptr;
    const XmlNode *bodyStyle = nullptr;
    const XmlNode *otherStyle = nullptr;
    // The master's colour map, or the layout's or slide's override of it
    QHash<QString, QString> colorMap;
    bool hasColorMap = false;
    const Theme *theme = nullptr;
    const Sheet *parent = nullptr; // a layout's master; a master has none

    const Placeholder *placeholder(const QString &type, const QString &idx) const
    {
        // By index first, then by type (KNOWN: how the standard has it)
        if (!idx.isEmpty()) {
            for (const Placeholder &p : placeholders) {
                if (p.idx == idx)
                    return &p;
            }
        }
        for (const Placeholder &p : placeholders) {
            if (typesMatch(p.type, type))
                return &p;
        }
        return nullptr;
    }
};

QHash<QString, QString> colorMapOf(const XmlNode *clrMap)
{
    QHash<QString, QString> map;
    if (!clrMap)
        return map;
    for (const XmlAttribute &attribute : clrMap->attributes)
        map.insert(attribute.name, attribute.value);
    return map;
}

void collectPlaceholders(const XmlNode &tree, QList<Placeholder> *out)
{
    for (const XmlNode &node : tree.children) {
        if (node.name == QLatin1String("sp") || node.name == QLatin1String("pic")) {
            const char *nv = node.name == QLatin1String("sp") ? "nvSpPr" : "nvPicPr";
            if (const XmlNode *ph = node.find({nv, "nvPr", "ph"}))
                out->append({placeholderType(*ph), ph->attr("idx"), &node});
        } else if (node.name == QLatin1String("grpSp")) {
            collectPlaceholders(node, out);
        }
    }
}

// ---- Text styles, from the most particular to the most general

// The paragraph properties of a level in a list style (a:lstStyle, p:titleStyle, ...)
const XmlNode *levelOf(const XmlNode *list, int level)
{
    if (!list)
        return nullptr;
    const QString name = QStringLiteral("lvl%1pPr").arg(level + 1);
    for (const XmlNode &child : list->children) {
        if (child.name == name)
            return &child;
    }
    return nullptr;
}

// Where a run's and a paragraph's properties are looked for, in order
struct StyleChain
{
    // The paragraph's own properties, where it has any
    const XmlNode *paragraph = nullptr;
    // List styles, the shape's own first, then the layout's and master's placeholders',
    // then the master's text style for the kind of shape, then the presentation's
    QList<const XmlNode *> lists;
    // A shape style's font reference (p:style/a:fontRef): the colour and the font it
    // gives, which come after the shape's and its placeholders' own lists and before the
    // master's styles (A GUESS at PowerPoint's order, which draws a new rectangle's words
    // in the theme's light colour though the master's other style says dark)
    int fontReferenceAfter = -1;
    const XmlNode *fontReference = nullptr;
    int level = 0;

    // The first paragraph properties up the chain that have `child`
    const XmlNode *paragraphChild(const char *child) const
    {
        if (paragraph && paragraph->child(child))
            return paragraph->child(child);
        for (const XmlNode *list : lists) {
            if (const XmlNode *props = levelOf(list, level)) {
                if (const XmlNode *found = props->child(child))
                    return found;
            }
        }
        return nullptr;
    }
    // Whether one of the bullet choices is made up the chain, and which
    const XmlNode *bulletChoice() const
    {
        const auto choiceIn = [](const XmlNode *props) -> const XmlNode * {
            if (!props)
                return nullptr;
            for (const char *name : {"buNone", "buChar", "buAutoNum", "buBlip"}) {
                if (const XmlNode *found = props->child(name))
                    return found;
            }
            return nullptr;
        };
        if (const XmlNode *found = choiceIn(paragraph))
            return found;
        for (const XmlNode *list : lists) {
            if (const XmlNode *found = choiceIn(levelOf(list, level)))
                return found;
        }
        return nullptr;
    }
    QString paragraphAttr(const char *attr) const
    {
        if (paragraph && paragraph->has(attr))
            return paragraph->attr(attr);
        for (const XmlNode *list : lists) {
            if (const XmlNode *props = levelOf(list, level)) {
                if (props->has(attr))
                    return props->attr(attr);
            }
        }
        return {};
    }
    // The run properties up the chain (the paragraph's own `rPr` first, given by the
    // caller) that have `attr`, or `child`; the font reference comes in at its place
    QString runAttr(const XmlNode *rPr, const char *attr) const
    {
        if (rPr && rPr->has(attr))
            return rPr->attr(attr);
        for (const XmlNode *list : lists) {
            if (const XmlNode *props = levelOf(list, level)) {
                if (const XmlNode *def = props->child("defRPr")) {
                    if (def->has(attr))
                        return def->attr(attr);
                }
            }
        }
        return {};
    }
    const XmlNode *runChild(const XmlNode *rPr, const char *child, bool *fromReference = nullptr) const
    {
        if (fromReference)
            *fromReference = false;
        if (rPr && rPr->child(child))
            return rPr->child(child);
        for (int k = 0; k < lists.size(); ++k) {
            if (k == fontReferenceAfter && fontReference) {
                if (fromReference)
                    *fromReference = true;
                return fontReference;
            }
            if (const XmlNode *props = levelOf(lists.at(k), level)) {
                if (const XmlNode *def = props->child("defRPr")) {
                    if (const XmlNode *found = def->child(child))
                        return found;
                }
            }
        }
        if (fontReferenceAfter >= lists.size() && fontReference) {
            if (fromReference)
                *fromReference = true;
            return fontReference;
        }
        return nullptr;
    }
};

// ---- Preset shapes: the outlines of those PowerPoint has that this app can draw as a
// polygon (KNOWN: the standard's presetShapeDefinitions, simplified to their corners;
// the adjustments are the default ones unless the shape gives its own)

double adjustment(const XmlNode *prstGeom, const char *name, double fallback)
{
    if (!prstGeom)
        return fallback;
    if (const XmlNode *list = prstGeom->child("avLst")) {
        for (const XmlNode *gd : list->each("gd")) {
            if (gd->attr("name") == QLatin1String(name)) {
                const QString formula = gd->attr("fmla");
                if (formula.startsWith(QLatin1String("val ")))
                    return formula.mid(4).toDouble();
            }
        }
    }
    return fallback;
}

// Points on the unit square for a preset, or none for one this has no outline for.
// `w` and `h` are the shape's size, since some adjustments go by the shorter side.
std::optional<QList<QPointF>> presetOutline(const QString &preset, const XmlNode *prstGeom, double w, double h)
{
    const double ss = std::max(1.0, std::min(w, h));
    w = std::max(1.0, w);
    h = std::max(1.0, h);
    const auto fraction = [&](const char *name, double fallback) { return adjustment(prstGeom, name, fallback) / 100000.0; };
    if (preset == QLatin1String("triangle")) {
        const double apex = std::clamp(fraction("adj", 50000), 0.0, 1.0);
        return QList<QPointF> {{apex, 0}, {1, 1}, {0, 1}};
    }
    if (preset == QLatin1String("rtTriangle"))
        return QList<QPointF> {{0, 0}, {1, 1}, {0, 1}};
    if (preset == QLatin1String("diamond"))
        return QList<QPointF> {{0.5, 0}, {1, 0.5}, {0.5, 1}, {0, 0.5}};
    if (preset == QLatin1String("parallelogram")) {
        const double a = std::clamp(fraction("adj", 25000) * ss / w, 0.0, 0.5);
        return QList<QPointF> {{a, 0}, {1, 0}, {1 - a, 1}, {0, 1}};
    }
    if (preset == QLatin1String("trapezoid")) {
        const double a = std::clamp(fraction("adj", 25000) * ss / w, 0.0, 0.5);
        return QList<QPointF> {{a, 0}, {1 - a, 0}, {1, 1}, {0, 1}};
    }
    if (preset == QLatin1String("pentagon"))
        return QList<QPointF> {{0.5, 0}, {1, 0.382}, {0.809, 1}, {0.191, 1}, {0, 0.382}};
    if (preset == QLatin1String("hexagon")) {
        const double a = std::clamp(fraction("adj", 25000) * ss / w, 0.0, 0.5);
        return QList<QPointF> {{a, 0}, {1 - a, 0}, {1, 0.5}, {1 - a, 1}, {a, 1}, {0, 0.5}};
    }
    if (preset == QLatin1String("octagon")) {
        const double a = std::clamp(fraction("adj", 29289) * ss, 0.0, ss / 2);
        const double ax = a / w, ay = a / h;
        return QList<QPointF> {{ax, 0}, {1 - ax, 0}, {1, ay}, {1, 1 - ay}, {1 - ax, 1}, {ax, 1}, {0, 1 - ay}, {0, ay}};
    }
    if (preset == QLatin1String("homePlate")) {
        const double a = std::clamp(fraction("adj", 50000) * ss / w, 0.0, 1.0);
        return QList<QPointF> {{0, 0}, {1 - a, 0}, {1, 0.5}, {1 - a, 1}, {0, 1}};
    }
    if (preset == QLatin1String("chevron")) {
        const double a = std::clamp(fraction("adj", 50000) * ss / w, 0.0, 0.5);
        return QList<QPointF> {{0, 0}, {1 - a, 0}, {1, 0.5}, {1 - a, 1}, {0, 1}, {a, 0.5}};
    }
    if (preset == QLatin1String("rightArrow") || preset == QLatin1String("leftArrow") || preset == QLatin1String("upArrow")
        || preset == QLatin1String("downArrow")) {
        // (A shaft adj1 of the height, a head adj2 of the shorter side long)
        const double shaft = std::clamp(fraction("adj1", 50000), 0.0, 1.0) / 2;
        const double head = std::clamp(fraction("adj2", 50000) * ss / (preset.startsWith(u'u') || preset.startsWith(u'd') ? h : w), 0.0, 1.0);
        QList<QPointF> points {{0, 0.5 - shaft}, {1 - head, 0.5 - shaft}, {1 - head, 0}, {1, 0.5}, {1 - head, 1}, {1 - head, 0.5 + shaft}, {0, 0.5 + shaft}};
        if (preset == QLatin1String("leftArrow")) {
            for (QPointF &p : points)
                p.setX(1 - p.x());
        } else if (preset == QLatin1String("upArrow")) {
            for (QPointF &p : points)
                p = QPointF(p.y(), 1 - p.x());
        } else if (preset == QLatin1String("downArrow")) {
            for (QPointF &p : points)
                p = QPointF(p.y(), p.x());
        }
        return points;
    }
    if (preset == QLatin1String("star4") || preset == QLatin1String("star5") || preset == QLatin1String("star6")
        || preset == QLatin1String("star8")) {
        const int arms = preset.mid(4).toInt();
        const double inner = std::clamp(fraction("adj", arms == 5 ? 19098 : arms == 4 ? 12500 : 28868) * 2, 0.05, 0.95);
        QList<QPointF> points;
        for (int k = 0; k < arms * 2; ++k) {
            const double angle = -M_PI / 2 + k * M_PI / arms;
            const double r = (k % 2 == 0 ? 1 : inner) / 2;
            points.append({0.5 + r * std::cos(angle), 0.5 + r * std::sin(angle)});
        }
        return points;
    }
    return std::nullopt;
}

// A custom shape's path, as its corners (its curves taken by their ends: said in the
// notes by whoever calls this, if `curved` comes back true)
std::optional<QList<QPointF>> customOutline(const XmlNode &custGeom, double w, double h, bool *curved)
{
    *curved = false;
    const XmlNode *pathList = custGeom.child("pathLst");
    const XmlNode *path = pathList ? pathList->child("path") : nullptr;
    if (!path)
        return std::nullopt;
    const double pw = path->number("w", w) > 0 ? path->number("w", w) : std::max(1.0, w);
    const double ph = path->number("h", h) > 0 ? path->number("h", h) : std::max(1.0, h);
    QList<QPointF> points;
    const auto ptOf = [&](const XmlNode &pt) { return QPointF(pt.number("x") / pw, pt.number("y") / ph); };
    for (const XmlNode &step : path->children) {
        if (step.name == QLatin1String("moveTo") || step.name == QLatin1String("lnTo")) {
            if (const XmlNode *pt = step.child("pt"))
                points.append(ptOf(*pt));
        } else if (step.name == QLatin1String("cubicBezTo") || step.name == QLatin1String("quadBezTo")) {
            *curved = true;
            const QList<const XmlNode *> pts = step.each("pt");
            if (!pts.isEmpty())
                points.append(ptOf(*pts.last()));
        } else if (step.name == QLatin1String("arcTo")) {
            *curved = true;
        }
    }
    if (points.size() < 3)
        return std::nullopt;
    return points;
}

QList<double> dashesOf(const QString &preset)
{
    if (preset == QLatin1String("dash") || preset == QLatin1String("lgDash"))
        return {preset == QLatin1String("dash") ? 4.0 : 8.0, 3.0};
    if (preset == QLatin1String("sysDash"))
        return {3.0, 1.0};
    if (preset == QLatin1String("dot"))
        return {1.0, 3.0};
    if (preset == QLatin1String("sysDot"))
        return {1.0, 1.0};
    if (preset == QLatin1String("dashDot") || preset == QLatin1String("lgDashDot"))
        return {4.0, 3.0, 1.0, 3.0};
    return {};
}

// ---- The reader

class Reader
{
public:
    Reader(const QString &path, const std::function<bool(int, int)> &progress) : m_path(path), m_progress(progress) {}

    ReadResult run()
    {
        ReadResult result;
        result.scratch = std::make_shared<QTemporaryDir>();
        m_scratch = result.scratch;
        QString error;
        if (!m_package.open(m_path, &error)) {
            result.error = notAFile(error);
            return result;
        }
        const Part *presentation = presentationPart(&error);
        if (!presentation) {
            result.error = error;
            return result;
        }
        m_deck.title = QFileInfo(m_path).completeBaseName();
        readSize(presentation->xml);
        m_defaultTextStyle = presentation->xml.child("defaultTextStyle");

        // The slides, in the order the presentation lists them
        QList<QPair<QString, QString>> slides; // the slide's id in the list, and its part
        if (const XmlNode *list = presentation->xml.child("sldIdLst")) {
            for (const XmlNode *entry : list->each("sldId")) {
                const Relationship rel = presentation->rels.value(entry->rel("id"));
                if (!rel.target.isEmpty())
                    slides.append({entry->attr("id"), rel.target});
            }
        }
        if (slides.isEmpty()) {
            result.error = QStringLiteral("%1 has no slides in it").arg(QFileInfo(m_path).fileName());
            return result;
        }
        readSections(presentation->xml, slides);
        for (int k = 0; k < slides.size(); ++k) {
            Slide slide;
            readSlide(slides.at(k).second, k + 1, &slide, &error);
            if (!error.isEmpty()) {
                result.error = error;
                return result;
            }
            m_deck.slides.append(slide);
            if (m_progress && !m_progress(k + 1, int(slides.size()))) {
                result.error = QStringLiteral("The import was stopped");
                return result;
            }
        }
        result.deck = m_deck;
        result.notes = m_notes.all();
        return result;
    }

    Glance glance()
    {
        Glance glance;
        QString error;
        if (!m_package.open(m_path, &error)) {
            glance.error = notAFile(error);
            return glance;
        }
        const Part *presentation = presentationPart(&error);
        if (!presentation) {
            glance.error = error;
            return glance;
        }
        glance.name = QFileInfo(m_path).completeBaseName();
        readSize(presentation->xml);
        glance.size = m_deck.size;
        if (const XmlNode *list = presentation->xml.child("sldIdLst"))
            glance.slides = int(list->each("sldId").size());
        if (glance.slides == 0)
            glance.error = QStringLiteral("%1 has no slides in it").arg(QFileInfo(m_path).fileName());
        return glance;
    }

private:
    // (The zip reader's own words are of playlist archives, which is what else it reads)
    QString notAFile(const QString &why) const
    {
        if (why.contains(QLatin1String("not a playlist archive")) || why.contains(QLatin1String("not a zip")))
            return QStringLiteral("%1 is not a PowerPoint file").arg(QFileInfo(m_path).fileName());
        return QStringLiteral("%1 could not be opened: %2").arg(QFileInfo(m_path).fileName(), why);
    }

    const Part *presentationPart(QString *error)
    {
        // The root relationships name the main part; it is ppt/presentation.xml in every
        // file met, but is found rather than assumed
        QString name = QStringLiteral("ppt/presentation.xml");
        if (m_package.has(QStringLiteral("_rels/.rels"))) {
            QString why;
            if (const Part *root = m_package.part(QStringLiteral("_rels/.rels"), &why)) {
                for (const XmlNode *rel : root->xml.each("Relationship")) {
                    if (rel->attr("Type").endsWith(QLatin1String("/officeDocument"))) {
                        name = resolvePartName(QString(), rel->attr("Target"));
                        break;
                    }
                }
            }
        }
        if (!m_package.has(name)) {
            *error = QStringLiteral("%1 is not a PowerPoint file: it has no presentation in it").arg(QFileInfo(m_path).fileName());
            return nullptr;
        }
        const Part *part = m_package.part(name, error);
        if (part && part->xml.name != QLatin1String("presentation")) {
            *error = QStringLiteral("%1 is not a PowerPoint file (its main part is a %2)").arg(QFileInfo(m_path).fileName(), part->xml.name);
            return nullptr;
        }
        return part;
    }

    void readSize(const XmlNode &presentation)
    {
        // (KNOWN: PowerPoint's slides are 6858000 EMUs high, 7.5 inches, whatever their
        // shape; a file without the size is taken to be the usual 16:9)
        double cx = 12192000, cy = 6858000;
        if (const XmlNode *size = presentation.child("sldSz")) {
            cx = size->number("cx", cx);
            cy = size->number("cy", cy);
        }
        if (cx <= 0 || cy <= 0) {
            cx = 12192000;
            cy = 6858000;
        }
        m_measure.unitsPerEmu = 1080.0 / cy;
        m_deck.size = QSizeF(std::round(cx * m_measure.unitsPerEmu), 1080);
    }

    void readSections(const XmlNode &presentation, const QList<QPair<QString, QString>> &slides)
    {
        // PowerPoint's sections are an extension (p14), in the presentation's extLst:
        // each names the slides it holds by their ids in the list
        const XmlNode *extList = presentation.child("extLst");
        if (!extList)
            return;
        for (const XmlNode *ext : extList->each("ext")) {
            const XmlNode *sectionList = ext->child("sectionLst");
            if (!sectionList)
                continue;
            for (const XmlNode *section : sectionList->each("section")) {
                const XmlNode *ids = section->child("sldIdLst");
                if (!ids)
                    continue;
                int first = -1;
                for (const XmlNode *id : ids->each("sldId")) {
                    for (int k = 0; k < slides.size(); ++k) {
                        if (slides.at(k).first == id->attr("id") && (first < 0 || k < first))
                            first = k;
                    }
                }
                if (first >= 0)
                    m_deck.sections.append({section->attr("name"), first});
            }
            std::sort(m_deck.sections.begin(), m_deck.sections.end(), [](const Section &a, const Section &b) { return a.firstSlide < b.firstSlide; });
            return;
        }
    }

    // ---- Sheets: masters and layouts, read once each

    const Theme *theme(const QString &partName)
    {
        if (m_themes.contains(partName))
            return m_themes.value(partName).get();
        auto made = std::make_shared<Theme>();
        QString error;
        made->part = m_package.part(partName, &error);
        if (made->part) {
            if (const XmlNode *elements = made->part->xml.child("themeElements")) {
                if (const XmlNode *scheme = elements->child("clrScheme")) {
                    for (const XmlNode &entry : scheme->children) {
                        ColorContext plain;
                        if (const std::optional<QColor> color = innerColor(&entry, plain))
                            made->colors.insert(entry.name, *color);
                    }
                }
                if (const XmlNode *fonts = elements->child("fontScheme")) {
                    if (const XmlNode *latin = fonts->find({"majorFont", "latin"}))
                        made->majorFont = latin->attr("typeface");
                    if (const XmlNode *latin = fonts->find({"minorFont", "latin"}))
                        made->minorFont = latin->attr("typeface");
                }
                if (const XmlNode *formats = elements->child("fmtScheme")) {
                    if (const XmlNode *fills = formats->child("fillStyleLst")) {
                        for (const XmlNode &fill : fills->children)
                            made->fillStyles.append(&fill);
                    }
                    if (const XmlNode *lines = formats->child("lnStyleLst")) {
                        for (const XmlNode &line : lines->children)
                            made->lineStyles.append(&line);
                    }
                    if (const XmlNode *fills = formats->child("bgFillStyleLst")) {
                        for (const XmlNode &fill : fills->children)
                            made->backgroundFillStyles.append(&fill);
                    }
                }
            }
        }
        if (made->majorFont.isEmpty())
            made->majorFont = QStringLiteral("Calibri Light");
        if (made->minorFont.isEmpty())
            made->minorFont = QStringLiteral("Calibri");
        m_themes.insert(partName, made);
        return made.get();
    }

    const Sheet *sheet(const QString &partName, bool master)
    {
        if (m_sheets.contains(partName))
            return m_sheets.value(partName).get();
        auto made = std::make_shared<Sheet>();
        QString error;
        made->part = m_package.part(partName, &error);
        if (made->part) {
            const XmlNode &root = made->part->xml;
            if (const XmlNode *cSld = root.child("cSld")) {
                made->background = cSld->child("bg");
                if (const XmlNode *tree = cSld->child("spTree"))
                    collectPlaceholders(*tree, &made->placeholders);
            }
            if (master) {
                made->colorMap = colorMapOf(root.child("clrMap"));
                made->hasColorMap = root.child("clrMap") != nullptr;
                if (const XmlNode *styles = root.child("txStyles")) {
                    made->titleStyle = styles->child("titleStyle");
                    made->bodyStyle = styles->child("bodyStyle");
                    made->otherStyle = styles->child("otherStyle");
                }
                if (const Relationship *rel = Package::relOfType(*made->part, "theme"))
                    made->theme = theme(rel->target);
            } else {
                if (const XmlNode *override = root.find({"clrMapOvr", "overrideClrMapping"})) {
                    made->colorMap = colorMapOf(override);
                    made->hasColorMap = true;
                }
                if (const Relationship *rel = Package::relOfType(*made->part, "slideMaster"))
                    made->parent = sheet(rel->target, true);
                if (made->parent)
                    made->theme = made->parent->theme;
            }
        }
        m_sheets.insert(partName, made);
        return made.get();
    }

    // ---- A slide

    struct SlideContext
    {
        const Part *part = nullptr;
        const Sheet *layout = nullptr;
        const Sheet *master = nullptr;
        ColorContext colors;
        int number = 0;
    };

    void readSlide(const QString &partName, int number, Slide *slide, QString *error)
    {
        SlideContext context;
        context.number = number;
        context.part = m_package.part(partName, error);
        if (!context.part)
            return;
        const XmlNode &root = context.part->xml;
        if (root.attr("show") == QLatin1String("0"))
            slide->hidden = true;
        if (const Relationship *rel = Package::relOfType(*context.part, "slideLayout"))
            context.layout = sheet(rel->target, false);
        context.master = context.layout ? context.layout->parent : nullptr;
        // The colour map: the slide's override, else the layout's, else the master's
        context.colors.theme = context.master ? context.master->theme : (context.layout ? context.layout->theme : nullptr);
        if (const XmlNode *override = root.find({"clrMapOvr", "overrideClrMapping"}))
            context.colors.map = colorMapOf(override);
        else if (context.layout && context.layout->hasColorMap)
            context.colors.map = context.layout->colorMap;
        else if (context.master)
            context.colors.map = context.master->colorMap;

        const XmlNode *cSld = root.child("cSld");
        if (!cSld)
            return;
        readBackground(cSld, context, slide);
        if (const XmlNode *tree = cSld->child("spTree"))
            readTree(*tree, context, Transform(), slide);
        readNotes(context, slide);

        if (root.child("transition"))
            m_notes.add(Note::Simplified, QStringLiteral("Slide transitions are not carried over"), number);
        if (root.child("timing"))
            m_notes.add(Note::Simplified, QStringLiteral("Animations are not carried over: everything on the slide is shown as it is when they are over"), number);
    }

    // The slide's background: its own, or its layout's, or its master's
    void readBackground(const XmlNode *cSld, const SlideContext &context, Slide *slide)
    {
        const XmlNode *bg = cSld->child("bg");
        if (!bg && context.layout)
            bg = context.layout->background;
        if (!bg && context.master)
            bg = context.master->background;
        if (!bg)
            return;
        if (const XmlNode *props = bg->child("bgPr")) {
            applyBackgroundFill(*props, context.colors, context, slide);
        } else if (const XmlNode *ref = bg->child("bgRef")) {
            // A style of the theme's, with the colour the reference gives
            ColorContext colors = context.colors;
            colors.placeholderColor = innerColor(ref, context.colors);
            const int idx = int(ref->integer("idx"));
            const XmlNode *style = themeStyle(colors.theme, idx);
            if (style) {
                XmlNode holder;
                holder.children.push_back(*style);
                applyBackgroundFill(holder, colors, context, slide);
            } else if (colors.placeholderColor) {
                slide->backgroundColor = colors.placeholderColor;
            }
        }
    }

    static const XmlNode *themeStyle(const Theme *theme, int idx)
    {
        if (!theme)
            return nullptr;
        if (idx >= 1001)
            return theme->backgroundFillStyles.value(idx - 1001, nullptr);
        if (idx >= 1)
            return theme->fillStyles.value(idx - 1, nullptr);
        return nullptr;
    }

    void applyBackgroundFill(const XmlNode &props, const ColorContext &colors, const SlideContext &context, Slide *slide)
    {
        for (const XmlNode &child : props.children) {
            if (child.name == QLatin1String("solidFill")) {
                slide->backgroundColor = innerColor(&child, colors);
                return;
            }
            if (child.name == QLatin1String("gradFill")) {
                slide->backgroundGradient = gradientOf(child, colors);
                return;
            }
            if (child.name == QLatin1String("blipFill")) {
                if (const std::optional<Media> media = mediaOf(child, context)) {
                    BackgroundMedia background;
                    background.media = *media;
                    background.scale = ScaleMode::Stretch;
                    if (child.child("tile"))
                        m_notes.add(Note::Simplified, QStringLiteral("A background picture set to tile is stretched over the slide instead"), context.number);
                    slide->background = background;
                } else if (colors.placeholderColor) {
                    // (A theme's textured background: its colour stands in)
                    slide->backgroundColor = colors.placeholderColor;
                }
                return;
            }
            if (child.name == QLatin1String("pattFill")) {
                slide->backgroundColor = innerColor(child.child("fgClr"), colors);
                m_notes.add(Note::Simplified, QStringLiteral("A patterned background is shown as its colour"), context.number);
                return;
            }
        }
    }

    // ---- Shapes

    // Where a group puts its children: the group's frame against the children's own
    // coordinates
    struct Transform
    {
        double scaleX = 1, scaleY = 1;
        double offsetX = 0, offsetY = 0; // added after scaling
        double rotation = 0;
        QPointF centre; // of the group, in the parent's coordinates, for rotating about

        QRectF apply(const QRectF &rect) const
        {
            QRectF mapped(offsetX + rect.x() * scaleX, offsetY + rect.y() * scaleY, rect.width() * scaleX, rect.height() * scaleY);
            if (rotation != 0) {
                // The child's centre swung round the group's
                const QPointF c = mapped.center() - centre;
                const double a = qDegreesToRadians(rotation);
                const QPointF turned(c.x() * std::cos(a) - c.y() * std::sin(a), c.x() * std::sin(a) + c.y() * std::cos(a));
                mapped.moveCenter(centre + turned);
            }
            return mapped;
        }
        Transform then(const Transform &inner) const
        {
            // `inner` maps a child into this group's child coordinates; this maps those
            // into the parent's
            Transform combined;
            combined.scaleX = scaleX * inner.scaleX;
            combined.scaleY = scaleY * inner.scaleY;
            combined.offsetX = offsetX + inner.offsetX * scaleX;
            combined.offsetY = offsetY + inner.offsetY * scaleY;
            combined.rotation = rotation + inner.rotation;
            combined.centre = inner.rotation != 0 ? apply(QRectF(inner.centre, QSizeF(0, 0))).topLeft() : centre;
            return combined;
        }
    };

    void readTree(const XmlNode &tree, const SlideContext &context, const Transform &transform, Slide *slide)
    {
        for (const XmlNode &node : tree.children) {
            if (node.name == QLatin1String("sp")) {
                readShape(node, context, transform, slide);
            } else if (node.name == QLatin1String("pic")) {
                readPicture(node, context, transform, slide);
            } else if (node.name == QLatin1String("grpSp")) {
                readGroup(node, context, transform, slide);
            } else if (node.name == QLatin1String("graphicFrame")) {
                readGraphicFrame(node, context);
            } else if (node.name == QLatin1String("cxnSp")) {
                if (!isHidden(node, "nvCxnSpPr"))
                    m_notes.add(Note::Simplified, QStringLiteral("Lines and connectors are left out"), context.number);
            }
        }
    }

    static bool isHidden(const XmlNode &node, const char *nv)
    {
        const XmlNode *props = node.find({nv, "cNvPr"});
        return props && props->attr("hidden") == QLatin1String("1");
    }

    static QString nameOf(const XmlNode &node, const char *nv)
    {
        const XmlNode *props = node.find({nv, "cNvPr"});
        return props ? props->attr("name") : QString();
    }

    void readGroup(const XmlNode &group, const SlideContext &context, const Transform &transform, Slide *slide)
    {
        if (isHidden(group, "nvGrpSpPr"))
            return;
        Transform own;
        if (const XmlNode *xfrm = group.find({"grpSpPr", "xfrm"})) {
            const QRectF frame = m_measure.rect(xfrm);
            const XmlNode *chOff = xfrm->child("chOff");
            const XmlNode *chExt = xfrm->child("chExt");
            const double childX = chOff ? m_measure.length(*chOff, "x") : frame.x();
            const double childY = chOff ? m_measure.length(*chOff, "y") : frame.y();
            const double childW = chExt ? m_measure.length(*chExt, "cx") : frame.width();
            const double childH = chExt ? m_measure.length(*chExt, "cy") : frame.height();
            own.scaleX = childW > 0 ? frame.width() / childW : 1;
            own.scaleY = childH > 0 ? frame.height() / childH : 1;
            own.offsetX = frame.x() - childX * own.scaleX;
            own.offsetY = frame.y() - childY * own.scaleY;
            own.rotation = xfrm->number("rot") / 60000.0;
            own.centre = frame.center();
        }
        readTree(group, context, transform.then(own), slide);
    }

    void readGraphicFrame(const XmlNode &frame, const SlideContext &context)
    {
        if (isHidden(frame, "nvGraphicFramePr"))
            return;
        const XmlNode *data = frame.find({"graphic", "graphicData"});
        const QString uri = data ? data->attr("uri") : QString();
        if (uri.endsWith(QLatin1String("/table")))
            m_notes.add(Note::Simplified, QStringLiteral("Tables are left out"), context.number);
        else if (uri.endsWith(QLatin1String("/chart")))
            m_notes.add(Note::Simplified, QStringLiteral("Charts are left out"), context.number);
        else if (uri.endsWith(QLatin1String("/diagram")))
            m_notes.add(Note::Simplified, QStringLiteral("SmartArt is left out"), context.number);
        else
            m_notes.add(Note::Simplified, QStringLiteral("Embedded objects are left out"), context.number);
    }

    // The frame of a shape or picture: its own, or its placeholder's up the sheets
    QRectF frameOf(const XmlNode &node, const char *spPr, const Placeholder *inLayout, const Placeholder *inMaster) const
    {
        if (const XmlNode *xfrm = node.find({spPr, "xfrm"}))
            return m_measure.rect(xfrm);
        for (const Placeholder *p : {inLayout, inMaster}) {
            if (p && p->shape) {
                if (const XmlNode *xfrm = p->shape->find({"spPr", "xfrm"}))
                    return m_measure.rect(xfrm);
            }
        }
        return {};
    }

    // The placeholders a shape stands in, in the layout and in the master
    void placeholdersOf(const XmlNode &node, const char *nv, const SlideContext &context, const Placeholder **inLayout, const Placeholder **inMaster) const
    {
        *inLayout = nullptr;
        *inMaster = nullptr;
        const XmlNode *ph = node.find({nv, "nvPr", "ph"});
        if (!ph)
            return;
        const QString type = placeholderType(*ph);
        const QString idx = ph->attr("idx");
        if (context.layout)
            *inLayout = context.layout->placeholder(type, idx);
        if (context.master) {
            // (The master's placeholder is the layout's parent, by the layout
            // placeholder's type; or the slide's own, where the layout has none)
            const QString masterType = *inLayout ? (*inLayout)->type : type;
            const QString masterIdx = *inLayout ? (*inLayout)->idx : idx;
            *inMaster = context.master->placeholder(masterType, masterIdx);
        }
    }

    // The text style chain of a shape
    StyleChain chainFor(const XmlNode &shape, const SlideContext &context, const Placeholder *inLayout, const Placeholder *inMaster) const
    {
        StyleChain chain;
        if (const XmlNode *list = shape.find({"txBody", "lstStyle"}))
            chain.lists.append(list);
        for (const Placeholder *p : {inLayout, inMaster}) {
            if (p && p->shape) {
                if (const XmlNode *list = p->shape->find({"txBody", "lstStyle"}))
                    chain.lists.append(list);
            }
        }
        chain.fontReferenceAfter = int(chain.lists.size());
        chain.fontReference = shape.find({"style", "fontRef"});
        if (context.master) {
            const XmlNode *ph = shape.find({"nvSpPr", "nvPr", "ph"});
            const QString type = ph ? placeholderType(*ph) : QString();
            const XmlNode *style = !ph ? context.master->otherStyle
                                   : typesMatch(type, QStringLiteral("title")) ? context.master->titleStyle
                                   : (type == QLatin1String("dt") || type == QLatin1String("ftr") || type == QLatin1String("sldNum"))
                                       ? context.master->otherStyle
                                       : context.master->bodyStyle;
            if (style)
                chain.lists.append(style);
        }
        if (m_defaultTextStyle)
            chain.lists.append(m_defaultTextStyle);
        return chain;
    }

    void readShape(const XmlNode &shape, const SlideContext &context, const Transform &transform, Slide *slide)
    {
        if (isHidden(shape, "nvSpPr"))
            return;
        const Placeholder *inLayout = nullptr;
        const Placeholder *inMaster = nullptr;
        placeholdersOf(shape, "nvSpPr", context, &inLayout, &inMaster);
        const bool isPlaceholder = shape.find({"nvSpPr", "nvPr", "ph"}) != nullptr;
        const XmlNode *txBody = shape.child("txBody");
        const bool hasWords = txBody && hasText(*txBody);
        // (KNOWN: a placeholder with nothing typed into it shows nothing in the slide
        // show, not even its prompt; and one with no picture and no words is nothing)
        if (isPlaceholder && !hasWords)
            return;

        Shape made;
        made.name = nameOf(shape, "nvSpPr");
        const QRectF own = frameOf(shape, "spPr", inLayout, inMaster);
        made.frame = transform.apply(own);
        if (const XmlNode *xfrm = shape.find({"spPr", "xfrm"})) {
            made.rotation = xfrm->number("rot") / 60000.0 + transform.rotation;
            made.flipHorizontal = xfrm->attr("flipH") == QLatin1String("1");
            made.flipVertical = xfrm->attr("flipV") == QLatin1String("1");
        } else {
            made.rotation = transform.rotation;
        }

        // The outline
        const XmlNode *spPr = shape.child("spPr");
        const XmlNode *geometry = spPr ? spPr->child("prstGeom") : nullptr;
        if (!geometry) {
            for (const Placeholder *p : {inLayout, inMaster}) {
                if (p && p->shape && p->shape->find({"spPr", "prstGeom"})) {
                    geometry = p->shape->find({"spPr", "prstGeom"});
                    break;
                }
            }
        }
        const QString preset = geometry ? geometry->attr("prst") : QString();
        if (spPr && spPr->child("custGeom")) {
            bool curved = false;
            if (const std::optional<QList<QPointF>> points = customOutline(*spPr->child("custGeom"), own.width(), own.height(), &curved)) {
                made.geometry = Shape::Polygon;
                made.points = *points;
                if (curved)
                    m_notes.add(Note::Simplified, QStringLiteral("A drawn shape's curves are straightened"), context.number);
            } else {
                m_notes.add(Note::Simplified, QStringLiteral("A drawn shape this cannot follow is shown as a rectangle"), context.number);
            }
        } else if (preset == QLatin1String("roundRect")) {
            made.geometry = Shape::RoundedRectangle;
            // (KNOWN: the corner's radius is adj of the shorter side, 16667 unless set)
            made.cornerFraction = std::clamp(adjustment(geometry, "adj", 16667) / 100000.0, 0.0, 0.5);
        } else if (preset == QLatin1String("ellipse")) {
            made.geometry = Shape::Ellipse;
        } else if (preset == QLatin1String("line") || preset == QLatin1String("straightConnector1")) {
            m_notes.add(Note::Simplified, QStringLiteral("Lines and connectors are left out"), context.number);
            return;
        } else if (!preset.isEmpty() && preset != QLatin1String("rect")) {
            if (const std::optional<QList<QPointF>> points = presetOutline(preset, geometry, own.width(), own.height())) {
                made.geometry = Shape::Polygon;
                made.points = *points;
            } else {
                m_notes.add(Note::Simplified, QStringLiteral("Shapes of kinds this app has no outline for (%1) are shown as rectangles").arg(preset), context.number);
            }
        }

        // The fill, the line and the shadow: the shape's own, else its placeholder's,
        // else what the shape's style gives it from the theme
        const XmlNode *style = shape.child("style");
        made.fill = fillOf(spPr, inLayout, inMaster, style, context);
        made.line = lineOf(spPr, inLayout, inMaster, style, context);
        const XmlNode *effects = spPr ? spPr->child("effectLst") : nullptr;
        for (const Placeholder *p : {inLayout, inMaster}) {
            if (!effects && p && p->shape)
                effects = p->shape->find({"spPr", "effectLst"});
        }
        if (effects) {
            if (const XmlNode *shadow = effects->child("outerShdw"))
                made.shadow = shadowOf(*shadow, context.colors);
            if (const XmlNode *soft = effects->child("softEdge"))
                made.softEdge = m_measure.length(*soft, "rad");
        }

        if (hasWords) {
            const StyleChain chain = chainFor(shape, context, inLayout, inMaster);
            made.text = textBodyOf(*txBody, chain, inLayout, inMaster, context);
        }
        slide->items.append(made);
    }

    static bool hasText(const XmlNode &txBody)
    {
        for (const XmlNode *p : txBody.each("p")) {
            for (const XmlNode &child : p->children) {
                if ((child.name == QLatin1String("r") || child.name == QLatin1String("fld")) && child.child("t")
                    && !child.child("t")->text.trimmed().isEmpty())
                    return true;
            }
        }
        return false;
    }

    // The fill a shape's properties give, if they say: a fill, a "no fill", or nothing
    // said (std::nullopt with *said false)
    std::optional<Fill> fillIn(const XmlNode *props, const ColorContext &colors, const SlideContext &context, bool *said)
    {
        *said = false;
        if (!props)
            return std::nullopt;
        for (const XmlNode &child : props->children) {
            if (child.name == QLatin1String("noFill")) {
                *said = true;
                return std::nullopt;
            }
            if (child.name == QLatin1String("solidFill")) {
                *said = true;
                return Fill(innerColor(&child, colors).value_or(QColor(Qt::black)));
            }
            if (child.name == QLatin1String("gradFill")) {
                *said = true;
                return Fill(gradientOf(child, colors));
            }
            if (child.name == QLatin1String("blipFill")) {
                *said = true;
                if (const std::optional<Media> media = mediaOf(child, context)) {
                    PictureFill picture;
                    picture.media = *media;
                    picture.scale = ScaleMode::Stretch;
                    return Fill(picture);
                }
                return std::nullopt;
            }
            if (child.name == QLatin1String("pattFill")) {
                *said = true;
                m_notes.add(Note::Simplified, QStringLiteral("A patterned fill is shown as its colour"), context.number);
                return Fill(innerColor(child.child("fgClr"), colors).value_or(QColor(Qt::black)));
            }
            if (child.name == QLatin1String("grpFill")) {
                *said = true;
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<Fill> fillOf(const XmlNode *spPr, const Placeholder *inLayout, const Placeholder *inMaster, const XmlNode *style,
                               const SlideContext &context)
    {
        bool said = false;
        std::optional<Fill> fill = fillIn(spPr, context.colors, context, &said);
        if (said)
            return fill;
        for (const Placeholder *p : {inLayout, inMaster}) {
            if (p && p->shape) {
                fill = fillIn(p->shape->child("spPr"), context.colors, context, &said);
                if (said)
                    return fill;
            }
        }
        if (style) {
            if (const XmlNode *ref = style->child("fillRef")) {
                const int idx = int(ref->integer("idx"));
                if (const XmlNode *themed = themeStyle(context.colors.theme, idx)) {
                    ColorContext colors = context.colors;
                    colors.placeholderColor = innerColor(ref, context.colors);
                    XmlNode holder;
                    holder.children.push_back(*themed);
                    fill = fillIn(&holder, colors, context, &said);
                    if (said)
                        return fill;
                }
            }
        }
        return std::nullopt;
    }

    std::optional<Line> lineIn(const XmlNode *ln, const ColorContext &colors, bool *said)
    {
        *said = false;
        if (!ln)
            return std::nullopt;
        if (ln->child("noFill")) {
            *said = true;
            return std::nullopt;
        }
        std::optional<QColor> color;
        if (const XmlNode *solid = ln->child("solidFill"))
            color = innerColor(solid, colors);
        else if (const XmlNode *gradient = ln->child("gradFill"))
            color = gradientOf(*gradient, colors).stops.first().color;
        if (!color)
            return std::nullopt;
        *said = true;
        Line line;
        // (KNOWN: a line with no width is 0.75 of a point, 9525 EMUs)
        line.width = m_measure.length(*ln, "w", 9525);
        line.color = *color;
        if (const XmlNode *dash = ln->child("prstDash"))
            line.dashes = dashesOf(dash->attr("val"));
        return line;
    }

    std::optional<Line> lineOf(const XmlNode *spPr, const Placeholder *inLayout, const Placeholder *inMaster, const XmlNode *style,
                               const SlideContext &context)
    {
        bool said = false;
        std::optional<Line> line = lineIn(spPr ? spPr->child("ln") : nullptr, context.colors, &said);
        if (said)
            return line;
        for (const Placeholder *p : {inLayout, inMaster}) {
            if (p && p->shape) {
                line = lineIn(p->shape->find({"spPr", "ln"}), context.colors, &said);
                if (said)
                    return line;
            }
        }
        if (style) {
            if (const XmlNode *ref = style->child("lnRef")) {
                const int idx = int(ref->integer("idx"));
                const XmlNode *themed = context.colors.theme ? context.colors.theme->lineStyles.value(idx - 1, nullptr) : nullptr;
                if (idx >= 1 && themed) {
                    ColorContext colors = context.colors;
                    colors.placeholderColor = innerColor(ref, context.colors);
                    line = lineIn(themed, colors, &said);
                    if (said) {
                        // (The width the shape's own line gives, with no colour, wins over
                        // the theme's: a shape can thicken its line and keep the colour)
                        if (spPr && spPr->child("ln") && spPr->child("ln")->has("w") && line)
                            line->width = m_measure.length(*spPr->child("ln"), "w");
                        return line;
                    }
                }
            }
        }
        return std::nullopt;
    }

    Shadow shadowOf(const XmlNode &outerShdw, const ColorContext &colors) const
    {
        Shadow shadow;
        shadow.color = innerColor(&outerShdw, colors).value_or(QColor(0, 0, 0, 160));
        shadow.direction = outerShdw.number("dir") / 60000.0;
        shadow.distance = m_measure.length(outerShdw, "dist");
        shadow.blurRadius = m_measure.length(outerShdw, "blurRad");
        return shadow;
    }

    // ---- Text

    TextBody textBodyOf(const XmlNode &txBody, const StyleChain &chain, const Placeholder *inLayout, const Placeholder *inMaster,
                        const SlideContext &context)
    {
        TextBody body;
        // The body's properties: the shape's own, else its placeholders' (KNOWN: insets
        // of 0.1 inch each side and 0.05 above and below unless said)
        const XmlNode *bodyPr = txBody.child("bodyPr");
        const auto inherited = [&](const char *attr) -> QString {
            if (bodyPr && bodyPr->has(attr))
                return bodyPr->attr(attr);
            for (const Placeholder *p : {inLayout, inMaster}) {
                if (p && p->shape) {
                    if (const XmlNode *theirs = p->shape->find({"txBody", "bodyPr"})) {
                        if (theirs->has(attr))
                            return theirs->attr(attr);
                    }
                }
            }
            return {};
        };
        const auto inset = [&](const char *attr, double fallback) {
            const QString value = inherited(attr);
            bool ok = false;
            const double emu = value.toDouble(&ok);
            return m_measure.length(ok ? emu : fallback);
        };
        body.insets = QMarginsF(inset("lIns", 91440), inset("tIns", 45720), inset("rIns", 91440), inset("bIns", 45720));
        const QString anchor = inherited("anchor");
        body.verticalAlignment = anchor == QLatin1String("ctr") ? TextBody::Middle : anchor == QLatin1String("b") ? TextBody::Bottom : TextBody::Top;
        body.wraps = inherited("wrap") != QLatin1String("none");
        const QString vertical = inherited("vert");
        if (!vertical.isEmpty() && vertical != QLatin1String("horz"))
            m_notes.add(Note::Simplified, QStringLiteral("Text set to run vertically is shown horizontally"), context.number);

        // How PowerPoint fitted the words: shrunk by a factor it wrote, with the lines
        // closer by another (KNOWN), or the box grown to them (which its size shows
        // already), or neither
        double fontScale = 1;
        double spacingReduction = 0;
        const XmlNode *autofit = nullptr;
        for (const XmlNode *candidate : {bodyPr, inLayout && inLayout->shape ? inLayout->shape->find({"txBody", "bodyPr"}) : nullptr,
                                         inMaster && inMaster->shape ? inMaster->shape->find({"txBody", "bodyPr"}) : nullptr}) {
            if (candidate && (candidate->child("normAutofit") || candidate->child("spAutoFit") || candidate->child("noAutofit"))) {
                autofit = candidate;
                break;
            }
        }
        if (autofit && autofit->child("normAutofit")) {
            body.autofit = TextBody::Shrink;
            fontScale = autofit->child("normAutofit")->number("fontScale", 100000) / 100000.0;
            spacingReduction = autofit->child("normAutofit")->number("lnSpcReduction", 0) / 100000.0;
        } else if (autofit && autofit->child("spAutoFit")) {
            body.autofit = TextBody::ResizeShape;
        }
        body.fontScale = 1;

        for (const XmlNode *p : txBody.each("p")) {
            StyleChain here = chain;
            here.paragraph = p->child("pPr");
            here.level = std::clamp(int(here.paragraph ? here.paragraph->integer("lvl") : 0), 0, 8);
            body.paragraphs.append(paragraphOf(*p, here, fontScale, spacingReduction, context));
        }
        return body;
    }

    Paragraph paragraphOf(const XmlNode &p, const StyleChain &chain, double fontScale, double spacingReduction, const SlideContext &context)
    {
        Paragraph paragraph;
        const QString align = chain.paragraphAttr("algn");
        paragraph.alignment = align == QLatin1String("ctr") ? Paragraph::Center
                            : align == QLatin1String("r") ? Paragraph::Right
                            : (align == QLatin1String("just") || align == QLatin1String("dist")) ? Paragraph::Justified
                                                                                                 : Paragraph::Left;
        paragraph.leftIndent = m_measure.length(chain.paragraphAttr("marL").toDouble());
        paragraph.firstLineIndent = m_measure.length(chain.paragraphAttr("indent").toDouble());
        paragraph.rightIndent = m_measure.length(chain.paragraphAttr("marR").toDouble());

        // The runs
        for (const XmlNode &child : p.children) {
            if (child.name == QLatin1String("r") || child.name == QLatin1String("fld")) {
                Run run;
                run.text = child.child("t") ? child.child("t")->text : QString();
                run.style = runStyleOf(child.child("rPr"), chain, fontScale, context);
                if (!run.text.isEmpty())
                    paragraph.runs.append(run);
            } else if (child.name == QLatin1String("br")) {
                // (A line break inside the paragraph: U+2028 in the words, as the deck has it)
                if (!paragraph.runs.isEmpty()) {
                    paragraph.runs.last().text += QChar(0x2028);
                } else {
                    Run run;
                    run.text = QString(QChar(0x2028));
                    run.style = runStyleOf(child.child("rPr"), chain, fontScale, context);
                    paragraph.runs.append(run);
                }
            }
        }
        paragraph.endStyle = runStyleOf(p.child("endParaRPr"), chain, fontScale, context);

        // The largest size on the line, for the spacing that goes by it
        double largest = paragraph.endStyle.size;
        for (const Run &run : paragraph.runs)
            largest = std::max(largest, run.style.size);
        const RunStyle *largestStyle = &paragraph.endStyle;
        for (const Run &run : paragraph.runs) {
            if (run.style.size >= largest)
                largestStyle = &run.style;
        }

        // Line spacing: PowerPoint's percentage is of 1.2 x the size for any font (KNOWN,
        // ProToPPTX's measurements); ProPresenter's multiple is of the font's own line
        // height. A reduction from shrinking to fit comes off the percentage.
        const XmlNode *spacing = chain.paragraphChild("lnSpc");
        std::optional<double> percent;
        if (spacing && spacing->child("spcPct"))
            percent = spacing->child("spcPct")->number("val", 100000) / 100000.0;
        else if (spacing && spacing->child("spcPts"))
            paragraph.lineSpacing = Paragraph::LineSpacing {Paragraph::LineSpacing::Exactly, m_measure.centipoints(spacing->child("spcPts")->number("val"))};
        else if (spacingReduction > 0)
            percent = 1;
        if (percent) {
            const double effective = std::max(0.1, *percent - spacingReduction);
            const double ratio = std::max(0.5, lineHeightRatio(*largestStyle));
            paragraph.lineSpacing = Paragraph::LineSpacing {Paragraph::LineSpacing::Multiple, 1.2 * effective / ratio};
        }
        const auto spaceOf = [&](const char *name) -> double {
            const XmlNode *node = chain.paragraphChild(name);
            if (!node)
                return 0;
            if (const XmlNode *pts = node->child("spcPts"))
                return m_measure.centipoints(pts->number("val"));
            if (const XmlNode *pct = node->child("spcPct"))
                return largest * pct->number("val") / 100000.0;
            return 0;
        };
        paragraph.spaceBefore = spaceOf("spcBef");
        paragraph.spaceAfter = spaceOf("spcAft");

        // Bullets: none, a character, or a number (KNOWN: a paragraph's choice, else its
        // level's up the chain; a body placeholder's master style gives it one)
        if (const XmlNode *choice = chain.bulletChoice()) {
            if (choice->name == QLatin1String("buChar")) {
                paragraph.bullet = Paragraph::Bullet {choice->attr("char", QStringLiteral("•")), QString(), 1};
            } else if (choice->name == QLatin1String("buAutoNum")) {
                paragraph.bullet = Paragraph::Bullet {QString(), choice->attr("type", QStringLiteral("arabicPeriod")), int(choice->integer("startAt", 1))};
            } else if (choice->name == QLatin1String("buBlip")) {
                paragraph.bullet = Paragraph::Bullet {QStringLiteral("•"), QString(), 1};
            }
        }
        // (A paragraph with no words shows no bullet)
        if (paragraph.bullet && paragraph.runs.isEmpty())
            paragraph.bullet.reset();
        return paragraph;
    }

    RunStyle runStyleOf(const XmlNode *rPr, const StyleChain &chain, double fontScale, const SlideContext &context)
    {
        RunStyle style;
        // (KNOWN: 18 points where nothing says)
        const QString sz = chain.runAttr(rPr, "sz");
        style.size = m_measure.centipoints(sz.isEmpty() ? 1800 : sz.toDouble()) * fontScale;
        style.bold = chain.runAttr(rPr, "b") == QLatin1String("1");
        style.italic = chain.runAttr(rPr, "i") == QLatin1String("1");
        const QString underline = chain.runAttr(rPr, "u");
        style.underline = !underline.isEmpty() && underline != QLatin1String("none");
        const QString strike = chain.runAttr(rPr, "strike");
        style.strikethrough = !strike.isEmpty() && strike != QLatin1String("noStrike");
        const QString caps = chain.runAttr(rPr, "cap");
        style.capitalization = caps == QLatin1String("all") ? RunStyle::AllCaps : caps == QLatin1String("small") ? RunStyle::SmallCaps : RunStyle::NoCapitalization;
        const QString spacing = chain.runAttr(rPr, "spc");
        if (!spacing.isEmpty())
            style.characterSpacing = m_measure.centipoints(spacing.toDouble()) * fontScale;
        const QString baseline = chain.runAttr(rPr, "baseline");
        if (!baseline.isEmpty()) {
            const double percent = baseline.toDouble() / 100000.0;
            style.superscript = percent > 0 ? 1 : percent < 0 ? -1 : 0;
            style.baselineOffset = style.size * percent;
        }

        // The colour: the run's fill, else up the chain, else the shape style's font
        // reference, else the theme's text colour
        bool fromReference = false;
        const XmlNode *fill = chain.runChild(rPr, "solidFill", &fromReference);
        if (!fill) {
            fromReference = false;
            fill = chain.runChild(rPr, "gradFill", &fromReference);
        }
        if (fill && fromReference) {
            style.fill = innerColor(fill, context.colors).value_or(QColor(Qt::black));
        } else if (fill && fill->name == QLatin1String("gradFill")) {
            style.fill = gradientOf(*fill, context.colors);
        } else if (fill) {
            style.fill = innerColor(fill, context.colors).value_or(QColor(Qt::black));
        } else if (rPr && rPr->child("noFill")) {
            style.fill = QColor(0, 0, 0, 0);
        } else {
            ColorContext colors = context.colors;
            XmlNode scheme;
            scheme.name = QStringLiteral("schemeClr");
            scheme.attributes.append({QStringLiteral("val"), QString(), QStringLiteral("tx1")});
            style.fill = colorOf(&scheme, colors).value_or(QColor(Qt::black));
        }
        if (const XmlNode *ln = rPr ? rPr->child("ln") : nullptr) {
            bool said = false;
            style.outline = lineIn(ln, context.colors, &said);
        }
        if (const XmlNode *effects = rPr ? rPr->child("effectLst") : nullptr) {
            if (const XmlNode *shadow = effects->child("outerShdw"))
                style.shadow = shadowOf(*shadow, context.colors);
            if (effects->child("glow") || effects->child("reflection"))
                m_notes.add(Note::Simplified, QStringLiteral("Glows and reflections on text are left out"), context.number);
        }
        if (const XmlNode *highlight = rPr ? rPr->child("highlight") : nullptr)
            style.highlight = innerColor(highlight, context.colors);

        // The font: the run's latin typeface, else up the chain (a theme's font by its
        // "+mj-lt" or "+mn-lt"), else the style's font reference, else the minor font
        QString family;
        const XmlNode *latin = chain.runChild(rPr, "latin", &fromReference);
        if (latin && fromReference) {
            const Theme *theme = context.colors.theme;
            family = latin->attr("idx") == QLatin1String("major") && theme ? theme->majorFont : (theme ? theme->minorFont : QString());
        } else if (latin) {
            family = latin->attr("typeface");
        }
        if (family.startsWith(u'+')) {
            const Theme *theme = context.colors.theme;
            family = family.startsWith(QLatin1String("+mj")) && theme ? theme->majorFont : (theme ? theme->minorFont : QString());
        }
        if (family.isEmpty())
            family = context.colors.theme ? context.colors.theme->minorFont : QStringLiteral("Calibri");
        style.fontFamily = family;
        style.fontName = postScriptNameFor(family, style.bold, style.italic);
        if (!m_fontsChecked.contains(style.fontName)) {
            m_fontsChecked.insert(style.fontName);
            if (!resolvePostScriptName(style.fontName, family).installed)
                m_missingFonts.insert(family);
        }
        if (m_missingFonts.contains(family))
            m_notes.add(Note::MissingFont, QStringLiteral("The font \"%1\" is not installed here: its words are drawn in a stand-in").arg(family), context.number);
        return style;
    }

    // ---- Pictures and videos

    // The medium a picture fill names (a:blip r:embed), taken out of the file into the
    // scratch folder; none where it is not in the file
    std::optional<Media> mediaOf(const XmlNode &blipFill, const SlideContext &context)
    {
        const XmlNode *blip = blipFill.child("blip");
        if (!blip)
            return std::nullopt;
        const QString embed = blip->rel("embed");
        const QString link = blip->rel("link");
        const Relationship rel = context.part->rels.value(embed.isEmpty() ? link : embed);
        if (rel.target.isEmpty() || rel.external) {
            m_notes.add(Note::MissingMedia, QStringLiteral("A picture that is not in the file (linked to one on another computer) is left out"), context.number);
            return std::nullopt;
        }
        std::optional<Media> media = extracted(rel.target, Media::Image, context);
        if (media) {
            if (const XmlNode *alpha = blip->child("alphaModFix"))
                m_opacities.insert(media->key(), alpha->number("amt", 100000) / 100000.0);
        }
        return media;
    }

    std::optional<Media> extracted(const QString &partName, Media::Kind kind, const SlideContext &context)
    {
        if (m_extracted.contains(partName))
            return m_extracted.value(partName);
        if (!m_scratch || !m_scratch->isValid()) {
            m_notes.add(Note::MissingMedia, QStringLiteral("The file's pictures could not be taken out: no folder could be made for them"), context.number);
            return std::nullopt;
        }
        const QString fileName = partName.mid(partName.lastIndexOf(u'/') + 1);
        const QString destination = m_scratch->filePath(fileName);
        QString error;
        if (!m_package.extract(partName, destination, &error)) {
            m_notes.add(Note::MissingMedia, QStringLiteral("A picture could not be taken out of the file (%1)").arg(error), context.number);
            return std::nullopt;
        }
        Media media;
        media.kind = kind;
        media.path = destination;
        media.fileName = fileName;
        if (kind == Media::Image) {
            const QSize size = QImageReader(destination).size();
            if (size.isValid())
                media.naturalSize = size;
        }
        m_extracted.insert(partName, media);
        return media;
    }

    void readPicture(const XmlNode &pic, const SlideContext &context, const Transform &transform, Slide *slide)
    {
        if (isHidden(pic, "nvPicPr"))
            return;
        const Placeholder *inLayout = nullptr;
        const Placeholder *inMaster = nullptr;
        placeholdersOf(pic, "nvPicPr", context, &inLayout, &inMaster);
        const QRectF own = frameOf(pic, "spPr", inLayout, inMaster);
        const QRectF frame = transform.apply(own);
        const XmlNode *xfrm = pic.find({"spPr", "xfrm"});
        const double rotation = (xfrm ? xfrm->number("rot") / 60000.0 : 0) + transform.rotation;

        // A video or a sound, by its link in the picture's properties
        if (const XmlNode *video = pic.find({"nvPicPr", "nvPr", "videoFile"})) {
            const Relationship rel = context.part->rels.value(video->rel("link"));
            if (rel.target.isEmpty() || rel.external) {
                m_notes.add(Note::MissingMedia, QStringLiteral("A video that is not in the file (linked to one on another computer) is left out"), context.number);
                return;
            }
            if (const std::optional<Media> media = extracted(rel.target, Media::Video, context)) {
                Video made;
                made.name = nameOf(pic, "nvPicPr");
                made.frame = frame;
                made.media = *media;
                made.scale = ScaleMode::Stretch;
                made.loops = false;
                made.playsAutomatically = true;
                slide->items.append(made);
            }
            return;
        }
        if (pic.find({"nvPicPr", "nvPr", "audioFile"})) {
            m_notes.add(Note::Audio, QStringLiteral("Sound is left out"), context.number);
            return;
        }

        const XmlNode *blipFill = pic.child("blipFill");
        if (!blipFill)
            return;
        const std::optional<Media> media = mediaOf(*blipFill, context);
        if (!media)
            return;
        Picture made;
        made.name = nameOf(pic, "nvPicPr");
        made.frame = frame;
        made.rotation = rotation;
        made.media = *media;
        made.scale = ScaleMode::Stretch;
        made.opacity = m_opacities.value(media->key(), 1.0);
        if (xfrm) {
            made.flipHorizontal = xfrm->attr("flipH") == QLatin1String("1");
            made.flipVertical = xfrm->attr("flipV") == QLatin1String("1");
        }
        // (KNOWN: the source rectangle's sides are thousandths of a percent cut off)
        if (const XmlNode *source = blipFill->child("srcRect")) {
            made.crop = QMarginsF(std::clamp(source->number("l") / 100000.0, 0.0, 1.0), std::clamp(source->number("t") / 100000.0, 0.0, 1.0),
                                  std::clamp(source->number("r") / 100000.0, 0.0, 1.0), std::clamp(source->number("b") / 100000.0, 0.0, 1.0));
        }
        if (blipFill->child("tile"))
            m_notes.add(Note::Simplified, QStringLiteral("A picture set to tile is stretched over its box instead"), context.number);
        const XmlNode *geometry = pic.find({"spPr", "prstGeom"});
        if (geometry && geometry->attr("prst") != QLatin1String("rect"))
            m_notes.add(Note::Simplified, QStringLiteral("A picture cut to a shape is shown whole, in its box"), context.number);
        slide->items.append(made);
    }

    // ---- Notes

    void readNotes(const SlideContext &context, Slide *slide)
    {
        const Relationship *rel = Package::relOfType(*context.part, "notesSlide");
        if (!rel)
            return;
        QString error;
        const Part *notes = m_package.part(rel->target, &error);
        if (!notes)
            return;
        const XmlNode *tree = notes->xml.find({"cSld", "spTree"});
        if (!tree)
            return;
        for (const XmlNode *sp : tree->each("sp")) {
            const XmlNode *ph = sp->find({"nvSpPr", "nvPr", "ph"});
            if (!ph || placeholderType(*ph) != QLatin1String("body"))
                continue;
            const XmlNode *txBody = sp->child("txBody");
            if (!txBody)
                continue;
            QStringList lines;
            for (const XmlNode *p : txBody->each("p")) {
                QString line;
                for (const XmlNode &child : p->children) {
                    if ((child.name == QLatin1String("r") || child.name == QLatin1String("fld")) && child.child("t"))
                        line += child.child("t")->text;
                    else if (child.name == QLatin1String("br"))
                        line += u'\n';
                }
                lines << line;
            }
            while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
                lines.removeLast();
            slide->notes = lines.join(u'\n');
            return;
        }
    }

    QString m_path;
    std::function<bool(int, int)> m_progress;
    Package m_package;
    Deck m_deck;
    Measure m_measure;
    NoteCollector m_notes;
    const XmlNode *m_defaultTextStyle = nullptr;
    QHash<QString, std::shared_ptr<Theme>> m_themes;
    QHash<QString, std::shared_ptr<Sheet>> m_sheets;
    QHash<QString, Media> m_extracted;
    QHash<QString, double> m_opacities;
    QSet<QString> m_fontsChecked;
    QSet<QString> m_missingFonts;
    std::shared_ptr<QTemporaryDir> m_scratch;
};

} // namespace

ReadResult readDeck(const QString &path, const std::function<bool(int done, int total)> &progress)
{
    Reader reader(path, progress);
    return reader.run();
}

Glance glanceAt(const QString &path)
{
    Reader reader(path, {});
    return reader.glance();
}

} // namespace pptx
