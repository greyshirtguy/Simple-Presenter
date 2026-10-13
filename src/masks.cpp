#include "masks.h"

#include "maskfile.h"
#include "proconvert.h"
#include "sessionlog.h"

#include "slide.pb.h"

QString Masks::path() const
{
    return m_workspace.isEmpty() ? QString() : m_workspace + QStringLiteral("/Configuration/Workspace");
}

QString Masks::open(const QString &workspace)
{
    m_workspace = workspace;
    return reload();
}

QString Masks::reload()
{
    QString error;
    const QList<maskfile::Mask> found = maskfile::read(m_workspace, &error);
    QVariantList masks;
    for (const maskfile::Mask &mask : found) {
        // The slide as the file has it, made ready for drawing as every slide is
        rv::data::Slide slide;
        slide.ParseFromArray(mask.slide.constData(), int(mask.slide.size()));
        QVariantMap drawn = proconvert::toSlideMap(slide, mask.name);
        drawn.insert("id", mask.id);
        masks.append(QVariantMap {{"id", mask.id}, {"name", mask.name}, {"slide", drawn}});
    }
    if (masks != m_masks) {
        m_masks = masks;
        emit changed();
    }
    return error;
}

QVariantMap Masks::find(const QString &id) const
{
    if (id.isEmpty())
        return {};
    for (const QVariant &mask : m_masks) {
        if (mask.toMap().value("id").toString() == id)
            return mask.toMap();
    }
    return {};
}

QVariantMap Masks::add(const QString &name)
{
    QString id;
    const QString error = maskfile::add(m_workspace, name, &id);
    if (error.isEmpty()) {
        reload();
        SessionLog::write("masks", QStringLiteral("made the mask \"%1\"").arg(find(id).value("name").toString()));
    }
    return {{"id", id}, {"error", error}};
}

QString Masks::rename(const QString &id, const QString &name)
{
    const QString was = find(id).value("name").toString();
    const QString error = maskfile::rename(m_workspace, id, name);
    if (error.isEmpty()) {
        reload();
        SessionLog::write("masks", QStringLiteral("the mask \"%1\" is now called \"%2\"").arg(was, find(id).value("name").toString()));
    }
    return error;
}

QString Masks::remove(const QString &id)
{
    const QString was = find(id).value("name").toString();
    const QString error = maskfile::remove(m_workspace, id);
    if (error.isEmpty()) {
        reload();
        SessionLog::write("masks", QStringLiteral("removed the mask \"%1\"").arg(was));
    }
    return error;
}
