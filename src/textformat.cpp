#include "textformat.h"

#include <QColor>

#include <cmath>

namespace textformat {

namespace {

// The outline a run is given when one is turned on: three hundredths of its size, as the
// editor's Outline does
double strokeFor(double size)
{
    return qMax(1.0, std::round(size * 0.03));
}

// Qt's horizontal flags alone (a paragraph's alignment may carry others)
int horizontal(int alignment)
{
    return alignment & int(Qt::AlignHorizontal_Mask);
}

}

QStringList styles()
{
    return {QStringLiteral("Regular"), QStringLiteral("Italic"), QStringLiteral("Bold"), QStringLiteral("Bold Italic")};
}

QString styleOf(bool bold, bool italic)
{
    return styles().at((bold ? 2 : 0) + (italic ? 1 : 0));
}

QVariantMap forRun(const QVariantMap &format, const QVariantMap &run)
{
    QVariantMap out;
    for (const QString &key : {QStringLiteral("family"), QStringLiteral("color"), QStringLiteral("capitalization"),
                               QStringLiteral("size"), QStringLiteral("strokeColor"), QStringLiteral("strokeWidth")}) {
        if (format.contains(key))
            out.insert(key, format.value(key));
    }
    if (format.contains(QStringLiteral("style"))) {
        const int at = qMax(0, styles().indexOf(format.value(QStringLiteral("style")).toString()));
        out.insert(QStringLiteral("bold"), at >= 2);
        out.insert(QStringLiteral("italic"), at % 2 == 1);
    }
    const double size = run.value(QStringLiteral("size")).toDouble();
    if (format.contains(QStringLiteral("sizeBy")))
        out.insert(QStringLiteral("size"), qMax(1.0, size + format.value(QStringLiteral("sizeBy")).toDouble()));
    const double newSize = out.value(QStringLiteral("size"), size).toDouble();
    const bool hasStroke = run.value(QStringLiteral("strokeWidth")).toDouble() > 0;
    if (format.contains(QStringLiteral("strokeOn"))) {
        if (!format.value(QStringLiteral("strokeOn")).toBool())
            out.insert(QStringLiteral("strokeWidth"), 0.0);
        else if (!hasStroke && !format.contains(QStringLiteral("strokeWidth")))
            out.insert(QStringLiteral("strokeWidth"), strokeFor(newSize));
    } else if (format.contains(QStringLiteral("strokeColor")) && !format.contains(QStringLiteral("strokeWidth")) && !hasStroke) {
        out.insert(QStringLiteral("strokeWidth"), strokeFor(newSize));
    }
    if (format.contains(QStringLiteral("strokeWidthBy")) && !format.contains(QStringLiteral("strokeWidth")) && hasStroke)
        out.insert(QStringLiteral("strokeWidth"), qMax(0.5, run.value(QStringLiteral("strokeWidth")).toDouble()
                                                                + format.value(QStringLiteral("strokeWidthBy")).toDouble()));
    // (What the run has already, it is not given again)
    for (auto it = out.begin(); it != out.end();) {
        const QVariant was = run.value(it.key());
        const bool same = it.key() == QLatin1String("color") || it.key() == QLatin1String("strokeColor")
                              ? was.value<QColor>() == it.value().value<QColor>()
                          : it.key() == QLatin1String("family") ? was.toString() == it.value().toString()
                          : it.key() == QLatin1String("bold") || it.key() == QLatin1String("italic") ? was.toBool() == it.value().toBool()
                                                                                                         : was.isValid() && was.toDouble() == it.value().toDouble();
        it = same ? out.erase(it) : std::next(it);
    }
    return out;
}

int alignmentOf(const QVariantMap &format)
{
    return format.contains(QStringLiteral("alignment")) ? horizontal(format.value(QStringLiteral("alignment")).toInt()) : -1;
}

QVariantMap forBox(const QVariantMap &asked, const QVariantMap &box)
{
    // What the box has: its shadow's colour as drawn carries the shadow's opacity
    const QColor drawn = box.value(QStringLiteral("textShadowColor")).value<QColor>();
    QColor solid = drawn;
    solid.setAlphaF(1);
    // An arrow's step, made the value it comes to for this box, which is then put as a
    // typed one is (a typed one given as well wins)
    QVariantMap format = asked;
    const auto step = [&](const QString &key, double now, double (*bounded)(double)) {
        const QString by = key + QStringLiteral("By");
        if (asked.contains(by) && !asked.contains(key))
            format.insert(key, bounded(now + asked.value(by).toDouble()));
    };
    step(QStringLiteral("textShadowRadius"), box.value(QStringLiteral("textShadowRadius")).toDouble(), [](double v) { return qMax(0.0, v); });
    // (In hundredths, as the popover shows it)
    step(QStringLiteral("textShadowOpacity"), drawn.alphaF(), [](double v) { return qBound(0.0, std::round(v * 100) / 100, 1.0); });
    step(QStringLiteral("textShadowAngle"), box.value(QStringLiteral("textShadowAngle")).toDouble(),
         [](double v) { return std::fmod(std::fmod(v, 360.0) + 360.0, 360.0); });
    step(QStringLiteral("textShadowOffset"), box.value(QStringLiteral("textShadowOffset")).toDouble(), [](double v) { return qMax(0.0, v); });
    QVariantMap out;
    const auto put = [&](const QString &key, bool differs) {
        if (format.contains(key) && differs)
            out.insert(key, format.value(key));
    };
    put(QStringLiteral("verticalAlignment"), (box.value(QStringLiteral("verticalAlignment")).toInt() & int(Qt::AlignVertical_Mask))
                                                 != (format.value(QStringLiteral("verticalAlignment")).toInt() & int(Qt::AlignVertical_Mask)));
    put(QStringLiteral("textShadowEnabled"), box.value(QStringLiteral("textShadowEnabled")).toBool() != format.value(QStringLiteral("textShadowEnabled")).toBool());
    put(QStringLiteral("textShadowSolidColor"), solid != format.value(QStringLiteral("textShadowSolidColor")).value<QColor>());
    put(QStringLiteral("textShadowOpacity"), std::abs(drawn.alphaF() - format.value(QStringLiteral("textShadowOpacity")).toDouble()) > 0.001);
    for (const QString &key : {QStringLiteral("textShadowRadius"), QStringLiteral("textShadowAngle"), QStringLiteral("textShadowOffset")})
        put(key, box.value(key).toDouble() != format.value(key).toDouble());
    // (A colour given to a shadow that is off turns it on, as the editor's does)
    if (format.contains(QStringLiteral("textShadowSolidColor")) && !format.contains(QStringLiteral("textShadowEnabled"))
        && !box.value(QStringLiteral("textShadowEnabled")).toBool())
        out.insert(QStringLiteral("textShadowEnabled"), true);
    return out;
}

void Common::add(const QString &key, const QVariant &value)
{
    if (m_mixed.contains(key))
        return;
    if (!m_values.contains(key)) {
        m_values.insert(key, value);
        return;
    }
    if (m_values.value(key) != value) {
        m_values.remove(key);
        m_mixed << key;
    }
}

void Common::addRun(const QVariantMap &run)
{
    add(QStringLiteral("family"), run.value(QStringLiteral("family")));
    add(QStringLiteral("style"), styleOf(run.value(QStringLiteral("bold")).toBool(), run.value(QStringLiteral("italic")).toBool()));
    add(QStringLiteral("size"), run.value(QStringLiteral("size")).toDouble());
    add(QStringLiteral("color"), run.value(QStringLiteral("color")));
    add(QStringLiteral("capitalization"), run.value(QStringLiteral("capitalization")).toInt());
    add(QStringLiteral("alignment"), horizontal(run.value(QStringLiteral("alignment")).toInt()));
    const double width = run.value(QStringLiteral("strokeWidth")).toDouble();
    add(QStringLiteral("strokeOn"), width > 0);
    // (The outline's width and colour, of the runs that have one)
    if (width > 0) {
        add(QStringLiteral("strokeWidth"), width);
        add(QStringLiteral("strokeColor"), run.value(QStringLiteral("strokeColor")));
    }
}

void Common::addBox(const QVariantMap &box)
{
    ++m_boxes;
    add(QStringLiteral("verticalAlignment"), box.value(QStringLiteral("verticalAlignment")).toInt() & int(Qt::AlignVertical_Mask));
    add(QStringLiteral("shadowOn"), box.value(QStringLiteral("textShadowEnabled")).toBool());
    // The colour as drawn carries the shadow's opacity, which the popover shows apart
    const QColor drawn = box.value(QStringLiteral("textShadowColor")).value<QColor>();
    QColor solid = drawn;
    solid.setAlphaF(1);
    add(QStringLiteral("shadowColor"), solid);
    add(QStringLiteral("shadowOpacity"), std::round(drawn.alphaF() * 100) / 100);
    add(QStringLiteral("shadowBlur"), box.value(QStringLiteral("textShadowRadius")).toDouble());
    add(QStringLiteral("shadowAngle"), box.value(QStringLiteral("textShadowAngle")).toDouble());
    add(QStringLiteral("shadowOffset"), box.value(QStringLiteral("textShadowOffset")).toDouble());
}

QVariantMap Common::values() const
{
    QVariantMap out = m_values;
    out.insert(QStringLiteral("boxes"), m_boxes);
    return out;
}

} // namespace textformat
