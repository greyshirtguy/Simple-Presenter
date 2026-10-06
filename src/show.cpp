#include "show.h"

#include "proconvert.h"
#include "richtext.h"

void Show::setCurrentSlide(const QVariantMap &slide)
{
    m_current = slide;
    ++m_revision;
    emit changed();
}

void Show::setNextSlide(const QVariantMap &slide)
{
    m_next = slide;
    ++m_revision;
    emit changed();
}

QString Show::slideText(bool next, int source, const QString &name, int transform) const
{
    const QVariantMap &slide = next ? m_next : m_current;
    QString text;
    if (source == Words) {
        text = slide.value("plainText").toString();
    } else if (source == ElementNamed) {
        // Of every element of that name that shows, whichever case the name is in
        QStringList texts;
        const QVariantList elements = slide.value("elements").toList();
        for (const QVariant &entry : elements) {
            const QVariantMap element = entry.toMap();
            if (!element.value("visible").toBool() || element.value("name").toString().compare(name, Qt::CaseInsensitive) != 0)
                continue;
            const QString words = element.value("displayText").value<RichText>().plainText().trimmed();
            if (!words.isEmpty())
                texts << words;
        }
        text = texts.join(u'\n');
    }
    return proconvert::linkTransformed(text, transform);
}
