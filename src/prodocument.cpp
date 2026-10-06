#include "prodocument.h"

#include "proconvert.h"
#include "workspacefiles.h"

using proconvert::readPresentation;
using proconvert::writePresentation;
using workspace::newUuid;

#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QUrl>

namespace {

// Empty if the file cannot be found.
QVariantMap toMedia(const rv::data::Media &media, workspace::FileFinder *finder)
{
    const QString path = finder->find(media.url());
    if (path.isEmpty())
        return {};
    return {
        {"name", QFileInfo(path).fileName()},
        {"path", path},
        {"source", QUrl::fromLocalFile(path)},
        {"video", media.has_video()},
    };
}

bool isVisualMedia(const rv::data::Action &action)
{
    return action.has_media() && (action.media().element().has_video() || action.media().element().has_image());
}

QVariantMap emptySlide(const QString &label)
{
    return {
        {"width", 1920.0}, {"height", 1080.0},
        {"drawsBackground", false}, {"backgroundColor", QColor(Qt::transparent)},
        {"label", label},
        {"group", QString()},
        {"groupColor", QString()},
        {"groupStart", false},
        {"mediaName", QString()},
        {"plainText", QString()},
        {"elements", QVariantList()},
    };
}

// The arrangement the presentation has selected, or null.
const rv::data::Presentation::Arrangement *selectedArrangement(const rv::data::Presentation &presentation)
{
    if (!presentation.has_selected_arrangement())
        return nullptr;
    for (const auto &candidate : presentation.arrangements()) {
        if (candidate.uuid().string() == presentation.selected_arrangement().string())
            return &candidate;
    }
    return nullptr;
}

} // namespace

ProDocument ProDocument::load(const QString &path, const QString &workspace,
                              const std::optional<QString> &arrangement, QString *error)
{
    rv::data::Presentation presentation;
    if (!readPresentation(path, &presentation, error))
        return {};

    QHash<std::string, const rv::data::Cue *> cuesById;
    for (const rv::data::Cue &cue : presentation.cues())
        cuesById.insert(cue.uuid().string(), &cue);
    QHash<std::string, const rv::data::Presentation::CueGroup *> groupsById;
    for (const auto &group : presentation.cue_groups())
        groupsById.insert(group.group().uuid().string(), &group);

    ProDocument document;
    document.name = presentation.name().empty() ? QFileInfo(path).completeBaseName()
                                                 : QString::fromStdString(presentation.name());
    for (const auto &candidate : presentation.arrangements())
        document.arrangements << QString::fromStdString(candidate.name());

    // Which arrangement: the one asked for by name, or the one the document has selected.
    const rv::data::Presentation::Arrangement *chosen = nullptr;
    if (!arrangement) {
        chosen = selectedArrangement(presentation);
    } else {
        for (const auto &candidate : presentation.arrangements()) {
            if (QString::fromStdString(candidate.name()) == *arrangement) {
                chosen = &candidate;
                break;
            }
        }
    }

    // Display order: the arrangement's groups, which may repeat, or with no arrangement
    // every group as stored (ProPresenter's "Master"). Each group lists its cues.
    QList<const rv::data::Presentation::CueGroup *> groups;
    if (chosen) {
        document.arrangement = QString::fromStdString(chosen->name());
        for (const rv::data::UUID &id : chosen->group_identifiers()) {
            if (const auto *group = groupsById.value(id.string()))
                groups.append(group);
        }
    } else {
        for (const auto &group : presentation.cue_groups())
            groups.append(&group);
    }

    // A cue becomes one grid entry: its slide, plus the media it triggers alongside. A cue
    // with media and no slide still gets an entry, so the media can be triggered.
    // One finder for the whole presentation, so that its media is only looked for by
    // name, if it comes to that, with one pass over the Media folder.
    workspace::FileFinder mediaFinder(workspace, QStringLiteral("Media"));
    const auto slidesForCue = [&mediaFinder](const rv::data::Cue &cue) {
        QVariantList slides;
        if (!cue.isenabled())
            return slides;
        const QString id = QString::fromStdString(cue.uuid().string());
        // The cue's media action, if it has one: its file's name, and the file if found.
        QString name;
        QVariantMap media;
        for (const rv::data::Action &action : cue.actions()) {
            if (!action.isenabled())
                continue;
            if (action.has_slide() && action.slide().has_presentation()) {
                slides.append(proconvert::toSlideMap(action.slide().presentation().base_slide(),
                                      QString::fromStdString(action.label().text())));
            } else if (isVisualMedia(action) && name.isEmpty()) {
                name = workspace::fileNameOf(action.media().element().url());
                media = toMedia(action.media().element(), &mediaFinder);
            }
        }
        if (slides.isEmpty() && !name.isEmpty())
            slides.append(emptySlide(name));
        if (!slides.isEmpty() && !name.isEmpty()) {
            QVariantMap first = slides.first().toMap();
            first.insert("mediaName", name);
            if (!media.isEmpty())
                first.insert("media", media);
            slides.first() = first;
        }
        for (QVariant &entry : slides) {
            QVariantMap slide = entry.toMap();
            slide.insert("id", id);
            entry = slide;
        }
        return slides;
    };

    if (groups.isEmpty()) {
        for (const rv::data::Cue &cue : presentation.cues())
            document.slides.append(slidesForCue(cue));
    } else {
        // Every slide knows its group; the first of each run of a group is marked, so
        // the group's name can be shown once.
        for (const auto *group : groups) {
            const rv::data::Color &color = group->group().color();
            bool first = true;
            for (const rv::data::UUID &id : group->cue_identifiers()) {
                const rv::data::Cue *cue = cuesById.value(id.string());
                if (!cue)
                    continue;
                for (const QVariant &entry : slidesForCue(*cue)) {
                    QVariantMap slide = entry.toMap();
                    slide.insert("group", QString::fromStdString(group->group().name()));
                    slide.insert("groupColor", group->group().has_color() && color.alpha() > 0
                                                   ? proconvert::toColor(color).name() : QString());
                    slide.insert("groupStart", first);
                    first = false;
                    document.slides.append(slide);
                }
            }
        }
    }

    if (document.slides.isEmpty())
        *error = QStringLiteral("%1 contains no slides").arg(QFileInfo(path).fileName());
    return document;
}

// Every list of presentations shows each one's arrangement, and the lists are read again
// whenever anything in the workspace changes, so this is asked of every file over and
// over. The answer is kept for as long as the file is the same file: same size, same
// time of last change.
bool ProDocument::arrangementsOf(const QString &path, QStringList *names, QString *selected)
{
    struct Known
    {
        qint64 size;
        QDateTime changed;
        QStringList names;
        QString selected;
    };
    static QHash<QString, Known> known;
    static QMutex mutex;

    const QFileInfo file(path);
    {
        const QMutexLocker lock(&mutex);
        const auto found = known.constFind(path);
        if (found != known.constEnd() && found->size == file.size() && found->changed == file.lastModified()) {
            *names = found->names;
            *selected = found->selected;
            return true;
        }
    }

    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return false;
    names->clear();
    for (const auto &candidate : presentation.arrangements())
        names->append(QString::fromStdString(candidate.name()));
    const auto *chosen = selectedArrangement(presentation);
    *selected = chosen ? QString::fromStdString(chosen->name()) : QString();

    const QMutexLocker lock(&mutex);
    known.insert(path, {file.size(), file.lastModified(), *names, *selected});
    return true;
}

QString ProDocument::setArrangement(const QString &path, const QString &name)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    if (name.isEmpty()) {
        presentation.clear_selected_arrangement();
    } else {
        const rv::data::Presentation::Arrangement *wanted = nullptr;
        for (const auto &candidate : presentation.arrangements()) {
            if (QString::fromStdString(candidate.name()) == name) {
                wanted = &candidate;
                break;
            }
        }
        if (!wanted)
            return QStringLiteral("%1 has no arrangement named %2").arg(QFileInfo(path).fileName(), name);
        *presentation.mutable_selected_arrangement() = wanted->uuid();
    }

    return writePresentation(path, presentation);
}

QString ProDocument::setCueMedia(const QString &path, const QString &cueId, const QString &mediaPath,
                                 const QString &workspace)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    rv::data::Cue *cue = nullptr;
    for (rv::data::Cue &candidate : *presentation.mutable_cues()) {
        if (QString::fromStdString(candidate.uuid().string()) == cueId) {
            cue = &candidate;
            break;
        }
    }
    if (!cue)
        return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());

    // Replace the media of the cue's existing media action, keeping that action's own
    // settings, or add an action the way ProPresenter writes a background.
    rv::data::Action *action = nullptr;
    for (rv::data::Action &candidate : *cue->mutable_actions()) {
        if (isVisualMedia(candidate)) {
            action = &candidate;
            break;
        }
    }
    if (!action) {
        action = cue->add_actions();
        action->mutable_uuid()->set_string(newUuid());
        action->set_isenabled(true);
        action->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
        action->mutable_media()->set_layer_type(rv::data::Action::LAYER_TYPE_BACKGROUND);
        action->mutable_media()->mutable_audio();
    }
    // It described the old file's length.
    action->clear_duration();

    *action->mutable_media()->mutable_element() = workspace::mediaElement(mediaPath, workspace);

    return writePresentation(path, presentation);
}

QString ProDocument::removeCueMedia(const QString &path, const QString &cueId)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    for (rv::data::Cue &cue : *presentation.mutable_cues()) {
        if (QString::fromStdString(cue.uuid().string()) != cueId)
            continue;
        // The same action the slide's media is read from: the first image or video one.
        for (int i = 0; i < cue.actions_size(); ++i) {
            if (cue.actions(i).isenabled() && isVisualMedia(cue.actions(i))) {
                cue.mutable_actions()->DeleteSubrange(i, 1);
                return writePresentation(path, presentation);
            }
        }
        return {};
    }
    return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
}

QString ProDocument::arrangementId(const QString &path, const QString &name)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return {};
    for (const auto &candidate : presentation.arrangements()) {
        if (QString::fromStdString(candidate.name()) == name)
            return QString::fromStdString(candidate.uuid().string());
    }
    return {};
}
