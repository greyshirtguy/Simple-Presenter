#include "maskfile.h"

#include "proworkspace.pb.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace maskfile {

namespace {

// (As in lookfile.cpp and screenfile.cpp, which keep other parts of the same file:
// this file stands by itself too.)
QString pathOf(const QString &workspace)
{
    return workspace + QStringLiteral("/Configuration/Workspace");
}

std::string newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

QString load(const QString &workspace, rv::data::ProPresenterWorkspace *document)
{
    document->Clear();
    QFile file(pathOf(workspace));
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("The workspace's set-up file could not be read: %1").arg(file.errorString());
    const QByteArray bytes = file.readAll();
    if (!document->ParseFromArray(bytes.constData(), int(bytes.size()))) {
        document->Clear();
        return QStringLiteral("The workspace's set-up file is not one this app can read, so it is left alone");
    }
    return {};
}

QString save(const QString &workspace, const rv::data::ProPresenterWorkspace &document)
{
    if (!QDir().mkpath(workspace + QStringLiteral("/Configuration")))
        return QStringLiteral("The workspace's Configuration folder could not be made");
    QSaveFile file(pathOf(workspace));
    const std::string bytes = document.SerializeAsString();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size()) || !file.commit())
        return QStringLiteral("The workspace's set-up file could not be written: %1").arg(file.errorString());
    return {};
}

Mask described(const rv::data::ProMask &mask)
{
    const std::string slide = mask.base_slide().SerializeAsString();
    return {QString::fromStdString(mask.base_slide().uuid().string()), QString::fromStdString(mask.name()),
            QByteArray(slide.data(), qsizetype(slide.size()))};
}

QList<Mask> masksOf(const rv::data::ProPresenterWorkspace &document)
{
    QList<Mask> masks;
    for (const rv::data::ProMask &mask : document.masks())
        masks.append(described(mask));
    return masks;
}

// Takes a mask's name out of a look's lines.
void forget(rv::data::ProAudienceLook *look, const std::string &id)
{
    for (rv::data::ProAudienceLook::ProScreenLook &screen : *look->mutable_screen_looks()) {
        if (screen.has_mask_uuid() && screen.mask_uuid().string() == id)
            screen.clear_mask_uuid();
    }
}

}

QList<Mask> read(const QString &workspace, QString *error)
{
    rv::data::ProPresenterWorkspace document;
    const QString failed = load(workspace, &document);
    if (error)
        *error = failed;
    return masksOf(document);
}

QString freeName(const QList<Mask> &masks, const QString &wanted)
{
    QSet<QString> taken;
    for (const Mask &mask : masks)
        taken.insert(mask.name);
    const QString base = wanted.trimmed().isEmpty() ? QStringLiteral("Mask") : wanted.trimmed();
    if (!taken.contains(base))
        return base;
    for (int number = 2;; ++number) {
        const QString name = base + u' ' + QString::number(number);
        if (!taken.contains(name))
            return name;
    }
}

QString add(const QString &workspace, const QString &name, QString *id)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    rv::data::ProMask *mask = document.add_masks();
    mask->set_name(freeName(masksOf(document).mid(0, document.masks_size() - 1), name).toStdString());
    rv::data::Slide *slide = mask->mutable_base_slide();
    slide->set_draws_background_color(false);
    slide->mutable_size()->set_width(1920);
    slide->mutable_size()->set_height(1080);
    slide->mutable_uuid()->set_string(newUuid());
    if (id)
        *id = QString::fromStdString(slide->uuid().string());
    return save(workspace, document);
}

QString rename(const QString &workspace, const QString &id, const QString &name)
{
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
        return QStringLiteral("A mask needs a name");
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    for (rv::data::ProMask &mask : *document.mutable_masks()) {
        if (id.isEmpty() || QString::fromStdString(mask.base_slide().uuid().string()) != id)
            continue;
        if (QString::fromStdString(mask.name()) == wanted)
            return {};
        mask.set_name(wanted.toStdString());
        return save(workspace, document);
    }
    return QStringLiteral("That mask is not in the workspace any more");
}

QString remove(const QString &workspace, const QString &id)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    for (int i = 0; i < document.masks_size(); ++i) {
        if (id.isEmpty() || QString::fromStdString(document.masks(i).base_slide().uuid().string()) != id)
            continue;
        document.mutable_masks()->DeleteSubrange(i, 1);
        for (rv::data::ProAudienceLook &look : *document.mutable_audience_looks())
            forget(&look, id.toStdString());
        if (document.has_live_audience_look())
            forget(document.mutable_live_audience_look(), id.toStdString());
        return save(workspace, document);
    }
    return QStringLiteral("That mask is not in the workspace any more");
}

}
