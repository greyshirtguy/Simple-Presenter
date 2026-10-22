#include "prodocument.h"

#include "builds.h"
#include "chords.h"
#include "arrangementedit.h"
#include "groupedit.h"
#include "labelfile.h"
#include "mediasettings.h"
#include "playbackmarkers.h"

#include "themefile.h"

#include "actions.h"
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
#include <QSizeF>
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
    // What its Media Inspector sets, for media set to anything (src/mediasettings.h)
    if (mediasettings::isSet(media))
        map.insert("settings", mediasettings::describe(media));
    workspace::mediaBehaviour(action).describe(&map);
    // Its playback markers, for media that has any (src/playbackmarkers.h)
    const QVariantList markers = playbackmarkers::describe(action, actions::describe);
    if (!markers.isEmpty())
        map.insert("markers", markers);
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
        {"labelColor", QString()},
        {"group", QString()},
        {"groupColor", QString()},
        {"groupStart", false},
        {"mediaName", QString()},
        {"mediaForeground", false},
        {"mediaVideo", false},
        {"mediaPlayback", 0},
        {"mediaLoopCount", 0},
        {"mediaLoopSeconds", 0.0},
        {"actions", QVariantList()},
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

// A cue's Go To Next Timer as a slide's map has it, {seconds, loop}, or an empty map for
// a cue with none: see setCueGoToNext() in the header for how the file says it. A cue
// whose target is none has no timer, whatever time it has kept from one it had once.
//
// (The file has two more targets, a random cue and a named one, and another way of
// being done, after an action. This app follows none of those: such a cue has no timer
// here, and is left as it is.)
QVariantMap goToNextOf(const rv::data::Cue &cue)
{
    using Cue = rv::data::Cue;
    const bool first = cue.completion_target_type() == Cue::COMPLETION_TARGET_TYPE_FIRST;
    if (cue.completion_action_type() != Cue::COMPLETION_ACTION_TYPE_AFTER_TIME
        || (!first && cue.completion_target_type() != Cue::COMPLETION_TARGET_TYPE_NEXT))
        return {};
    return {{"seconds", cue.completion_time()}, {"loop", first}};
}

// Where a presentation says it is sent, as a document has it: "" or "announcements".
QString destinationOf(const rv::data::Presentation &presentation)
{
    return presentation.content_destination() == rv::data::Action::CONTENT_DESTINATION_ANNOUNCEMENTS ? QStringLiteral("announcements")
                                                                                                    : QString();
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
    document.uuid = QString::fromStdString(presentation.uuid().string());
    for (const auto &candidate : presentation.arrangements())
        document.arrangements << QString::fromStdString(candidate.name());
    // For the arrangement editor: the groups as Master has them, and each
    // arrangement's own run of them.
    for (const auto &group : presentation.cue_groups()) {
        const rv::data::Color &color = group.group().color();
        document.groups.append(QVariantMap{
            {"id", QString::fromStdString(group.group().uuid().string())},
            {"name", QString::fromStdString(group.group().name())},
            {"color", group.group().has_color() && color.alpha() > 0 ? proconvert::toColor(color).name() : QString()},
            {"slides", group.cue_identifiers_size()},
        });
    }
    for (const auto &candidate : presentation.arrangements()) {
        // (Every id it has, in the file's order, so that a place in this list is the
        // same place in the file: one that is no group's any more is there too.)
        QStringList ids;
        for (const rv::data::UUID &id : candidate.group_identifiers())
            ids << QString::fromStdString(id.string());
        document.arrangementList.append(QVariantMap{
            {"id", QString::fromStdString(candidate.uuid().string())},
            {"name", QString::fromStdString(candidate.name())},
            {"groups", ids},
        });
    }
    // The key. A file with no `music` block names none; one with a block and no key in
    // it means A flat, that being number 0, which the file format leaves unwritten.
    if (presentation.has_music()) {
        using Scale = rv::data::MusicKeyScale;
        const auto name = [](const Scale &key) {
            return chords::keyName(int(key.music_key()), key.music_scale() == Scale::MUSIC_SCALE_MINOR);
        };
        document.originalKey = name(presentation.music().original());
        document.userKey = presentation.music().has_user() ? name(presentation.music().user()) : document.originalKey;
    }
    document.destination = destinationOf(presentation);
    document.slideShow = presentation.has_slide_show_duration() ? presentation.slide_show_duration() : 0;
    if (presentation.has_transition())
        document.transition = builds::describeTransition(presentation.transition());

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
        // The cue's media action, if it has one: its file's name, how it plays, and
        // the file if found. And the actions it has besides.
        QString name;
        bool video = false;
        workspace::MediaBehaviour behaviour;
        QVariantMap media;
        QVariantList others;
        for (const rv::data::Action &action : cue.actions()) {
            if (!action.isenabled())
                continue;
            if (action.has_slide() && action.slide().has_presentation()) {
                QVariantMap slide = proconvert::toSlideMap(action.slide().presentation().base_slide(),
                                                           QString::fromStdString(action.label().text()));
                // The colour of its label, which the label has beside its words
                slide.insert("labelColor", labelfile::read(action.label()).color);
                // Its own transition, if it has one (see builds.h): what it is brought
                // in with, in place of the presentation's or the one in use
                if (action.slide().presentation().has_transition())
                    slide.insert("transition", builds::describeTransition(action.slide().presentation().transition()));
                // Its notes, if it has any: for a text box that shows the live slide's
                proconvert::addNotes(action.slide().presentation(), &slide);
                slides.append(slide);
            } else if (isVisualMedia(action) && name.isEmpty()) {
                name = workspace::fileNameOf(action.media().element().url());
                video = action.media().element().has_video();
                behaviour = workspace::mediaBehaviour(action);
                media = toMedia(action, &mediaFinder);
            } else if (actions::listed(action)) {
                others.append(actions::describe(action));
            }
        }
        if (slides.isEmpty() && !name.isEmpty())
            slides.append(emptySlide(name));
        if (!slides.isEmpty()) {
            QVariantMap first = slides.first().toMap();
            if (!name.isEmpty()) {
                first.insert("mediaName", name);
                first.insert("mediaForeground", behaviour.foreground);
                first.insert("mediaVideo", video);
                first.insert("mediaPlayback", behaviour.playback);
                first.insert("mediaLoopCount", behaviour.loopCount);
                first.insert("mediaLoopSeconds", behaviour.loopSeconds);
                if (!media.isEmpty())
                    first.insert("media", media);
            }
            first.insert("actions", others);
            slides.first() = first;
        }
        const QVariantMap goToNext = goToNextOf(cue);
        for (QVariant &entry : slides) {
            QVariantMap slide = entry.toMap();
            slide.insert("id", id);
            if (!goToNext.isEmpty())
                slide.insert("goToNext", goToNext);
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
    for (const QVariant &slide : std::as_const(document.slides)) {
        const QVariantList elements = slide.toMap().value("elements").toList();
        if (std::any_of(elements.cbegin(), elements.cend(), [](const QVariant &element) { return element.toMap().contains("chords"); })) {
            document.hasChords = true;
            break;
        }
    }
    return document;
}

QString ProDocument::setOriginalKey(const QString &path, const QString &key)
{
    const int number = chords::keyNumber(key);
    if (number < 0)
        return QStringLiteral("%1 is not a key").arg(key);
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    using Scale = rv::data::MusicKeyScale;
    for (Scale *target : {presentation.mutable_music()->mutable_original(), presentation.mutable_music()->mutable_user()}) {
        target->set_music_key(Scale::MusicKey(number));
        target->set_music_scale(chords::keyIsMinor(key) ? Scale::MUSIC_SCALE_MINOR : Scale::MUSIC_SCALE_MAJOR);
    }
    return writePresentation(path, presentation);
}

// Every list of presentations shows each one's arrangement, and the lists are read again
// whenever anything in the workspace changes, so this is asked of every file over and
// over. The answer is kept for as long as the file is the same file: same size, same
// time of last change.
bool ProDocument::arrangementsOf(const QString &path, QStringList *names, QString *selected, QStringList *ids, QString *destination)
{
    struct Known
    {
        qint64 size;
        QDateTime changed;
        QStringList names;
        QString selected;
        QStringList ids;
        QString destination;
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
            if (ids)
                *ids = found->ids;
            if (destination)
                *destination = found->destination;
            return true;
        }
    }

    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return false;
    names->clear();
    QStringList theirIds;
    for (const auto &candidate : presentation.arrangements()) {
        names->append(QString::fromStdString(candidate.name()));
        theirIds.append(QString::fromStdString(candidate.uuid().string()));
    }
    const auto *chosen = selectedArrangement(presentation);
    *selected = chosen ? QString::fromStdString(chosen->name()) : QString();
    if (ids)
        *ids = theirIds;
    const QString sentTo = destinationOf(presentation);
    if (destination)
        *destination = sentTo;

    const QMutexLocker lock(&mutex);
    known.insert(path, {file.size(), file.lastModified(), *names, *selected, theirIds, sentTo});
    return true;
}

QString ProDocument::setDestination(const QString &path, const QString &destination)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    const auto wanted = destination == QLatin1String("announcements") ? rv::data::Action::CONTENT_DESTINATION_ANNOUNCEMENTS
                                                                      : rv::data::Action::CONTENT_DESTINATION_GLOBAL;
    // A file that says so already is left as it is, to the byte.
    if (presentation.content_destination() == wanted)
        return {};
    presentation.set_content_destination(wanted);
    return writePresentation(path, presentation);
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

namespace {

// What went wrong with a change to an arrangement, in words; "" for nothing.
QString said(arrangementedit::Outcome outcome, const QString &path, const QString &name = QString())
{
    using arrangementedit::Outcome;
    const QString file = QFileInfo(path).fileName();
    switch (outcome) {
    case Outcome::NoSuchArrangement:
        return QStringLiteral("%1 no longer has that arrangement").arg(file);
    case Outcome::NoSuchGroup:
        return QStringLiteral("%1 no longer has that group").arg(file);
    case Outcome::NoSuchPlace:
        return QStringLiteral("That arrangement of %1 is not as it was: it has no such place in it").arg(file);
    case Outcome::NoName:
        return QStringLiteral("An arrangement needs a name");
    case Outcome::NameTaken:
        return QStringLiteral("%1 has an arrangement named %2 already").arg(file, name.trimmed());
    case Outcome::Done:
    case Outcome::Unchanged:
        break;
    }
    return {};
}

// Reads the presentation, makes a change to its arrangements, and writes it back if
// the change was made.
template <typename Change>
QString changeArrangements(const QString &path, const QString &name, Change change)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    const arrangementedit::Outcome outcome = change(&presentation);
    if (outcome != arrangementedit::Outcome::Done)
        return said(outcome, path, name);
    return writePresentation(path, presentation);
}

} // namespace

QString ProDocument::addArrangement(const QString &path, QString *name)
{
    return changeArrangements(path, {}, [name](rv::data::Presentation *presentation) {
        *name = arrangementedit::add(presentation, QStringLiteral("New Arrangement"), newUuid());
        return arrangementedit::Outcome::Done;
    });
}

QString ProDocument::duplicateArrangement(const QString &path, const QString &id, QString *name)
{
    return changeArrangements(path, {}, [&id, name](rv::data::Presentation *presentation) {
        return arrangementedit::duplicate(presentation, id.toStdString(), newUuid(), name);
    });
}

QString ProDocument::renameArrangement(const QString &path, const QString &id, const QString &name)
{
    return changeArrangements(path, name, [&id, &name](rv::data::Presentation *presentation) {
        return arrangementedit::rename(presentation, id.toStdString(), name);
    });
}

QString ProDocument::removeArrangement(const QString &path, const QString &id)
{
    return changeArrangements(path, {}, [&id](rv::data::Presentation *presentation) {
        return arrangementedit::remove(presentation, id.toStdString());
    });
}

QString ProDocument::insertArrangementGroup(const QString &path, const QString &id, const QString &groupId, int landing)
{
    return changeArrangements(path, {}, [&id, &groupId, landing](rv::data::Presentation *presentation) {
        return arrangementedit::insertGroup(presentation, id.toStdString(), groupId.toStdString(), landing);
    });
}

QString ProDocument::moveArrangementGroup(const QString &path, const QString &id, int from, int landing)
{
    return changeArrangements(path, {}, [&id, from, landing](rv::data::Presentation *presentation) {
        return arrangementedit::moveGroup(presentation, id.toStdString(), from, landing);
    });
}

QString ProDocument::removeArrangementGroup(const QString &path, const QString &id, int place)
{
    return changeArrangements(path, {}, [&id, place](rv::data::Presentation *presentation) {
        return arrangementedit::removeGroup(presentation, id.toStdString(), place);
    });
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

QString ProDocument::setSlideShow(const QString &path, double seconds)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    const double had = presentation.has_slide_show_duration() ? presentation.slide_show_duration() : 0;
    if (seconds <= 0 ? !presentation.has_slide_show_duration() : had == seconds)
        return {};
    if (seconds > 0)
        presentation.set_slide_show_duration(seconds);
    else
        presentation.clear_slide_show_duration();
    return writePresentation(path, presentation);
}

QString ProDocument::setCueGoToNext(const QString &path, const QString &cueId, bool on, double seconds, bool loop)
{
    using Cue = rv::data::Cue;
    // What a cue without a timer said of being done before this app first gave it one,
    // in this run: so that a change of mind puts back exactly that.
    struct Was
    {
        Cue::CompletionActionType action;
        double time;
    };
    static QHash<QString, Was> before;
    static QMutex mutex;

    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    for (Cue &cue : *presentation.mutable_cues()) {
        if (QString::fromStdString(cue.uuid().string()) != cueId)
            continue;
        const QString key = path + u'\n' + cueId;
        const QMutexLocker lock(&mutex);
        if (on) {
            if (goToNextOf(cue).isEmpty() && !before.contains(key))
                before.insert(key, {cue.completion_action_type(), cue.completion_time()});
            cue.set_completion_target_type(loop ? Cue::COMPLETION_TARGET_TYPE_FIRST : Cue::COMPLETION_TARGET_TYPE_NEXT);
            cue.set_completion_action_type(Cue::COMPLETION_ACTION_TYPE_AFTER_TIME);
            cue.set_completion_time(qMax(0.0, seconds));
        } else {
            if (goToNextOf(cue).isEmpty())
                return {};
            cue.set_completion_target_type(Cue::COMPLETION_TARGET_TYPE_NONE);
            const auto was = before.constFind(key);
            if (was != before.constEnd()) {
                cue.set_completion_action_type(was->action);
                cue.set_completion_time(was->time);
                before.erase(was);
            } else {
                cue.set_completion_action_type(Cue::COMPLETION_ACTION_TYPE_LAST);
            }
        }
        return writePresentation(path, presentation);
    }
    return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
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
                workspace::setMediaLayer(&action, foreground);
                return writePresentation(path, presentation);
            }
        }
        return QStringLiteral("That slide has no media");
    }
    return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
}

QString ProDocument::setCueMediaPlayback(const QString &path, const QString &cueId, int playback, int loopCount, double loopSeconds)
{
    return changeCue(path, cueId, [&](rv::data::Cue *cue) {
        for (rv::data::Action &action : *cue->mutable_actions()) {
            if (action.isenabled() && isVisualMedia(action)) {
                if (!action.media().element().has_video())
                    return QStringLiteral("Only a video has a way of playing on from its end");
                workspace::setMediaPlayback(&action, playback, loopCount, loopSeconds);
                return QString();
            }
        }
        return QStringLiteral("That slide has no media");
    });
}

QString ProDocument::changeCueMedia(const QString &path, const QString &cueId, const std::function<QString(rv::data::Action *)> &change)
{
    return changeCue(path, cueId, [&](rv::data::Cue *cue) {
        for (rv::data::Action &action : *cue->mutable_actions()) {
            if (action.isenabled() && isVisualMedia(action))
                return change(&action);
        }
        return QStringLiteral("That slide has no media");
    });
}

QString ProDocument::setCueMediaSettings(const QString &path, const QString &cueId, const QVariantMap &changes)
{
    return changeCue(path, cueId, [&](rv::data::Cue *cue) {
        for (rv::data::Action &action : *cue->mutable_actions()) {
            if (action.isenabled() && isVisualMedia(action))
                return mediasettings::apply(action.mutable_media()->mutable_element(), changes);
        }
        return QStringLiteral("That slide has no media");
    });
}

QVariantMap ProDocument::cueMediaSettings(const QString &path, const QString &cueId)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return {};
    for (const rv::data::Cue &cue : presentation.cues()) {
        if (QString::fromStdString(cue.uuid().string()) != cueId)
            continue;
        for (const rv::data::Action &action : cue.actions()) {
            if (action.isenabled() && isVisualMedia(action))
                return mediasettings::describe(action.media().element());
        }
    }
    return {};
}

QString ProDocument::replaceCueMediaFile(const QString &path, const QString &cueId, const QString &mediaPath,
                                         const QString &workspace)
{
    return changeCue(path, cueId, [&](rv::data::Cue *cue) {
        for (rv::data::Action &action : *cue->mutable_actions()) {
            if (action.isenabled() && isVisualMedia(action)) {
                rv::data::Media made = workspace::mediaElement(mediaPath, workspace);
                mediasettings::carryOver(action.media().element(), &made);
                *action.mutable_media()->mutable_element() = std::move(made);
                // It described the old file's length.
                action.clear_duration();
                return QString();
            }
        }
        return QStringLiteral("That slide has no media");
    });
}

QString ProDocument::addCueAction(const QString &path, const QString &cueId, const QVariantMap &action)
{
    return changeCue(path, cueId, [&](rv::data::Cue *cue) {
        rv::data::Action made;
        const QString error = actions::build(action, &made);
        if (error.isEmpty())
            *cue->add_actions() = made;
        return error;
    });
}

QString ProDocument::changeCueAction(const QString &path, const QString &cueId, const QString &actionId, const QVariantMap &action)
{
    return changeCue(path, cueId, [&](rv::data::Cue *cue) {
        for (rv::data::Action &candidate : *cue->mutable_actions()) {
            if (QString::fromStdString(candidate.uuid().string()) == actionId && actions::listed(candidate))
                return actions::build(action, &candidate);
        }
        return QStringLiteral("That slide no longer has that action");
    });
}

QString ProDocument::removeCueAction(const QString &path, const QString &cueId, const QString &actionId)
{
    return changeCue(path, cueId, [&](rv::data::Cue *cue) {
        for (int i = 0; i < cue->actions_size(); ++i) {
            if (QString::fromStdString(cue->actions(i).uuid().string()) == actionId && actions::listed(cue->actions(i))) {
                cue->mutable_actions()->DeleteSubrange(i, 1);
                return QString();
            }
        }
        return QStringLiteral("That slide no longer has that action");
    });
}

QString ProDocument::dressCues(const QString &path, const QStringList &cueIds, const rv::data::Slide &theme,
                               const QSet<QString> &themeElements, int *dressed)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    int count = 0;
    for (rv::data::Cue &cue : *presentation.mutable_cues()) {
        if (!cueIds.isEmpty() && !cueIds.contains(QString::fromStdString(cue.uuid().string())))
            continue;
        for (rv::data::Action &action : *cue.mutable_actions()) {
            if (!action.has_slide() || !action.slide().has_presentation())
                continue;
            themefile::dress(action.mutable_slide()->mutable_presentation()->mutable_base_slide(), theme, themeElements);
            ++count;
        }
    }
    if (dressed)
        *dressed = count;
    if (count == 0)
        return QStringLiteral("%1 has no such slides any more").arg(QFileInfo(path).fileName());
    return writePresentation(path, presentation);
}

QVariantMap ProDocument::dressedSlide(const QString &path, const QString &cueId, const rv::data::Slide &theme,
                                      const QSet<QString> &themeElements)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return {};
    for (rv::data::Cue &cue : *presentation.mutable_cues()) {
        if (QString::fromStdString(cue.uuid().string()) != cueId)
            continue;
        for (rv::data::Action &action : *cue.mutable_actions()) {
            if (!action.isenabled() || !action.has_slide() || !action.slide().has_presentation())
                continue;
            rv::data::Slide *slide = action.mutable_slide()->mutable_presentation()->mutable_base_slide();
            // (For a screen: with the theme's builds, see themefile::dress.)
            themefile::dress(slide, theme, themeElements, true);
            return proconvert::toSlideMap(*slide, QString::fromStdString(action.label().text()));
        }
    }
    return {};
}

QByteArray ProDocument::slideOfCue(const QString &path, const QString &cueId, QString *error)
{
    rv::data::Presentation presentation;
    if (!readPresentation(path, &presentation, error))
        return {};
    for (const rv::data::Cue &cue : presentation.cues()) {
        if (QString::fromStdString(cue.uuid().string()) != cueId)
            continue;
        for (const rv::data::Action &action : cue.actions()) {
            if (!action.isenabled() || !action.has_slide() || !action.slide().has_presentation())
                continue;
            const rv::data::Slide &slide = action.slide().presentation().base_slide();
            // (A slide with no words has no text style: dressing with it would do nothing.)
            if (!themefile::hasWords(slide)) {
                *error = QStringLiteral("That slide has no words, so it has no text style to copy");
                return {};
            }
            return QByteArray::fromStdString(slide.SerializeAsString());
        }
        *error = QStringLiteral("That slide only triggers media, so it has no text style to copy");
        return {};
    }
    *error = QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
    return {};
}

QString ProDocument::dressCuesText(const QString &path, const QStringList &cueIds, const rv::data::Slide &style, int *restyled)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    int count = 0;
    bool found = false;
    for (rv::data::Cue &cue : *presentation.mutable_cues()) {
        if (!cueIds.contains(QString::fromStdString(cue.uuid().string())))
            continue;
        for (rv::data::Action &action : *cue.mutable_actions()) {
            if (!action.has_slide() || !action.slide().has_presentation())
                continue;
            found = true;
            count += themefile::dressText(action.mutable_slide()->mutable_presentation()->mutable_base_slide(), style);
        }
    }
    if (restyled)
        *restyled = count;
    if (!found)
        return QStringLiteral("%1 has no such slides any more").arg(QFileInfo(path).fileName());
    if (count == 0)
        return cueIds.size() == 1 ? QStringLiteral("That slide has no words for the copied text style to go on")
                                  : QStringLiteral("None of those slides has words for the copied text style to go on");
    return writePresentation(path, presentation);
}

QString ProDocument::setCueLabel(const QString &path, const QString &cueId, const QString &text, const QString &color)
{
    return changeCue(path, cueId, [&](rv::data::Cue *cue) {
        return labelfile::set(cue, {text, color}) ? QString() : QStringLiteral("That is not a slide that can have a label");
    });
}

QString ProDocument::changeCue(const QString &path, const QString &cueId, const std::function<QString(rv::data::Cue *)> &change)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    for (rv::data::Cue &cue : *presentation.mutable_cues()) {
        if (QString::fromStdString(cue.uuid().string()) != cueId)
            continue;
        error = change(&cue);
        return error.isEmpty() ? writePresentation(path, presentation) : error;
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

QString ProDocument::removeCue(const QString &path, const QString &cueId)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    int found = -1;
    for (int i = 0; i < presentation.cues_size(); ++i) {
        if (QString::fromStdString(presentation.cues(i).uuid().string()) == cueId)
            found = i;
    }
    if (found < 0)
        return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
    presentation.mutable_cues()->DeleteSubrange(found, 1);
    for (rv::data::Presentation::CueGroup &group : *presentation.mutable_cue_groups()) {
        for (int i = group.cue_identifiers_size() - 1; i >= 0; --i) {
            if (group.cue_identifiers(i).string() == cueId.toStdString())
                group.mutable_cue_identifiers()->DeleteSubrange(i, 1);
        }
    }
    return writePresentation(path, presentation);
}

namespace {

// Where in a presentation new cues go: their place in its list of cues, and the group
// that lists them, if any, with their place in that.
struct Place
{
    int anchor = -1;
    int cue = 0;
    rv::data::Presentation::CueGroup *group = nullptr;
    int inGroup = 0;
};

// The place just before the cue with this id, or with `after` just after it, in the
// presentation and in that cue's group; with no cue named, the end. False if a cue was
// named that the presentation does not have.
bool findPlace(rv::data::Presentation *presentation, const QString &cueId, bool after, Place *place)
{
    for (int i = 0; i < presentation->cues_size() && !cueId.isEmpty(); ++i) {
        if (QString::fromStdString(presentation->cues(i).uuid().string()) == cueId)
            place->anchor = i;
    }
    if (!cueId.isEmpty() && place->anchor < 0)
        return false;
    for (rv::data::Presentation::CueGroup &candidate : *presentation->mutable_cue_groups()) {
        for (int i = 0; i < candidate.cue_identifiers_size() && !place->group && place->anchor >= 0; ++i) {
            if (candidate.cue_identifiers(i).string() == cueId.toStdString()) {
                place->group = &candidate;
                place->inGroup = i + (after ? 1 : 0);
            }
        }
    }
    // With no cue named they go at the end: of the last group, if there are groups. A
    // presentation with nothing in it at all is given a group for them, since
    // ProPresenter shows a presentation by its groups.
    if (place->anchor < 0) {
        if (presentation->cue_groups_size() == 0 && presentation->cues_size() == 0) {
            rv::data::Presentation::CueGroup *made = presentation->add_cue_groups();
            made->mutable_group()->mutable_uuid()->set_string(newUuid());
            made->mutable_group()->mutable_hotkey();
        }
        if (presentation->cue_groups_size() > 0) {
            place->group = presentation->mutable_cue_groups(presentation->cue_groups_size() - 1);
            place->inGroup = place->group->cue_identifiers_size();
        }
    }
    place->cue = place->anchor < 0 ? presentation->cues_size() : place->anchor + (after ? 1 : 0);
    return true;
}

// The cue that has just been added at the end is walked back up to its place, in the
// list of cues and in the group's list of them, and the place moves on past it.
void settleCue(rv::data::Presentation *presentation, Place *place)
{
    const std::string id = presentation->cues(presentation->cues_size() - 1).uuid().string();
    for (int i = presentation->cues_size() - 1; i > place->cue; --i)
        presentation->mutable_cues()->SwapElements(i, i - 1);
    ++place->cue;
    if (place->group) {
        place->group->add_cue_identifiers()->set_string(id);
        for (int i = place->group->cue_identifiers_size() - 1; i > place->inGroup; --i)
            place->group->mutable_cue_identifiers()->SwapElements(i, i - 1);
        ++place->inGroup;
    }
}

// The size for a new slide: that of the one at `anchor`, or failing that of the first
// slide there is, or failing that of an HD screen.
QSizeF newSlideSize(const rv::data::Presentation &presentation, int anchor)
{
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
    return width > 0 ? QSizeF(width, height) : QSizeF(1920, 1080);
}

} // namespace

QString ProDocument::insertMediaCues(const QString &path, const QString &cueId, bool after, const QStringList &mediaPaths,
                                     const QString &workspace)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    Place place;
    if (!findPlace(&presentation, cueId, after, &place))
        return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());

    for (const QString &mediaPath : mediaPaths) {
        // A slide with nothing on it, labelled with the file's name, and the media as
        // a foreground.
        rv::data::Cue *cue = proconvert::addBlankCue(&presentation, QFileInfo(mediaPath).fileName().toStdString(),
                                                     newSlideSize(presentation, place.anchor));
        rv::data::Action *media = cue->add_actions();
        media->mutable_uuid()->set_string(newUuid());
        media->set_isenabled(true);
        media->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
        media->mutable_media()->mutable_audio();
        *media->mutable_media()->mutable_element() = workspace::mediaElement(mediaPath, workspace);
        workspace::setMediaForeground(media, true);
        settleCue(&presentation, &place);
    }

    return writePresentation(path, presentation);
}

QString ProDocument::insertBlankCue(const QString &path, const QString &cueId, bool after, QString *madeId)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    Place place;
    if (!findPlace(&presentation, cueId, after, &place))
        return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
    *madeId = QString::fromStdString(proconvert::addBlankCue(&presentation, std::string(), newSlideSize(presentation, place.anchor))
                                         ->uuid().string());
    settleCue(&presentation, &place);
    return writePresentation(path, presentation);
}

QString ProDocument::create(const QString &library, const QString &name, QString *made)
{
    if (library.isEmpty() || !QFileInfo(library).isDir())
        return QStringLiteral("There is no library to put it in");
    QString unique;
    const QString path = proconvert::presentationFileFor(library, name, QStringLiteral("New Presentation"), &unique);
    rv::data::Presentation presentation = proconvert::newPresentation(unique);
    // Its one slide, in a group of its own that has an id and no name. (ProPresenter
    // keeps every slide in a group: one that is in none to look at is in one with no
    // name, with an empty hot key beside its id, in 101 of the 103 such groups looked
    // at.)
    auto *group = presentation.add_cue_groups();
    group->mutable_group()->mutable_uuid()->set_string(newUuid());
    group->mutable_group()->mutable_hotkey();
    const rv::data::Cue *cue = proconvert::addBlankCue(&presentation, std::string(), QSizeF(1920, 1080));
    group->add_cue_identifiers()->set_string(cue->uuid().string());
    const QString error = writePresentation(path, presentation);
    if (error.isEmpty())
        *made = path;
    return error;
}

QString ProDocument::rename(const QString &path, const QString &name, QString *renamed)
{
    const QFileInfo was(path);
    const QString wanted = proconvert::presentationName(name);
    if (wanted.isEmpty())
        return QStringLiteral("That is not a name a presentation can have");
    *renamed = path;
    if (wanted == was.completeBaseName())
        return {};
    const QString target = was.dir().filePath(wanted + QStringLiteral(".pro"));
    if (QFileInfo::exists(target))
        return QStringLiteral("There is already a presentation called “%1” in %2").arg(wanted, was.dir().dirName());

    // Read first: a file that cannot be read is not one to be moving about.
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    // The file is given its new name, and then written again in its new place with
    // the name inside changed. Should that second step fail, the file is whole under
    // its new name and says its old one inside, which is still a presentation.
    if (!QFile::rename(path, target))
        return QStringLiteral("%1 could not be renamed").arg(was.fileName());
    *renamed = target;
    presentation.set_name(wanted.toStdString());
    return writePresentation(target, presentation);
}

QString ProDocument::setGroupFrom(const QString &path, const QString &cueId, const QString &name, const QString &color,
                                  const QString &listId, bool *changed)
{
    *changed = false;
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    const groupedit::Outcome outcome = groupedit::setGroupFrom(&presentation, cueId.toStdString(), {name, color, listId},
                                                               [] { return newUuid(); });
    if (outcome == groupedit::Outcome::NoSuchSlide)
        return QStringLiteral("%1 no longer has that slide in a group").arg(QFileInfo(path).fileName());
    if (outcome == groupedit::Outcome::Unchanged)
        return {};
    *changed = true;
    return writePresentation(path, presentation);
}

QByteArray ProDocument::copyCue(const QString &path, const QString &cueId, QString *error)
{
    rv::data::Presentation presentation;
    if (!readPresentation(path, &presentation, error))
        return {};
    for (const rv::data::Cue &cue : presentation.cues()) {
        if (QString::fromStdString(cue.uuid().string()) == cueId)
            return QByteArray::fromStdString(cue.SerializeAsString());
    }
    *error = QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
    return {};
}

QString ProDocument::pasteCue(const QString &path, const QString &cueId, bool after, const QByteArray &copied, QString *madeId)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;
    Place place;
    if (!findPlace(&presentation, cueId, after, &place))
        return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());

    // The copy is a cue of its own: it, its actions, its slide and the elements on the
    // slide each get an id of their own. An id is written the same wherever it is
    // referred to (an element whose text is another's, or that shows only when
    // another has text), so each is replaced wherever it comes up in the cue, which
    // keeps those links pointing inside the copy. What the cue refers to outside
    // itself (its media, a timer) is left as it is.
    rv::data::Cue original;
    if (copied.isEmpty() || !original.ParseFromArray(copied.constData(), int(copied.size())))
        return QStringLiteral("There is no copied slide to paste");
    QList<std::string> ids{original.uuid().string()};
    for (const rv::data::Action &action : original.actions()) {
        ids.append(action.uuid().string());
        if (!action.has_slide() || !action.slide().has_presentation())
            continue;
        const rv::data::Slide &slide = action.slide().presentation().base_slide();
        ids.append(slide.uuid().string());
        for (const rv::data::Slide::Element &element : slide.elements())
            ids.append(element.element().uuid().string());
    }
    QByteArray bytes = copied;
    for (const std::string &id : ids) {
        const std::string fresh = newUuid();
        if (id.size() == fresh.size())
            bytes.replace(QByteArray::fromStdString(id), QByteArray::fromStdString(fresh));
    }
    rv::data::Cue *cue = presentation.add_cues();
    if (!cue->ParseFromArray(bytes.constData(), int(bytes.size()))) {
        return QStringLiteral("The copied slide could not be read back");
    }
    *madeId = QString::fromStdString(cue->uuid().string());
    settleCue(&presentation, &place);
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
