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

// The media of a media action, with how it is to be played. Empty if the file cannot be
// found.
QVariantMap toMedia(const rv::data::Action &action, workspace::FileFinder *finder)
{
    const rv::data::Media &media = action.media().element();
    const QString path = finder->find(media.url());
    if (path.isEmpty())
        return {};
    QVariantMap map {
        {"name", QFileInfo(path).fileName()},
        {"path", path},
        {"source", QUrl::fromLocalFile(path)},
        {"video", media.has_video()},
    };
    workspace::mediaBehaviour(action).describe(&map);
    return map;
}

// What a cue's timer action asks, as Timers::act() takes it.
QVariantMap toTimerAction(const rv::data::Action::TimerType &timer)
{
    QVariantMap map {
        {"action", int(timer.action_type())},
        {"timerId", QString::fromStdString(timer.timer_identification().parameter_uuid().string())},
        {"timerName", QString::fromStdString(timer.timer_identification().parameter_name())},
        {"amount", timer.increment_amount()},
    };
    // How the timer is to be set up, if the action says: passed on as it is in the
    // file, for the timers to read as they read their own.
    if (timer.has_timer_configuration()) {
        map.insert("configuration", QString::fromLatin1(
                       QByteArray::fromStdString(timer.timer_configuration().SerializeAsString()).toBase64()));
    }
    return map;
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
        {"mediaForeground", false},
        {"timerActions", QVariantList()},
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
        // The cue's media action, if it has one: its file's name, whether it is a
        // foreground, and the file if found. And what it does to timers.
        QString name;
        bool foreground = false;
        QVariantMap media;
        QVariantList timerActions;
        for (const rv::data::Action &action : cue.actions()) {
            if (!action.isenabled())
                continue;
            if (action.has_slide() && action.slide().has_presentation()) {
                slides.append(proconvert::toSlideMap(action.slide().presentation().base_slide(),
                                      QString::fromStdString(action.label().text())));
            } else if (isVisualMedia(action) && name.isEmpty()) {
                name = workspace::fileNameOf(action.media().element().url());
                foreground = workspace::mediaBehaviour(action).foreground;
                media = toMedia(action, &mediaFinder);
            } else if (action.has_timer()) {
                timerActions.append(toTimerAction(action.timer()));
            }
        }
        if (slides.isEmpty() && !name.isEmpty())
            slides.append(emptySlide(name));
        if (!slides.isEmpty()) {
            QVariantMap first = slides.first().toMap();
            if (!name.isEmpty()) {
                first.insert("mediaName", name);
                first.insert("mediaForeground", foreground);
                if (!media.isEmpty())
                    first.insert("media", media);
            }
            first.insert("timerActions", timerActions);
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

QString ProDocument::setCueMedia(const QString &path, const QString &cueId, const QString &mediaPath, bool foreground,
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
    workspace::setMediaForeground(action, foreground);

    return writePresentation(path, presentation);
}

QString ProDocument::setCueMediaForeground(const QString &path, const QString &cueId, bool foreground)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    for (rv::data::Cue &cue : *presentation.mutable_cues()) {
        if (QString::fromStdString(cue.uuid().string()) != cueId)
            continue;
        // The same action the slide's media is read from: the first image or video one.
        for (rv::data::Action &action : *cue.mutable_actions()) {
            if (action.isenabled() && isVisualMedia(action)) {
                workspace::setMediaForeground(&action, foreground);
                return writePresentation(path, presentation);
            }
        }
        return QStringLiteral("That slide has no media");
    }
    return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
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

QString ProDocument::insertMediaCues(const QString &path, const QString &cueId, bool after, const QStringList &mediaPaths,
                                     const QString &workspace)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    // The cue they go beside, if one was named, and the group it is in
    int anchor = -1;
    for (int i = 0; i < presentation.cues_size() && !cueId.isEmpty(); ++i) {
        if (QString::fromStdString(presentation.cues(i).uuid().string()) == cueId)
            anchor = i;
    }
    if (!cueId.isEmpty() && anchor < 0)
        return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
    rv::data::Presentation::CueGroup *group = nullptr;
    int placeInGroup = 0;
    for (rv::data::Presentation::CueGroup &candidate : *presentation.mutable_cue_groups()) {
        for (int i = 0; i < candidate.cue_identifiers_size() && !group && anchor >= 0; ++i) {
            if (candidate.cue_identifiers(i).string() == cueId.toStdString()) {
                group = &candidate;
                placeInGroup = i + (after ? 1 : 0);
            }
        }
    }
    // With no cue named they go at the end: of the last group, if there are groups. A
    // presentation with nothing in it at all is given a group for them, since
    // ProPresenter shows a presentation by its groups.
    if (anchor < 0) {
        if (presentation.cue_groups_size() == 0 && presentation.cues_size() == 0) {
            rv::data::Presentation::CueGroup *made = presentation.add_cue_groups();
            made->mutable_group()->mutable_uuid()->set_string(newUuid());
            made->mutable_group()->mutable_hotkey();
        }
        if (presentation.cue_groups_size() > 0) {
            group = presentation.mutable_cue_groups(presentation.cue_groups_size() - 1);
            placeInGroup = group->cue_identifiers_size();
        }
    }

    // The new slides are the size of the one they go beside, or failing that of the
    // first slide there is, or failing that of an HD screen.
    double width = 0;
    double height = 0;
    const auto sizeFrom = [&width, &height](const rv::data::Cue &cue) {
        for (const rv::data::Action &action : cue.actions()) {
            if (width > 0 || !action.has_slide() || !action.slide().has_presentation())
                continue;
            const rv::data::Graphics::Size &size = action.slide().presentation().base_slide().size();
            if (size.width() > 0 && size.height() > 0) {
                width = size.width();
                height = size.height();
            }
        }
    };
    if (anchor >= 0)
        sizeFrom(presentation.cues(anchor));
    for (const rv::data::Cue &cue : presentation.cues())
        sizeFrom(cue);
    if (width <= 0) {
        width = 1920;
        height = 1080;
    }

    int place = anchor < 0 ? presentation.cues_size() : anchor + (after ? 1 : 0);
    for (const QString &mediaPath : mediaPaths) {
        const std::string name = QFileInfo(mediaPath).fileName().toStdString();

        // The cue, laid out as ProPresenter writes one of these: a slide with nothing
        // on it, labelled with the file's name, and the media as a foreground.
        rv::data::Cue *cue = presentation.add_cues();
        cue->mutable_uuid()->set_string(newUuid());
        cue->set_name(name);
        cue->set_completion_action_type(rv::data::Cue::COMPLETION_ACTION_TYPE_LAST);
        cue->mutable_hot_key();
        cue->set_isenabled(true);

        rv::data::Action *slide = cue->add_actions();
        slide->mutable_uuid()->set_string(newUuid());
        slide->mutable_label()->set_text(name);
        slide->set_isenabled(true);
        slide->set_type(rv::data::Action::ACTION_TYPE_PRESENTATION_SLIDE);
        rv::data::Slide *base = slide->mutable_slide()->mutable_presentation()->mutable_base_slide();
        base->mutable_size()->set_width(width);
        base->mutable_size()->set_height(height);
        base->mutable_uuid()->set_string(newUuid());

        rv::data::Action *media = cue->add_actions();
        media->mutable_uuid()->set_string(newUuid());
        media->set_isenabled(true);
        media->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
        media->mutable_media()->mutable_audio();
        *media->mutable_media()->mutable_element() = workspace::mediaElement(mediaPath, workspace);
        workspace::setMediaForeground(media, true);

        // Added at the end and walked back up to its place, in the list of cues and in
        // the group's list of them.
        const std::string id = cue->uuid().string();
        for (int i = presentation.cues_size() - 1; i > place; --i)
            presentation.mutable_cues()->SwapElements(i, i - 1);
        ++place;
        if (group) {
            group->add_cue_identifiers()->set_string(id);
            for (int i = group->cue_identifiers_size() - 1; i > placeInGroup; --i)
                group->mutable_cue_identifiers()->SwapElements(i, i - 1);
            ++placeInGroup;
        }
    }

    return writePresentation(path, presentation);
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
