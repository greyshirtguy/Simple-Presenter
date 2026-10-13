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

// A message that is taken off has no time left to run: whatever was counting it is
// told so.
Effect noTime(const QString &id)
{
    Effect effect;
    effect.kind = Effect::MessageTime;
    effect.text = id;
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

QString audioWords(const QVariantMap &audio)
{
    const QString playback = audio.value("playback").toString();
    return (audio.value("effect").toBool() ? QStringLiteral("sound effect ") : QStringLiteral("music "))
         + quoted(audio.value("name").toString())
         + (playback == QLatin1String("loop") ? QStringLiteral(", looping") : playback == QLatin1String("next") ? QStringLiteral(", then the next") : QString());
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

void State::renamed(const QString &from, const QString &to)
{
    if (playlistId.isEmpty() && key == from)
        key = to;
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

void State::clearMessages(Effects &effects)
{
    if (messages.isEmpty())
        return;
    effects << note("clear", QStringLiteral("the messages, %1 of them").arg(messages.size()));
    for (const QString &id : std::as_const(messages))
        effects << noTime(id);
    messages.clear();
}

void State::showMessage(const QVariantMap &message, Effects &effects)
{
    const QString id = message.value("id").toString();
    const double seconds = qMax(0.0, message.value("seconds").toDouble());
    const bool again = messages.contains(id);
    messages.removeAll(id);
    messages << id;
    effects << note("message", quoted(message.value("name").toString()) + (again ? QStringLiteral(" shown again") : QStringLiteral(" shown"))
                                   + (seconds > 0 ? QStringLiteral(", to go by itself after %1 seconds").arg(seconds) : QString())
                                   + QStringLiteral("; %1 shown").arg(messages.size()));
    Effect shown;
    shown.kind = Effect::Message;
    shown.text = id;
    effects << shown;
    Effect time;
    time.kind = Effect::MessageTime;
    time.text = id;
    time.seconds = seconds;
    effects << time;
}

void State::hideMessage(const QString &id, const QString &name, const QString &how, Effects &effects)
{
    if (!messages.contains(id))
        return;
    messages.removeAll(id);
    effects << note("message", quoted(name) + how + QStringLiteral("; %1 shown").arg(messages.size()));
    effects << noTime(id);
}

Effects State::setMessage(const QString &id, bool on, const Workspace &workspace)
{
    Effects effects;
    const QVariantMap message = findMessage(id, QString(), workspace);
    if (!on)
        hideMessage(id, message.value("name").toString(), QStringLiteral(" hidden"), effects);
    else if (!message.isEmpty())
        showMessage(message, effects);
    return effects;
}

Effects State::toggleMessage(const QString &id, const Workspace &workspace)
{
    return setMessage(id, !messages.contains(id), workspace);
}

Effects State::messageTimeUp(const QString &id, const Workspace &workspace)
{
    Effects effects;
    hideMessage(id, findMessage(id, QString(), workspace).value("name").toString(), QStringLiteral(" gone by itself, its time being up"), effects);
    return effects;
}

bool State::dropMissingMessages(const Workspace &workspace)
{
    const qsizetype before = messages.size();
    messages.removeIf([&workspace](const QString &id) { return findMessage(id, QString(), workspace).isEmpty(); });
    return messages.size() != before;
}

Effects State::showStageMessage(const QString &text)
{
    if (text.trimmed().isEmpty())
        return hideStageMessage();
    Effects effects;
    // (How many characters, and not what it says: the log is not for what is said.)
    effects << note("stage", QStringLiteral("a stage message shown, of %1 characters").arg(text.size()));
    stageMessage = text;
    return effects;
}

Effects State::hideStageMessage()
{
    Effects effects;
    if (stageMessage.isEmpty())
        return effects;
    effects << note("stage", QStringLiteral("the stage message hidden"));
    stageMessage.clear();
    return effects;
}

void State::clearMusic(Effects &effects)
{
    if (!hasMusic)
        return;
    effects << note("clear", QStringLiteral("the music, ") + quoted(music.value("name").toString()));
    hasMusic = false;
    music.clear();
    musicPlaylistId.clear();
    effects << plain(Effect::NoMusic);
}

void State::clearSounds(Effects &effects)
{
    if (sounds.isEmpty())
        return;
    effects << note("clear", QStringLiteral("the sound effects, %1 of them").arg(sounds.size()));
    sounds.clear();
    effects << plain(Effect::NoSounds);
}

// Starts a piece of audio. A piece whose file is not on this computer is not started,
// and nothing that is playing gives way to it.
void State::startAudio(const QVariantMap &asked, const QString &playlist, Effects &effects)
{
    if (asked.value("missing").toBool() || asked.value("path").toString().isEmpty()) {
        effects << problem(QStringLiteral("The audio %1 is not on this computer, and was not played").arg(quoted(asked.value("name").toString())));
        return;
    }
    QVariantMap audio = asked;
    audio.insert("play", ++audioPlays);
    if (audio.value("effect").toBool()) {
        sounds.append(audio);
        effects << note("audio", audioWords(audio) + QStringLiteral(", over what is playing; %1 playing").arg(sounds.size()));
    } else {
        effects << note("audio", audioWords(audio) + (hasMusic ? QStringLiteral(", in place of ") + quoted(music.value("name").toString()) : QString()));
        hasMusic = true;
        music = audio;
        musicPlaylistId = playlist;
    }
    effects << withMedia(Effect::Audio, audio);
}

Effects State::playAudio(const QVariantMap &audio, const QString &playlist)
{
    Effects effects;
    startAudio(audio, playlist, effects);
    return effects;
}

Effects State::audioFinished(int play, const QVariantList &rows)
{
    Effects effects;
    for (qsizetype i = 0; i < sounds.size(); ++i) {
        if (sounds.at(i).toMap().value("play").toInt() == play) {
            effects << note("audio", audioWords(sounds.at(i).toMap()) + QStringLiteral(" has played to its end"));
            sounds.removeAt(i);
            return effects;
        }
    }
    if (!hasMusic || music.value("play").toInt() != play)
        return effects;
    const QVariantMap ended = music;
    const QString playlist = musicPlaylistId;
    hasMusic = false;
    music.clear();
    musicPlaylistId.clear();
    // The next of its playlist, for music that is followed by it: the first row after
    // its own that has a file to play.
    QVariantMap following;
    if (ended.value("playback").toString() == QLatin1String("next") && !playlist.isEmpty()) {
        bool found = false;
        for (const QVariant &entry : rows) {
            const QVariantMap row = entry.toMap();
            if (found && !row.value("missing").toBool() && !row.value("path").toString().isEmpty()) {
                following = row;
                break;
            }
            if (row.value("id") == ended.value("id"))
                found = true;
        }
    }
    effects << note("audio", audioWords(ended) + QStringLiteral(" has played to its end")
                                 + (following.isEmpty() && ended.value("playback").toString() == QLatin1String("next")
                                        ? QStringLiteral(", and there is none after it") : QString()));
    if (!following.isEmpty())
        startAudio(following, playlist, effects);
    return effects;
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

Effects State::clearAudio()
{
    Effects effects;
    clearMusic(effects);
    clearSounds(effects);
    return effects;
}

Effects State::clearAudioEffects()
{
    Effects effects;
    clearSounds(effects);
    return effects;
}

Effects State::clearMessages()
{
    Effects effects;
    clearMessages(effects);
    return effects;
}

Effects State::clearAll()
{
    Effects effects;
    effects << note("clear", QStringLiteral("everything asked for"));
    clearSlide(effects);
    clearMedia(effects);
    clearProps(effects);
    clearMessages(effects);
    clearMusic(effects);
    clearSounds(effects);
    return effects;
}

// The layers go in the order everything is cleared in, whatever order the group has
// them in: they are cleared together, and the log reads the same either way.
//
//
// The audio is two of ProPresenter's layers, "audio" and "audio effects". Here the
// first is the whole audio layer, the music and the sound effects, which is what the
// button that clears the audio clears; and the second the sound effects alone, so
// that a group can stop those and leave the music playing. A GUESS: whether
// ProPresenter's own "audio" stops its sound effects too has not been seen.
//
// TO COME: a group names ProPresenter's other layers too (announcements, video
// inputs: see clearfile.h). As the show gains one of them, it is cleared here, for a
// group that has it.
void State::clearGroup(const QVariantMap &group, Effects &effects)
{
    const QStringList layers = group.value("layers").toStringList();
    effects << note("clear", QStringLiteral("the clear group %1 asked for").arg(quoted(group.value("name").toString())));
    if (layers.contains(QLatin1String("slide")))
        clearSlide(effects);
    if (layers.contains(QLatin1String("media")))
        clearMedia(effects);
    if (layers.contains(QLatin1String("props")))
        clearProps(effects);
    if (layers.contains(QLatin1String("messages")))
        clearMessages(effects);
    if (layers.contains(QLatin1String("audio")))
        clearMusic(effects);
    if (layers.contains(QLatin1String("audio")) || layers.contains(QLatin1String("audioEffects")))
        clearSounds(effects);
}

Effects State::clearGroup(const QString &id, const Workspace &workspace)
{
    Effects effects;
    const QVariantMap group = findClearGroup(id, QString(), workspace);
    if (!group.isEmpty())
        clearGroup(group, effects);
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
    const QVariantMap look = findLook(id, QString(), workspace);
    if (look.isEmpty())
        return effects;
    lookId = id;
    Effect made;
    made.kind = Effect::Look;
    made.text = id;
    effects << made << note("look", QStringLiteral("the look %1 is live").arg(quoted(look.value("name").toString())));
    return effects;
}

void State::adoptLook(const QString &id)
{
    lookId = id;
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
            clearMessages(effects);
            clearMusic(effects);
            clearSounds(effects);
        } else if (layer == 7) {
            clearMessages(effects);
        } else if (layer == 1) {
            // (The audio: the music and the sound effects. See clearGroup.)
            clearMusic(effects);
            clearSounds(effects);
        } else if (layer == 8) {
            clearSounds(effects);
        } else if (layer == 2) {
            clearMedia(effects);
        } else if (layer == 4) {
            clearProps(effects);
        } else {
            clearSlide(effects);
        }
    } else if (kind == QLatin1String("audio")) {
        // A piece of audio, as a row of the audio bin is (the action has the same
        // things to say of it). It is played from no playlist, so nothing follows it.
        effects << note("action", title);
        startAudio(action.value("audio").toMap(), QString(), effects);
    } else if (kind == QLatin1String("clearGroup")) {
        const QVariantMap group = findClearGroup(action.value("groupId").toString(), action.value("groupName").toString(), workspace);
        effects << note("action", title + (group.isEmpty() ? QStringLiteral(": there is no such clear group here") : QString()));
        if (!group.isEmpty())
            clearGroup(group, effects);
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
        if (!look.isEmpty()) {
            lookId = look.value("id").toString();
            Effect made;
            made.kind = Effect::Look;
            made.text = lookId;
            effects << made;
        }
    } else if (kind == QLatin1String("prop")) {
        const QVariantMap prop = propOf(action, workspace);
        effects << note("action", title + (prop.isEmpty() ? QStringLiteral(": there is no such prop here") : QString()));
        if (!prop.isEmpty()) {
            const QString id = prop.value("id").toString();
            if (props.contains(id) == action.value("clear").toBool())
                toggleProp(id, workspace, effects);
        }
    } else if (kind == QLatin1String("message")) {
        // (Shown with what its tokens were last given here. An action of ProPresenter's
        // can carry what to give them, which is not read: see actions.cpp.)
        const QVariantMap message = findMessage(action.value("messageId").toString(), action.value("messageName").toString(), workspace);
        effects << note("action", title + (message.isEmpty() ? QStringLiteral(": there is no such message here") : QString()));
        if (!message.isEmpty())
            showMessage(message, effects);
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

QVariantMap findMessage(const QString &id, const QString &name, const Workspace &workspace)
{
    QVariantMap byName;
    for (const QVariant &entry : workspace.messages) {
        const QVariantMap message = entry.toMap();
        if (!id.isEmpty() && message.value("id").toString() == id)
            return message;
        if (byName.isEmpty() && !name.isEmpty() && message.value("name").toString() == name)
            byName = message;
    }
    return byName;
}

QVariantMap findClearGroup(const QString &id, const QString &name, const Workspace &workspace)
{
    QVariantMap byName;
    for (const QVariant &entry : workspace.clearGroups) {
        const QVariantMap group = entry.toMap();
        if (!id.isEmpty() && group.value("id").toString() == id)
            return group;
        if (byName.isEmpty() && !name.isEmpty() && group.value("name").toString() == name)
            byName = group;
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
