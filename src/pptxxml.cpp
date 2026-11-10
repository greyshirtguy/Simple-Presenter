#include "pptxxml.h"

#include <QXmlStreamReader>

namespace {

const QString relationshipsNs = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");

} // namespace

const XmlNode *XmlNode::child(const char *wanted) const
{
    const QLatin1StringView name(wanted);
    for (const XmlNode &node : children) {
        if (node.name == name)
            return &node;
    }
    return nullptr;
}

QList<const XmlNode *> XmlNode::each(const char *wanted) const
{
    const QLatin1StringView name(wanted);
    QList<const XmlNode *> found;
    for (const XmlNode &node : children) {
        if (node.name == name)
            found.append(&node);
    }
    return found;
}

const XmlNode *XmlNode::find(std::initializer_list<const char *> path) const
{
    const XmlNode *node = this;
    for (const char *step : path) {
        node = node->child(step);
        if (!node)
            return nullptr;
    }
    return node;
}

QString XmlNode::attr(const char *wanted) const
{
    return attr(wanted, QString());
}

QString XmlNode::attr(const char *wanted, const QString &fallback) const
{
    const QLatin1StringView name(wanted);
    for (const XmlAttribute &attribute : attributes) {
        if (attribute.name == name)
            return attribute.value;
    }
    return fallback;
}

bool XmlNode::has(const char *wanted) const
{
    const QLatin1StringView name(wanted);
    for (const XmlAttribute &attribute : attributes) {
        if (attribute.name == name)
            return true;
    }
    return false;
}

QString XmlNode::attrIn(const char *wanted, const QString &ns) const
{
    const QLatin1StringView name(wanted);
    for (const XmlAttribute &attribute : attributes) {
        if (attribute.name == name && attribute.ns == ns)
            return attribute.value;
    }
    return {};
}

QString XmlNode::rel(const char *wanted) const
{
    return attrIn(wanted, relationshipsNs);
}

double XmlNode::number(const char *wanted, double fallback) const
{
    const QString value = attr(wanted);
    bool ok = false;
    const double n = value.toDouble(&ok);
    return ok ? n : fallback;
}

qint64 XmlNode::integer(const char *wanted, qint64 fallback) const
{
    const QString value = attr(wanted);
    bool ok = false;
    const qint64 n = value.toLongLong(&ok);
    return ok ? n : fallback;
}

bool parseXml(const QByteArray &bytes, XmlNode *root, QString *error)
{
    QXmlStreamReader xml(bytes);
    // The nodes being built, outermost first; the root is made when its start is read
    std::vector<XmlNode *> open;
    bool started = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            XmlNode *node = nullptr;
            if (!started) {
                *root = XmlNode();
                node = root;
                started = true;
            } else {
                open.back()->children.emplace_back();
                node = &open.back()->children.back();
            }
            node->name = xml.name().toString();
            node->ns = xml.namespaceUri().toString();
            for (const QXmlStreamAttribute &attribute : xml.attributes())
                node->attributes.append({attribute.name().toString(), attribute.namespaceUri().toString(), attribute.value().toString()});
            open.push_back(node);
        } else if (xml.isEndElement()) {
            if (!open.empty())
                open.pop_back();
        } else if (xml.isCharacters() && !open.empty()) {
            open.back()->text += xml.text();
        }
    }
    if (xml.hasError()) {
        if (error)
            *error = xml.errorString();
        return false;
    }
    if (!started) {
        if (error)
            *error = QStringLiteral("the document has no elements in it");
        return false;
    }
    return true;
}
