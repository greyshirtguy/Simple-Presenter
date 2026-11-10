#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <vector>

// A small tree of an XML document, for reading PowerPoint's parts (Import PowerPoint,
// pptxread.h).
//
// Why not Qt's DOM: the app links Qt's Core, Gui, Quick, Multimedia and a few more, and
// not its Xml module; a tree over QXmlStreamReader (which Core has, and which reads the
// Bibles' XML) is sixty lines and keeps the package's dependencies as they are. And a
// reader of PowerPoint's parts wants a tree, not a stream: what a shape looks like is
// found by looking up from it through its layout and its master, part by part, and
// back again.
//
// Names are kept without their prefixes ("sp" for "p:sp"): every part of a PowerPoint
// file uses the same prefixes by convention, but a prefix is the writer's choice and a
// namespace is not, so an element is found by its local name, and an attribute by its
// local name, or by its local name in a namespace where two share one ("id" and "r:id"
// on a slide's entry in the list of them).
struct XmlAttribute
{
    QString name;
    QString ns;
    QString value;
};

struct XmlNode
{
    QString name;
    QString ns;
    QList<XmlAttribute> attributes;
    std::vector<XmlNode> children;
    // The character data directly inside the element (meaningful for a leaf such as
    // <a:t>; whitespace between child elements, where a writer puts any, is ignored
    // by whoever reads a node that has children)
    QString text;

    // The first child element of this local name, or null
    const XmlNode *child(const char *name) const;
    // Every child element of this local name, in order
    QList<const XmlNode *> each(const char *name) const;
    // The child at the end of a path of local names ("spPr", "xfrm", "off"), or null
    const XmlNode *find(std::initializer_list<const char *> path) const;
    // An attribute by its local name ("" if there is none); by its local name in a
    // namespace; and one of the relationships namespace ("r:id", "r:embed")
    QString attr(const char *name) const;
    QString attr(const char *name, const QString &fallback) const;
    bool has(const char *name) const;
    QString attrIn(const char *name, const QString &ns) const;
    QString rel(const char *name) const;
    // An attribute as a number, or `fallback` where it is missing or not one
    double number(const char *name, double fallback = 0) const;
    qint64 integer(const char *name, qint64 fallback = 0) const;
};

// Parses a whole document. Returns false, with *error set, on malformed XML.
bool parseXml(const QByteArray &bytes, XmlNode *root, QString *error);
