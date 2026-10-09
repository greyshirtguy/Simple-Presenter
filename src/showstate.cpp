#include "showstate.h"

namespace show {

namespace {

Effect note(const char *topic, const QString &text)
{
    Effect effect;
    effect.kind = Effect::Note;
    effect.topic = QString::fromLatin1(topic);
    effect.text = text;
    return effect;
}

Effect problem(const QString &text)
{
    Effect effect;
    effect.kind = Effect::Problem;
    effect.text = text;
    return effect;
}

Effect plain(Effect::Kind kind)
{
    Effect effect;
    effect.kind = kind;
    return effect;
}

Effect withMedia(Effect::Kind kind, const QVariantMap &media)
{
    Effect effect;
    effect.kind = kind;
    effect.media = media;
    return effect;
}

QString actionsWord(qsizetype count)
{
    return QString::number(count) + (count == 1 ? QStringLiteral(" action") : QStringLiteral(" actions"));
}

}

QString quoted(const QString &name)
{
    return u'"' + name + u'"';
}

QString mediaWords(const QVariantMap &media)
{
    const bool video = media.value("video").toBool();
    return (media.value("foreground").toBool() ? QStringLiteral("foreground ") : QStringLiteral("background "))
         + (video ? QStringLiteral("video ") : QStringLiteral("picture ")) + quoted(media.value("name").toString())
         + (video && media.value("loops").toBool() ? QStringLiteral(", looping") : QString());
}

bool State::at(const QString &key, const QString &playlistId) const
{
    return atSlide && this->key == key && this->playlistId == playlistId;
}

Effects State::goLive(const Cue &cue, bool withoutMedia, const Workspace &workspace)
{
    Effects effects;
    if (cue.index < 0 || cue.index >= cue.count)
        return effects;
    const QVariantMap cueMedia = cue.slide.value("media").toMap();
    const QString mediaName = cue.slide.value("mediaName").toString();
    const bool brings = !withoutMedia && !cueMedia.isEmpty();
    const QVariantList actions = cue.slide.value("actions").toList();
    const QString label = cue.slide.value("label").toString();

    atSlide = true;
    key = cue.key;
    playlistId = cue.playlistId;
    presentation = cue.presentation;
    index = cue.index;
    count = cue.count;
    slide = cue.slide;
    next = cue.next;
    cleared = false;
    clearedByCue = false;

    const bool playing = brings && alreadyPlaying(cueMedia);
    QString said = QStringLiteral("slide %1 of %2 of %3").arg(cue.index + 1).arg(cue.count).arg(quoted(cue.presentation));
    if (!label.isEmpty())
        said += QStringLiteral(" (") + label + u')';
    if (withoutMedia && !mediaName.isEmpty())
        said += QStringLiteral("; without its media, as asked");
    else if (brings)
        said += playing ? QStringLiteral("; its ") + mediaWords(cueMedia) + QStringLiteral(" is playing already")
                        : QStringLiteral("; with its ") + mediaWords(cueMedia);
    else if (!mediaName.isEmpty())
        said += QStringLiteral("; its media ") + quoted(mediaName) + QStringLiteral(" was not found");
    else if (hasMedia && media.value("foreground").toBool())
        said += QStringLiteral("; which takes off the foreground media");
    if (!actions.isEmpty())
        said += QStringLiteral("; and has ") + actionsWord(actions.size());
    effects << note("live", said);
    if (!withoutMedia && cueMedia.isEmpty() && !mediaName.isEmpty())
        effects << problem(QStringLiteral("The media %1 of slide %2 of %3 was not found in the workspace, so the slide is shown without it")
                               .arg(quoted(mediaName)).arg(cue.index + 1).arg(quoted(cue.presentation)));

    if (!brings) {
        // A foreground is for the moment it was triggered in: a slide that brings no
        // media of its own ends it. A background plays on.
        if (hasMedia && media.value("foreground").toBool())
            clearMedia(effects);
        effects << plain(Effect::Slide);
    } else if (playing) {
        effects << plain(Effect::SlideOverMedia);
    } else {
        hasMedia = true;
        media = cueMedia;
        mediaPlaylistId.clear();
        effects << withMedia(Effect::SlideWithMedia, cueMedia);
    }

    // What else the slide's cue does, such as starting the countdown it shows: the
    // slide first, and then its actions, in their order. So an action that clears the
    // slide clears this one, which is how a cue is made that shows nothing of its own;
    // it is then still the slide the show is at (see clearedByCue).
    m_runningCue = true;
    runActions(actions, 0, workspace, effects);
    m_runningCue = false;
    return effects;
}

int State::stepTarget(const QString &key, const QString &playlistId, int delta) const
{
    if (!at(key, playlistId))
        return 0;
    if (cleared && !clearedByCue)
        return index;
    return index + delta;
}

void State::follow(const Cue &cue)
{
    if (!atSlide)
        return;
    presentation = cue.presentation;
    index = cue.index;
    count = cue.count;
    slide = cue.slide;
    next = cue.next;
}

void State::leavePresentation()
{
    atSlide = false;
    key.clear();
    playlistId.clear();
    presentation.clear();
    index = -1;
    count = 0;
    slide.clear();
    next.clear();
    clearedByCue = false;
}

bool State::alreadyPlaying(const QVariantMap &asked) const
{
    if (!hasMedia || media.value("foreground").toBool() || asked.value("foreground").toBool() || asked.value("retriggers").toBool()
        || asked.value("path") != media.value("path"))
        return false;
    if (!asked.value("video").toBool())
        return true;
    // (The count and the time only matter for the way of playing that uses them.)
    const int playback = asked.value("playback").toInt();
    return media.value("loops").toBool() && playback == media.value("playback").toInt()
        && (playback != 2 || asked.value("loopCount").toInt() == media.value("loopCount").toInt())
        && (playback != 3 || asked.value("loopSeconds").toDouble() == media.value("loopSeconds").toDouble());
}

Effects State::showMedia(const QVariantMap &asked, const QString &playlist)
{
    Effects effects;
    const bool playing = alreadyPlaying(asked);
    effects << note("media", mediaWords(asked) + (playing ? QStringLiteral(", which is playing already and is left to") : QString()));
    hasMedia = true;
    media = asked;
    mediaPlaylistId = playlist;
    if (!playing)
        effects << withMedia(Effect::Media, asked);
    return effects;
}

void State::clearSlide(Effects &effects)
{
    if (cleared)
        return;
    effects << note("clear", QStringLiteral("the slide") + (m_runningCue ? QStringLiteral(", by its own action") : QString()));
    cleared = true;
    clearedByCue = m_runningCue;
    effects << plain(Effect::NoSlide);
}

void State::clearMedia(Effects &effects)
{
    if (!hasMedia)
        return;
    effects << note("clear", QStringLiteral("the media, ") + quoted(media.value("name").toString()));
    hasMedia = false;
    media.clear();
    mediaPlaylistId.clear();
    effects << plain(Effect::NoMedia);
}

void State::clearProps(Effects &effects)
{
    if (props.isEmpty())
        return;
    effects << note("clear", QStringLiteral("the props, %1 of them").arg(props.size()));
    props.clear();
}

Effects State::clearSlide()
{
    Effects effects;
    clearSlide(effects);
    return effects;
}

Effects State::clearMedia()
{
    Effects effects;
    clearMedia(effects);
    return effects;
}

Effects State::clearProps()
{
    Effects effects;
    clearProps(effects);
    return effects;
}

Effects State::clearAll()
{
    Effects effects;
    effects << note("clear", QStringLiteral("everything asked for"));
    clearSlide(effects);
    clearMedia(effects);
    clearProps(effects);
    return effects;
}

void State::toggleProp(const QString &id, const Workspace &workspace, Effects &effects)
{
    const QVariantMap prop = findProp(id, workspace);
    if (props.contains(id)) {
        props.removeAll(id);
        effects << note("prop", QStringLiteral("%1 off; %2 on").arg(quoted(prop.value("name").toString())).arg(props.size()));
        return;
    }
    if (prop.isEmpty())
        return;
    // The others of a collection that shows one at a time give way.
    QStringList rivals;
    if (prop.value("single").toBool()) {
        for (const QVariant &entry : workspace.props) {
            const QVariantMap collection = entry.toMap();
            if (collection.value("id") != prop.value("collection"))
                continue;
            const QVariantList others = collection.value("props").toList();
            for (const QVariant &other : others)
                rivals << other.toMap().value("id").toString();
        }
    }
    const qsizetype before = props.size();
    for (const QString &rival : std::as_const(rivals))
        props.removeAll(rival);
    props << id;
    effects << note("prop", quoted(prop.value("name").toString()) + QStringLiteral(" on")
                                + (props.size() <= before ? QStringLiteral(", in place of another of its collection") : QString())
                                + QStringLiteral("; %1 on").arg(props.size()));
}

Effects State::toggleProp(const QString &id, const Workspace &workspace)
{
    Effects effects;
    toggleProp(id, workspace, effects);
    return effects;
}

Effects State::setProp(const QString &id, bool on, const Workspace &workspace)
{
    Effects effects;
    if (props.contains(id) != on)
        toggleProp(id, workspace, effects);
    return effects;
}

bool State::dropMissingProps(const Workspace &workspace)
{
    const qsizetype before = props.size();
    props.removeIf([&workspace](const QString &id) { return findProp(id, workspace).isEmpty(); });
    return props.size() != before;
}

void State::setStageLayout(const QString &screenId, const QString &layoutId)
{
    if (layoutId.isEmpty())
        stageLayouts.remove(screenId);
    else
        stageLayouts.insert(screenId, layoutId);
}

Effects State::setLook(const QString &id, const Workspace &workspace)
{
    Effects effects;
    if (id == lookId)
        return effects;
    const QVariantMap look = findLook(id, QString(), workspace);
    if (!id.isEmpty() && look.isEmpty())
        return effects;
    lookId = id;
    effects << note("look", id.isEmpty() ? QStringLiteral("no look: every screen gets everything")
                                         : QStringLiteral("the look %1 is live").arg(quoted(look.value("name").toString())));
    return effects;
}

Effects State::runMacro(const QString &id, const Workspace &workspace)
{
    Effects effects;
    const QVariantMap macro = findMacro(id, QString(), workspace);
    if (macro.isEmpty())
        return effects;
    const QVariantList actions = macro.value("actions").toList();
    effects << note("macro", quoted(macro.value("name").toString()) + QStringLiteral(" run by hand: ") + actionsWord(actions.size()));
    runActions(actions, 1, workspace, effects);
    return effects;
}

// Runs a list of actions, in order: a slide's, as it goes live, or a macro's. `depth` is
// how many macros deep this is, a macro being able to run a macro.
void State::runActions(const QVariantList &actions, int depth, const Workspace &workspace, Effects &effects)
{
    for (const QVariant &action : actions)
        runAction(action.toMap(), depth, workspace, effects);
}

void State::runAction(const QVariantMap &action, int depth, const Workspace &workspace, Effects &effects)
{
    const QString kind = action.value("kind").toString();
    const QString title = action.value("title").toString();
    if (kind == QLatin1String("timer")) {
        Effect effect;
        effect.kind = Effect::Timer;
        effect.action = action;
        effects << effect;
    } else if (kind == QLatin1String("clear")) {
        // The layers there are here to clear; the others are ProPresenter's.
        const bool done = action.value("done").toBool();
        effects << note("action", title + (done ? QString() : QStringLiteral(": not done here")));
        if (!done)
            return;
        const int layer = action.value("layer").toInt();
        if (layer == 0) {
            effects << note("clear", QStringLiteral("everything asked for"));
            clearSlide(effects);
            clearMedia(effects);
            clearProps(effects);
        } else if (layer == 2) {
            clearMedia(effects);
        } else if (layer == 4) {
            clearProps(effects);
        } else {
            clearSlide(effects);
        }
    } else if (kind == QLatin1String("stage")) {
        const QMap<QString, QVariantMap> layouts = stageLayoutsOf(action, workspace);
        bool any = false;
        for (auto given = layouts.constBegin(); given != layouts.constEnd(); ++given) {
            if (given.value().isEmpty())
                continue;
            any = true;
            stageLayouts.insert(given.key(), given.value().value("id").toString());
        }
        effects << note("action", title + (any ? QString() : QStringLiteral(": nothing to change here")));
    } else if (kind == QLatin1String("look")) {
        const QVariantMap look = findLook(action.value("lookId").toString(), action.value("lookName").toString(), workspace);
        effects << note("action", title + (look.isEmpty() ? QStringLiteral(": there is no such look here") : QString()));
        if (!look.isEmpty())
            lookId = look.value("id").toString();
    } else if (kind == QLatin1String("prop")) {
        const QVariantMap prop = propOf(action, workspace);
        effects << note("action", title + (prop.isEmpty() ? QStringLiteral(": there is no such prop here") : QString()));
        if (!prop.isEmpty()) {
            const QString id = prop.value("id").toString();
            if (props.contains(id) == action.value("clear").toBool())
                toggleProp(id, workspace, effects);
        }
    } else if (kind == QLatin1String("macro")) {
        const QVariantMap macro = findMacro(action.value("macroId").toString(), action.value("macroName").toString(), workspace);
        if (macro.isEmpty()) {
            effects << note("action", title + QStringLiteral(": there is no such macro here"));
        } else if (depth >= 8) {
            effects << problem(QStringLiteral("The macro %1 was not run again: macros that run each other have gone round eight times")
                                   .arg(quoted(macro.value("name").toString())));
        } else {
            const QVariantList actions = macro.value("actions").toList();
            effects << note("action", title + QStringLiteral(", which has ") + actionsWord(actions.size()));
            runActions(actions, depth + 1, workspace, effects);
        }
    } else {
        effects << note("action", title + QStringLiteral(": not done here"));
    }
}

QVariantMap findProp(const QString &id, const Workspace &workspace)
{
    for (const QVariant &entry : workspace.props) {
        const QVariantMap collection = entry.toMap();
        const QVariantList props = collection.value("props").toList();
        for (const QVariant &candidate : props) {
            QVariantMap prop = candidate.toMap();
            if (prop.value("id").toString() == id) {
                prop.insert("collection", collection.value("id"));
                prop.insert("single", collection.value("single"));
                return prop;
            }
        }
    }
    return {};
}

QVariantMap propOf(const QVariantMap &action, const Workspace &workspace)
{
    const QString id = action.value("propId").toString();
    const QString name = action.value("propName").toString();
    QVariantMap named;
    for (const QVariant &entry : workspace.props) {
        const QVariantList props = entry.toMap().value("props").toList();
        for (const QVariant &candidate : props) {
            const QVariantMap prop = candidate.toMap();
            if (prop.value("id").toString() == id)
                return prop;
            if (named.isEmpty() && prop.value("name").toString() == name)
                named = prop;
        }
    }
    return named;
}

QVariantMap findMacro(const QString &id, const QString &name, const Workspace &workspace)
{
    QVariantMap byName;
    for (const QVariant &entry : workspace.macros) {
        const QVariantMap collection = entry.toMap();
        const QVariantList macros = collection.value("macros").toList();
        for (const QVariant &candidate : macros) {
            QVariantMap macro = candidate.toMap();
            const bool byId = !id.isEmpty() && macro.value("id").toString() == id;
            const bool named = byName.isEmpty() && !name.isEmpty() && macro.value("name").toString() == name;
            if (!byId && !named)
                continue;
            macro.insert("collection", collection.value("id"));
            macro.insert("collectionName", collection.value("name"));
            if (byId)
                return macro;
            byName = macro;
        }
    }
    return byName;
}

QVariantMap findLook(const QString &id, const QString &name, const Workspace &workspace)
{
    QVariantMap byName;
    for (const QVariant &entry : workspace.looks) {
        const QVariantMap look = entry.toMap();
        if (!id.isEmpty() && look.value("id").toString() == id)
            return look;
        if (byName.isEmpty() && !name.isEmpty() && look.value("name").toString() == name)
            byName = look;
    }
    return byName;
}

QVariantList stageScreens(const Workspace &workspace)
{
    if (!workspace.stageScreens.isEmpty())
        return workspace.stageScreens;
    return {QVariantMap {{"id", QString()}, {"name", QStringLiteral("Stage")}}};
}

QVariantMap stageScreen(const Workspace &workspace)
{
    return stageScreens(workspace).first().toMap();
}

int stageAssignmentFor(const QVariantMap &action, const QVariantMap &screen)
{
    const QString screenId = screen.value("id").toString();
    const QString screenName = screen.value("name").toString();
    const QVariantList list = action.value("assignments").toList();
    int byName = -1;
    for (int i = 0; i < list.size(); ++i) {
        const QVariantMap assignment = list.at(i).toMap();
        if (!screenId.isEmpty() && assignment.value("screenId").toString() == screenId)
            return i;
        if (byName < 0 && assignment.value("screenName").toString() == screenName)
            byName = i;
    }
    return byName;
}

namespace {

// Whether an action names any of the workspace's stage screens
bool namesAScreen(const QVariantMap &action, const Workspace &workspace)
{
    const QVariantList screens = stageScreens(workspace);
    for (const QVariant &screen : screens) {
        if (stageAssignmentFor(action, screen.toMap()) >= 0)
            return true;
    }
    return false;
}

int firstWithLayout(const QVariantMap &action)
{
    const QVariantList list = action.value("assignments").toList();
    for (int i = 0; i < list.size(); ++i) {
        const QVariantMap assignment = list.at(i).toMap();
        if (!assignment.value("layoutId").toString().isEmpty() || !assignment.value("layoutName").toString().isEmpty())
            return i;
    }
    return -1;
}

// The layout a line of a stage action gives: empty for none, or for one that is not
// among the workspace's.
QVariantMap layoutOfLine(const QVariantMap &action, int at, const Workspace &workspace)
{
    if (at < 0)
        return {};
    const QVariantMap assignment = action.value("assignments").toList().value(at).toMap();
    const QString id = assignment.value("layoutId").toString();
    const QString name = assignment.value("layoutName").toString();
    if (id.isEmpty() && name.isEmpty())
        return {};
    QVariantMap named;
    for (const QVariant &entry : workspace.stageLayouts) {
        const QVariantMap layout = entry.toMap();
        if (layout.value("id").toString() == id)
            return layout;
        if (named.isEmpty() && layout.value("name").toString() == name)
            named = layout;
    }
    return named;
}

}

int stageAssignmentOf(const QVariantMap &action, const Workspace &workspace)
{
    if (namesAScreen(action, workspace))
        return stageAssignmentFor(action, stageScreen(workspace));
    return firstWithLayout(action);
}

QMap<QString, QVariantMap> stageLayoutsOf(const QVariantMap &action, const Workspace &workspace)
{
    QMap<QString, QVariantMap> layouts;
    const QVariantList screens = stageScreens(workspace);
    const bool named = namesAScreen(action, workspace);
    for (int i = 0; i < screens.size(); ++i) {
        const QVariantMap screen = screens.at(i).toMap();
        const int at = named ? stageAssignmentFor(action, screen) : i == 0 ? firstWithLayout(action) : -1;
        layouts.insert(screen.value("id").toString(), layoutOfLine(action, at, workspace));
    }
    return layouts;
}

QVariantMap stageLayoutOf(const QVariantMap &action, const Workspace &workspace)
{
    return layoutOfLine(action, stageAssignmentOf(action, workspace), workspace);
}

QVariantList stageAssignments(const QVariantMap &existing, const QMap<QString, QVariantMap> &layouts, const Workspace &workspace)
{
    const QVariantList screens = stageScreens(workspace);
    QVariantList list = existing.value("assignments").toList();
    const bool changing = !list.isEmpty();
    const bool named = changing && namesAScreen(existing, workspace);
    for (int i = 0; i < screens.size(); ++i) {
        const QVariantMap screen = screens.at(i).toMap();
        const QString screenId = screen.value("id").toString();
        const bool given = layouts.contains(screenId);
        const QVariantMap layout = layouts.value(screenId);
        // The line that is this screen's, if the action has one
        int at = !changing ? -1 : named ? stageAssignmentFor(existing, screen) : i == 0 ? qMax(0, firstWithLayout(existing)) : -1;
        if (at < 0) {
            // A new action names every screen; one being changed gets a line only for a
            // screen that is now given a layout.
            if (changing && (!given || layout.isEmpty()))
                continue;
            list << QVariantMap {{"screenId", screenId}, {"screenName", screen.value("name").toString()},
                                 {"layoutId", QString()}, {"layoutName", QString()}};
            at = int(list.size()) - 1;
        }
        if (!given)
            continue;
        QVariantMap mine = list.at(at).toMap();
        mine.insert("layoutId", layout.value("id").toString());
        mine.insert("layoutName", layout.value("name").toString());
        list[at] = mine;
    }
    return list;
}

QVariantList stageAssignments(const QVariantMap &existing, const QVariantMap &layout, const Workspace &workspace)
{
    return stageAssignments(existing, QMap<QString, QVariantMap> {{stageScreen(workspace).value("id").toString(), layout}}, workspace);
}

int placeAfterReload(const QStringList &before, int index, const QStringList &after)
{
    if (index < 0 || index >= before.size())
        return -1;
    const QString &id = before.at(index);
    // The slide's place among those of its id, counting from one
    qsizetype place = 0;
    for (int i = 0; i <= index; ++i)
        place += before.at(i) == id ? 1 : 0;
    qsizetype passed = 0;
    for (int i = 0; i < after.size(); ++i) {
        if (after.at(i) == id && ++passed == place)
            return i;
    }
    return -1;
}

}
