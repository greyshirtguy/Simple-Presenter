#include "showstate.h"

#include "ccli.h"

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

// A going on by itself to be started or stopped, at the show's own place or the
// announcements' (see State::advance)
Effect advanceEffect(Effect::Kind kind, bool announcements, double seconds = 0)
{
    Effect effect;
    effect.kind = kind;
    effect.text = announcements ? QStringLiteral("announcements") : QString();
    effect.seconds = seconds;
    return effect;
}

// What a slide asks for when it goes live: its own Go To Next Timer, or failing that
// its presentation's Slide Show, or nothing.
State::Advance advanceOf(const Cue &cue)
{
    State::Advance wanted;
    const QVariantMap own = cue.slide.value("goToNext").toMap();
    if (!own.isEmpty()) {
        wanted.waiting = true;
        wanted.seconds = qMax(0.0, own.value("seconds").toDouble());
        wanted.toFirst = own.value("loop").toBool();
        // From the last slide there is no next.
        if (!wanted.toFirst && cue.index + 1 >= cue.count)
            return {};
    } else if (cue.slideShow > 0) {
        wanted.waiting = true;
        wanted.seconds = cue.slideShow;
        wanted.loops = true;
    }
    return wanted;
}

// A number of seconds as it is said: "5 s", "2.5 s"
QString secondsWords(double seconds)
{
    return QString::number(seconds, 'g', 4) + QStringLiteral(" s");
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

bool State::announcementAt(const QString &key, const QString &playlistId) const
{
    return announcement.atSlide && announcement.key == key && announcement.playlistId == playlistId;
}

namespace {

// Where the markers of the media a cue's slide brings are kept: the slide of its
// presentation's file (see Catalog.addMarker's places)
QVariantMap slidePlace(const Cue &cue)
{
    const QString id = cue.slide.value("id").toString();
    if (cue.file.isEmpty() || id.isEmpty())
        return {};
    return {{"kind", QStringLiteral("slide")}, {"path", cue.file}, {"slideId", id}};
}

// The media of a layer with `markers` in place of its own (none taken away)
void putMarkers(QVariantMap *media, const QVariantList &markers)
{
    if (markers.isEmpty())
        media->remove("markers");
    else
        media->insert("markers", markers);
}

// What of a piece of media's Media Inspector settings (as mediasettings::describe has
// them) changes how it looks or plays. The same file in two places can differ in the
// rest, the moment its thumbnail is taken from or the size and length its file says it
// has, and still be the same media playing. Read as plain values, so that media with no
// settings at all is media set to nothing.
QVariantMap lookOf(const QVariantMap &settings)
{
    QVariantMap look {
        {"scale", settings.value("scale").toInt()},
        {"blurred", settings.value("blurred").toBool()},
        {"rotation", settings.value("rotation").toInt()},
        {"flipH", settings.value("flipH").toBool()},
        {"flipV", settings.value("flipV").toBool()},
        {"inPoint", settings.value("inPoint").toDouble()},
        {"outPoint", settings.value("trimmedOut").toBool() ? settings.value("outPoint").toDouble() : 0.0},
    };
    if (settings.value("cropOn").toBool()) {
        for (const char *edge : {"cropLeft", "cropRight", "cropTop", "cropBottom"})
            look.insert(edge, settings.value(edge).toDouble());
    }
    QVariantList effects;
    for (const QVariant &effect : settings.value("effects").toList()) {
        const QVariantMap map = effect.toMap();
        if (map.value("on").toBool())
            effects.append(QVariantList {map.value("renderId"), map.value("numbers"), map.value("colors")});
    }
    look.insert("effects", effects);
    return look;
}

// Whether media that is asked for is a background that is the one playing on a layer
// already, and so is left to play on: see State::alreadyPlaying, which says when.
bool playsOn(const QVariantMap &asked, bool has, const QVariantMap &playing)
{
    // (The same file set to look or play otherwise, in its Media Inspector, is other
    // media: what is asked for is shown as it is set.)
    if (!has || playing.value("foreground").toBool() || asked.value("foreground").toBool() || asked.value("retriggers").toBool()
        || asked.value("path") != playing.value("path")
        || lookOf(asked.value("settings").toMap()) != lookOf(playing.value("settings").toMap()))
        return false;
    if (!asked.value("video").toBool())
        return true;
    // (The count and the time only matter for the way of playing that uses them.)
    const int playback = asked.value("playback").toInt();
    return playing.value("loops").toBool() && playback == playing.value("playback").toInt()
        && (playback != 2 || asked.value("loopCount").toInt() == playing.value("loopCount").toInt())
        && (playback != 3 || asked.value("loopSeconds").toDouble() == playing.value("loopSeconds").toDouble());
}

} // namespace

// ---- going on by itself (see `advance` in the header)

void State::arm(bool announcements, const Cue &cue, bool byTimer, Effects &effects)
{
    Advance &waiting = announcements ? announcementAdvance : advance;
    int &run = announcements ? m_announcementRun : m_run;
    Advance wanted = advanceOf(cue);
    if (!byTimer)
        run = 0;
    if (wanted.waiting && wanted.seconds <= 0) {
        // Round the whole presentation with no time passing: it would never stop.
        if (++run > qMax(1, cue.count)) {
            effects << problem(QStringLiteral("%1 goes from slide to slide with no time between and would never stop, so it has been "
                                              "stopped at slide %2").arg(quoted(cue.presentation)).arg(cue.index + 1));
            wanted = Advance();
            run = 0;
        }
    } else {
        run = 0;
    }
    if (wanted.waiting) {
        waiting = wanted;
        effects << note("next", QStringLiteral("%1 on to %2 in %3, by %4")
                                    .arg(announcements ? QStringLiteral("the announcements go") : QStringLiteral("the show goes"),
                                         wanted.toFirst ? QStringLiteral("the first slide") : QStringLiteral("the next slide"),
                                         secondsWords(wanted.seconds),
                                         wanted.loops ? QStringLiteral("the presentation's Slide Show")
                                                      : QStringLiteral("the slide's Go To Next Timer")));
        effects << advanceEffect(Effect::Advance, announcements, wanted.seconds);
    } else if (waiting.waiting) {
        waiting = Advance();
        effects << advanceEffect(Effect::NoAdvance, announcements);
    }
}

void State::stopAdvance(bool announcements, Effects &effects)
{
    Advance &waiting = announcements ? announcementAdvance : advance;
    (announcements ? m_announcementRun : m_run) = 0;
    if (!waiting.waiting)
        return;
    waiting = Advance();
    effects << note("next", announcements ? QStringLiteral("the announcements no longer go on by themselves")
                                          : QStringLiteral("the show no longer goes on by itself"));
    effects << advanceEffect(Effect::NoAdvance, announcements);
}

int State::takeAdvance(bool announcements)
{
    Advance &waiting = announcements ? announcementAdvance : advance;
    if (!waiting.waiting)
        return -1;
    const Advance was = waiting;
    waiting = Advance();
    const bool there = announcements ? announcement.atSlide : atSlide;
    const int from = announcements ? announcement.index : index;
    const int slides = announcements ? announcement.count : count;
    const QList<int> &disabled = announcements ? announcement.disabled : this->disabled;
    if (!there || slides <= 0)
        return -1;
    // (A disabled slide is passed over: to the next that is not, and round to the first
    // that is not for a presentation that goes round.)
    if (was.toFirst)
        return enabledFrom(0, 1, slides, disabled);
    const int next = enabledFrom(from + 1, 1, slides, disabled);
    if (next >= 0)
        return next;
    return was.loops ? enabledFrom(0, 1, slides, disabled) : -1;
}

// A slide of a presentation that is sent to the announcements goes live there: the
// rules of goLive(), on the announcements layer's own slide and media. Its actions are
// done as any slide's are, and do what they say: one that clears the slide clears the
// slide layer, and not this.
Effects State::goLiveAnnouncement(const Cue &cue, bool withoutMedia, const Workspace &workspace, bool byTimer)
{
    Effects effects;
    const QVariantMap cueMedia = cue.slide.value("media").toMap();
    const QString mediaName = cue.slide.value("mediaName").toString();
    const bool brings = !withoutMedia && !cueMedia.isEmpty();
    const QVariantList actions = cue.slide.value("actions").toList();
    const QString label = cue.slide.value("label").toString();

    announcement.atSlide = true;
    announcement.key = cue.key;
    announcement.playlistId = cue.playlistId;
    announcement.presentation = cue.presentation;
    announcement.index = cue.index;
    announcement.count = cue.count;
    announcement.disabled = cue.disabled;
    announcement.slide = cue.slide;
    announcement.next = cue.next;
    announcement.cleared = false;
    announcement.clearedByCue = false;

    const bool playing = brings && playsOn(cueMedia, announcementHasMedia, announcementMedia);
    QString said = QStringLiteral("to the announcements, slide %1 of %2 of %3").arg(cue.index + 1).arg(cue.count).arg(quoted(cue.presentation));
    if (!label.isEmpty())
        said += QStringLiteral(" (") + label + u')';
    if (withoutMedia && !mediaName.isEmpty())
        said += QStringLiteral("; without its media, as asked");
    else if (brings)
        said += playing ? QStringLiteral("; its ") + mediaWords(cueMedia) + QStringLiteral(" is playing there already")
                        : QStringLiteral("; with its ") + mediaWords(cueMedia);
    else if (!mediaName.isEmpty())
        said += QStringLiteral("; its media ") + quoted(mediaName) + QStringLiteral(" was not found");
    if (!actions.isEmpty())
        said += QStringLiteral("; and has ") + actionsWord(actions.size());
    effects << note("live", said);
    if (!withoutMedia && cueMedia.isEmpty() && !mediaName.isEmpty())
        effects << problem(QStringLiteral("The media %1 of slide %2 of %3 was not found in the workspace, so the slide is shown without it")
                               .arg(quoted(mediaName)).arg(cue.index + 1).arg(quoted(cue.presentation)));

    if (!brings) {
        if (announcementHasMedia && announcementMedia.value("foreground").toBool()) {
            // The foreground that was under the slide before goes with it: the layer
            // is given the slide alone, which takes the media off.
            announcementHasMedia = false;
            announcementMedia.clear();
            announcementMediaPlace.clear();
            effects << withMedia(Effect::AnnouncementWithMedia, QVariantMap());
        } else {
            effects << plain(Effect::Announcement);
        }
    } else if (playing) {
        effects << plain(Effect::AnnouncementOverMedia);
    } else {
        announcementHasMedia = true;
        announcementMedia = cueMedia;
        announcementMediaPlace = slidePlace(cue);
        effects << withMedia(Effect::AnnouncementWithMedia, cueMedia);
    }

    m_runningAnnouncement = true;
    runActions(actions, 0, workspace, effects);
    m_runningAnnouncement = false;
    arm(true, cue, byTimer, effects);
    return effects;
}

Effects State::goLive(const Cue &cue, bool withoutMedia, const Workspace &workspace, bool byTimer)
{
    Effects effects;
    if (cue.index < 0 || cue.index >= cue.count)
        return effects;
    if (cue.destination == QLatin1String("announcements"))
        return goLiveAnnouncement(cue, withoutMedia, workspace, byTimer);
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
    disabled = cue.disabled;
    slide = cue.slide;
    next = cue.next;
    cleared = false;
    clearedByCue = false;
    buildStep = 0;
    // The copyright line over it, where the display is on, the presentation shows its
    // details, and this is one of the slides they are shown on
    const QVariantMap &display = workspace.copyright;
    copyright = display.value("enabled").toBool() && cue.ccli.value("display").toBool()
                        && ccli::shownOn(display.value("display").toString(), cue.index, cue.count)
                    ? ccli::compose(display.value("tokens").toList(), cue.ccli, display.value("license").toString())
                    : QString();

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
        mediaPlace = slidePlace(cue);
        effects << withMedia(Effect::SlideWithMedia, cueMedia);
    }

    // What else the slide's cue does, such as starting the countdown it shows: the
    // slide first, and then its actions, in their order. So an action that clears the
    // slide clears this one, which is how a cue is made that shows nothing of its own;
    // it is then still the slide the show is at (see clearedByCue).
    m_runningCue = true;
    runActions(actions, 0, workspace, effects);
    m_runningCue = false;
    arm(false, cue, byTimer, effects);
    return effects;
}

int State::enabledFrom(int from, int step, int count, const QList<int> &disabled)
{
    for (int place = from; place >= 0 && (count < 0 || place < count); place += step) {
        if (!disabled.contains(place))
            return place;
        if (step == 0)
            break;
    }
    return -1;
}

int State::stepTarget(const QString &key, const QString &playlistId, int delta, bool announcements, const QList<int> &disabled,
                      int count) const
{
    const int way = delta < 0 ? -1 : 1;
    const bool here = announcements ? announcementAt(key, playlistId) : at(key, playlistId);
    // From another presentation, its first slide that is not disabled
    if (!here)
        return enabledFrom(0, 1, count, disabled);
    const bool back = announcements ? announcement.cleared && !announcement.clearedByCue : cleared && !clearedByCue;
    const int from = announcements ? announcement.index : index;
    if (back)
        return from;
    const int target = from + delta;
    if (target < 0 || (count >= 0 && target >= count))
        return target;
    const int found = enabledFrom(target, way, count, disabled);
    return found;
}

Effects State::stepBuild(const QString &key, const QString &playlistId, int index, bool *ran)
{
    Effects effects;
    *ran = false;
    const int steps = slide.value("buildSteps").toInt();
    if (!at(key, playlistId) || cleared || (index >= 0 && index != this->index) || buildStep >= steps)
        return effects;
    ++buildStep;
    *ran = true;
    effects << note("live", QStringLiteral("build %1 of %2 of slide %3 of %4").arg(buildStep).arg(steps).arg(this->index + 1)
                                .arg(quoted(presentation)));
    effects << plain(Effect::Build);
    return effects;
}

Effects State::follow(const Cue &cue)
{
    Effects effects;
    const bool announcements = cue.destination == QLatin1String("announcements");
    if (announcements) {
        if (!announcement.atSlide)
            return effects;
        announcement.presentation = cue.presentation;
        announcement.index = cue.index;
        announcement.count = cue.count;
        announcement.disabled = cue.disabled;
        announcement.slide = cue.slide;
        announcement.next = cue.next;
    } else {
        if (!atSlide)
            return effects;
        presentation = cue.presentation;
        index = cue.index;
        count = cue.count;
        disabled = cue.disabled;
        slide = cue.slide;
        next = cue.next;
    }
    // A going on that is waiting is off if the slide, as it is now, asks for none: its
    // timer was taken off, or its presentation's Slide Show was. One that is still
    // asked for runs out as it was started, and nothing is started here.
    if ((announcements ? announcementAdvance : advance).waiting && !advanceOf(cue).waiting)
        stopAdvance(announcements, effects);
    return effects;
}

void State::renamed(const QString &from, const QString &to)
{
    if (playlistId.isEmpty() && key == from)
        key = to;
    if (announcement.playlistId.isEmpty() && announcement.key == from)
        announcement.key = to;
}

Effects State::leavePresentation()
{
    Effects effects;
    stopAdvance(false, effects);
    stopAdvance(true, effects);
    atSlide = false;
    key.clear();
    playlistId.clear();
    presentation.clear();
    index = -1;
    count = 0;
    slide.clear();
    next.clear();
    clearedByCue = false;
    // (And the announcements are at none: what is on their layer stays there.)
    const bool wasCleared = announcement.cleared;
    announcement = Place();
    announcement.cleared = wasCleared;
    return effects;
}

bool State::alreadyPlaying(const QVariantMap &asked) const
{
    return playsOn(asked, hasMedia, media);
}

Effects State::showMedia(const QVariantMap &asked, const QString &playlist)
{
    Effects effects;
    const bool playing = alreadyPlaying(asked);
    effects << note("media", mediaWords(asked) + (playing ? QStringLiteral(", which is playing already and is left to") : QString()));
    hasMedia = true;
    media = asked;
    mediaPlaylistId = playlist;
    // (Media already playing is the same playing, kept where it was.)
    if (!playing)
        mediaPlace = playlist.isEmpty() || asked.value("id").toString().isEmpty()
                         ? QVariantMap()
                         : QVariantMap {{"kind", QStringLiteral("mediaRow")}, {"item", asked.value("id").toString()}};
    if (!playing)
        effects << withMedia(Effect::Media, asked);
    return effects;
}

void State::clearSlide(Effects &effects)
{
    // Cleared by anything but its own action, the slide no longer leads anywhere.
    if (!m_runningCue)
        stopAdvance(false, effects);
    if (cleared)
        return;
    effects << note("clear", QStringLiteral("the slide") + (m_runningCue ? QStringLiteral(", by its own action") : QString()));
    copyright.clear();
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
    mediaPlace.clear();
    effects << plain(Effect::NoMedia);
}

void State::showVideoInput(int index, Effects &effects)
{
    if (index < 0)
        return;
    if (videoInput == index) {
        effects << note("input", QStringLiteral("video input %1 is on already, and is left as it is").arg(index + 1));
        return;
    }
    effects << note("input", QStringLiteral("video input %1 on").arg(index + 1)
                                 + (videoInput >= 0 ? QStringLiteral(", in place of %1").arg(videoInput + 1) : QString()));
    videoInput = index;
    Effect shown;
    shown.kind = Effect::VideoInput;
    shown.action = {{"index", index}};
    effects << shown;
}

void State::clearVideoInput(Effects &effects)
{
    if (videoInput < 0)
        return;
    effects << note("clear", QStringLiteral("the video input, %1").arg(videoInput + 1));
    videoInput = -1;
    effects << plain(Effect::NoVideoInput);
}

Effects State::showVideoInput(int index)
{
    Effects effects;
    showVideoInput(index, effects);
    return effects;
}

Effects State::clearVideoInput()
{
    Effects effects;
    clearVideoInput(effects);
    return effects;
}

void State::clearAnnouncements(Effects &effects)
{
    if (!m_runningAnnouncement)
        stopAdvance(true, effects);
    if (announcement.cleared && !announcementHasMedia)
        return;
    effects << note("clear", QStringLiteral("the announcements") + (m_runningAnnouncement ? QStringLiteral(", by their own slide's action") : QString()));
    announcement.clearedByCue = m_runningAnnouncement && !announcement.cleared;
    announcement.cleared = true;
    announcementHasMedia = false;
    announcementMedia.clear();
    announcementMediaPlace.clear();
    effects << plain(Effect::NoAnnouncements);
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

void State::showMessage(const QVariantMap &message, Effects &effects, const QVariantMap &values)
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
    shown.action = values;
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

// The row a playlist is started at, by an action that plays it: the one the action
// names, if it names one that is still there with a file to play, or else the first
// that has a file to play; an empty map for none. A GUESS: that ProPresenter starts a
// playlist at its first item, which it would have to for an action that names none (as
// none of its own that have been seen does).
static QVariantMap firstRow(const QVariantList &rows, const QString &itemId)
{
    const auto playable = [](const QVariantMap &row) { return !row.value("missing").toBool() && !row.value("path").toString().isEmpty(); };
    if (!itemId.isEmpty()) {
        for (const QVariant &entry : rows) {
            const QVariantMap row = entry.toMap();
            if (row.value("id").toString() == itemId && playable(row))
                return row;
        }
    }
    for (const QVariant &entry : rows) {
        const QVariantMap row = entry.toMap();
        if (playable(row))
            return row;
    }
    return {};
}

// A playlist of the media bin or of the audio bin, played by an action as a click on
// its first row in the bin would play it (see playlistactions.h). A playlist that is
// playing already, which is to say whose media is on the media layer, or whose music is
// the music, whichever of its rows that is, is left to play on: unless the action is
// set to Always Retrigger, when it starts again from its first row, a background there
// that is playing already being started again with it. Music played so goes on to the
// next of its playlist as music clicked in the bin does (see audioFinished).
//
// GUESSES at ProPresenter: that "playing already" is the playlist, and not the row the
// action starts at; that a sound effect at the head of an audio playlist plays over the
// music, as any sound effect does, and so is never playing already; and that what its
// file calls Select Playlist (which none of its own actions that have been seen has) is
// only a matter of which playlist its bin shows, which is not done here.
void State::playPlaylist(const QVariantMap &action, const Workspace &workspace, Effects &effects)
{
    const bool media = action.value("kind").toString() == QLatin1String("mediaPlaylist");
    const QString title = action.value("title").toString();
    const QVariantMap list = workspace.playlist ? workspace.playlist(media ? QStringLiteral("media") : QStringLiteral("audio"),
                                                                     action.value("playlistId").toString(),
                                                                     action.value("playlistName").toString())
                                                : QVariantMap();
    if (list.isEmpty()) {
        effects << note("action", title + QStringLiteral(": there is no such playlist here"));
        return;
    }
    const QString id = list.value("id").toString();
    const bool retrigger = action.value("retrigger").toBool();
    const bool playing = media ? hasMedia && mediaPlaylistId == id : hasMusic && musicPlaylistId == id;
    if (playing && !retrigger) {
        effects << note("action", title + QStringLiteral(": it is playing already, and is left to play on"));
        return;
    }
    const QVariantMap row = firstRow(list.value("rows").toList(), action.value("itemId").toString());
    if (row.isEmpty()) {
        effects << note("action", title + QStringLiteral(": it has nothing on this computer to play"));
        return;
    }
    effects << note("action", title + (playing ? QStringLiteral(", which is playing already: from its start again") : QString()));
    if (media) {
        QVariantMap started = row;
        if (retrigger)
            started.insert("retriggers", true);
        effects << showMedia(started, id);
    } else {
        startAudio(row, id, effects);
    }
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

namespace {

bool playableMedia(const QVariantMap &row)
{
    return !row.value("missing").toBool() && !row.value("path").toString().isEmpty();
}

} // namespace

int State::followingMedia(const QVariantList &rows, const QString &id, const std::function<int(int)> &random)
{
    int at = -1;
    for (int i = 0; i < rows.size(); ++i) {
        if (rows.at(i).toMap().value("id").toString() == id) {
            at = i;
            break;
        }
    }
    // Its own row no longer there, nothing follows it.
    if (at < 0)
        return -1;
    const QVariantMap ended = rows.at(at).toMap();
    const QString kind = ended.value("next").toString();
    if (kind == QLatin1String("next")) {
        for (int i = at + 1; i < rows.size(); ++i) {
            if (playableMedia(rows.at(i).toMap()))
                return i;
        }
        return -1;
    }
    if (kind == QLatin1String("first")) {
        for (int i = 0; i < rows.size(); ++i) {
            if (playableMedia(rows.at(i).toMap()))
                return i;
        }
        return -1;
    }
    if (kind == QLatin1String("cue")) {
        const QString cue = ended.value("nextCue").toString();
        for (int i = 0; i < rows.size(); ++i) {
            if (rows.at(i).toMap().value("cueId").toString() == cue)
                return playableMedia(rows.at(i).toMap()) ? i : -1;
        }
        return -1;
    }
    if (kind == QLatin1String("random")) {
        QList<int> others;
        for (int i = 0; i < rows.size(); ++i) {
            if (i != at && playableMedia(rows.at(i).toMap()))
                others << i;
        }
        // With no other, the one there is, if it has a file
        if (others.isEmpty())
            return playableMedia(ended) ? at : -1;
        return others.at(qBound(0, random ? random(int(others.size())) : 0, int(others.size()) - 1));
    }
    return -1;
}

Effects State::mediaFinished(const QString &id, const QVariantList &rows, const std::function<int(int)> &random)
{
    Effects effects;
    // (Media that has been put in its place, or cleared, since is not what ended; nor
    // is media a slide triggered, which has no playlist to go on in.)
    if (!hasMedia || mediaPlaylistId.isEmpty() || id.isEmpty() || media.value("id").toString() != id)
        return effects;
    const int at = followingMedia(rows, id, random);
    QString kind;
    for (const QVariant &row : rows) {
        if (row.toMap().value("id").toString() == id)
            kind = row.toMap().value("next").toString();
    }
    if (at < 0) {
        effects << note("media", mediaWords(media) + QStringLiteral(" has played to its end")
                                     + (kind.isEmpty() ? QString() : QStringLiteral(", and there is nothing for it to go on to")));
        return effects;
    }
    effects << note("media", mediaWords(media) + QStringLiteral(" has played to its end, and goes on to ")
                                 + (kind == QLatin1String("cue") ? QStringLiteral("the one it names") : kind == QLatin1String("random")
                                        ? QStringLiteral("one at random") : QStringLiteral("the ") + kind));
    effects += showMedia(rows.at(at).toMap(), mediaPlaylistId);
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

Effects State::clearAnnouncements()
{
    Effects effects;
    clearAnnouncements(effects);
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
    clearVideoInput(effects);
    clearAnnouncements(effects);
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
// TO COME: a group names one more of ProPresenter's layers (video inputs: see
// clearfile.h). When the show gains it, it is cleared here, for a group that has it.
void State::clearGroup(const QVariantMap &group, Effects &effects)
{
    const QStringList layers = group.value("layers").toStringList();
    effects << note("clear", QStringLiteral("the clear group %1 asked for").arg(quoted(group.value("name").toString())));
    if (layers.contains(QLatin1String("slide")))
        clearSlide(effects);
    if (layers.contains(QLatin1String("media")))
        clearMedia(effects);
    if (layers.contains(QLatin1String("videoInput")))
        clearVideoInput(effects);
    if (layers.contains(QLatin1String("announcements")))
        clearAnnouncements(effects);
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

QVariantMap State::placeOf(const QString &layer) const
{
    if (layer == QLatin1String("media"))
        return hasMedia ? mediaPlace : QVariantMap();
    if (layer == QLatin1String("announcements"))
        return announcementHasMedia ? announcementMediaPlace : QVariantMap();
    if (layer == QLatin1String("audio") && hasMusic && !musicPlaylistId.isEmpty() && !music.value("id").toString().isEmpty())
        return {{"kind", QStringLiteral("audioRow")}, {"item", music.value("id").toString()}};
    return {};
}

QStringList State::setMarkers(const QVariantMap &place, const QVariantList &markers)
{
    QStringList changed;
    if (place.isEmpty())
        return changed;
    if (placeOf(QStringLiteral("media")) == place) {
        putMarkers(&media, markers);
        changed << QStringLiteral("media");
    }
    if (placeOf(QStringLiteral("announcements")) == place) {
        putMarkers(&announcementMedia, markers);
        changed << QStringLiteral("announcements");
    }
    if (placeOf(QStringLiteral("audio")) == place) {
        putMarkers(&music, markers);
        changed << QStringLiteral("audio");
    }
    return changed;
}

Effects State::runMarker(const QVariantMap &marker, const QString &media, const Workspace &workspace)
{
    Effects effects;
    const QVariantList actions = marker.value("actions").toList();
    const QString name = marker.value("name").toString();
    const double time = marker.value("time").toDouble();
    effects << note("marker", (name.isEmpty() ? QStringLiteral("the marker at %1 s").arg(time, 0, 'f', 2)
                                              : QStringLiteral("the marker ") + quoted(name))
                                  + QStringLiteral(" of ") + media + QStringLiteral(": ") + actionsWord(actions.size()));
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
        if (action.value("announcements").toBool()) {
            // (The announcements are not one of the file's layers but a destination:
            // see actions.cpp.)
            clearAnnouncements(effects);
        } else if (layer == 0) {
            effects << note("clear", QStringLiteral("everything asked for"));
            clearSlide(effects);
            clearMedia(effects);
            clearVideoInput(effects);
            clearAnnouncements(effects);
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
        } else if (layer == 3) {
            clearVideoInput(effects);
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
    } else if (kind == QLatin1String("videoInput")) {
        // A video input, by its place, put on its layer (see videoInput in the header)
        effects << note("action", title);
        showVideoInput(qMax(0, action.value("inputIndex").toInt()), effects);
    } else if (kind == QLatin1String("mediaPlaylist") || kind == QLatin1String("audioPlaylist")) {
        playPlaylist(action, workspace, effects);
    } else if (kind == QLatin1String("message")) {
        // Shown with what the action gives its text tokens, by the token's id; a token
        // it gives nothing shows what it was last given on the messages' tab (see
        // messagefile::actionValues).
        const QVariantMap message = findMessage(action.value("messageId").toString(), action.value("messageName").toString(), workspace);
        effects << note("action", title + (message.isEmpty() ? QStringLiteral(": there is no such message here") : QString()));
        QVariantMap values;
        for (const QVariant &entry : action.value("values").toList()) {
            const QVariantMap value = entry.toMap();
            if (value.value("kind").toString() == QLatin1String("text"))
                values.insert(value.value("tokenId").toString(), value.value("value").toString());
        }
        if (!message.isEmpty())
            showMessage(message, effects, values);
    } else if (kind == QLatin1String("communication")) {
        // Sent by whatever has the devices (see Devices), which says in the log what
        // became of it: the device may not be there, or not connected.
        effects << note("action", title + (action.value("done").toBool() ? QString() : QStringLiteral(": not done here")));
        if (action.value("done").toBool()) {
            Effect made;
            made.kind = Effect::Communicate;
            made.action = action;
            effects << made;
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
