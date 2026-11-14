#include "timeline.h"

#include "presentation.pb.h"

#include <QHash>

#include <algorithm>

namespace timeline {

using Timeline = rv::data::Presentation::Timeline;
using TimelineCue = rv::data::Presentation::Timeline::Cue;
using Cues = google::protobuf::RepeatedPtrField<TimelineCue>;

namespace {

QString text(const std::string &value)
{
    return QString::fromStdString(value);
}

// The list the cues are in: the new one wherever it has any (see the header)
const Cues &cuesOf(const Timeline &timeline)
{
    return timeline.cues_v2_size() > 0 || timeline.cues_size() == 0 ? timeline.cues_v2() : timeline.cues();
}

// The list a change is made in, which is always the new one: a timeline that has only
// the old one has it copied over first, and the old one left as it was.
Cues *changedCues(Timeline *timeline)
{
    if (timeline->cues_v2_size() == 0 && timeline->cues_size() > 0)
        *timeline->mutable_cues_v2() = timeline->cues();
    return timeline->mutable_cues_v2();
}

// The timeline to change. A presentation with none is given what ProPresenter gives
// every presentation, its length.
Timeline *changedTimeline(rv::data::Presentation *presentation)
{
    if (!presentation->has_timeline())
        presentation->mutable_timeline()->set_duration(kUsualDuration);
    return presentation->mutable_timeline();
}

// What its media says of a piece of audio's or a video's length: where it ends, which
// is its end point; 0 where it says nothing
double mediaLength(const rv::data::Action &action)
{
    if (!action.has_media())
        return 0;
    const rv::data::Media &element = action.media().element();
    if (element.has_audio())
        return element.audio().transport().end_point();
    if (element.has_video())
        return element.video().transport().end_point();
    return 0;
}

double lengthOf(const rv::data::Action &action)
{
    return action.duration() > 0 ? action.duration() : mediaLength(action);
}

// Every slide's name on a timeline and its group, by the slide's id (see describe in
// the header): numbered from 1 through the groups in the order they are stored, and
// then any slide that is in no group, in the order the cues are
QHash<QString, QVariantMap> slidesOf(const rv::data::Presentation &presentation)
{
    QHash<QString, QVariantMap> slides;
    int number = 0;
    for (const auto &group : presentation.cue_groups()) {
        const rv::data::Color &color = group.group().color();
        QString colour;
        if (group.group().has_color() && color.alpha() > 0) {
            const auto channel = [](float value) { return qBound(0, qRound(value * 255), 255); };
            colour = QStringLiteral("#%1%2%3").arg(channel(color.red()), 2, 16, QLatin1Char('0'))
                         .arg(channel(color.green()), 2, 16, QLatin1Char('0')).arg(channel(color.blue()), 2, 16, QLatin1Char('0'));
        }
        for (const rv::data::UUID &id : group.cue_identifiers()) {
            const QString key = text(id.string());
            if (slides.contains(key))
                continue;
            slides.insert(key, QVariantMap {{"name", QStringLiteral("Cue %1").arg(++number)},
                                            {"group", text(group.group().name())},
                                            {"groupColor", colour}});
        }
    }
    for (const rv::data::Cue &cue : presentation.cues()) {
        const QString key = text(cue.uuid().string());
        if (!slides.contains(key))
            slides.insert(key, QVariantMap {{"name", QStringLiteral("Cue %1").arg(++number)}, {"group", QString()}, {"groupColor", QString()}});
    }
    return slides;
}

// The cues in the order they are to be done: by time, and at one time by place
QList<int> inOrder(const QVariantList &cues, const std::function<bool(const QVariantMap &)> &wanted)
{
    QList<int> places;
    for (int place = 0; place < cues.size(); ++place) {
        if (wanted(cues.at(place).toMap()))
            places << place;
    }
    std::stable_sort(places.begin(), places.end(), [&](int a, int b) {
        return cues.at(a).toMap().value("time").toDouble() < cues.at(b).toMap().value("time").toDouble();
    });
    return places;
}

} // namespace

QVariantMap describe(const rv::data::Presentation &presentation,
                     const std::function<QVariantMap(const rv::data::Action &)> &describeAction)
{
    const Timeline &timeline = presentation.timeline();
    const QHash<QString, QVariantMap> slides = slidesOf(presentation);
    QVariantMap slideMap;
    for (auto slide = slides.constBegin(); slide != slides.constEnd(); ++slide)
        slideMap.insert(slide.key(), slide.value());

    QVariantList cues;
    const Cues &list = cuesOf(timeline);
    for (int place = 0; place < list.size(); ++place) {
        const TimelineCue &cue = list.at(place);
        QVariantMap map {
            {"place", place},
            {"time", cue.trigger_time()},
            {"name", text(cue.name())},
            {"length", 0.0},
        };
        if (cue.has_action()) {
            const QVariantMap action = describeAction(cue.action());
            const QString kind = action.value("kind").toString();
            map.insert("track", kind == QLatin1String("audio") || kind == QLatin1String("media") ? kind : QStringLiteral("action"));
            map.insert("action", action);
            map.insert("length", lengthOf(cue.action()));
        } else {
            // (A cue with neither is taken for a slide that is not there: nothing is
            // triggered by it, and it is drawn where it is.)
            const QString id = text(cue.cue_id().string());
            const QVariantMap slide = slides.value(id);
            map.insert("track", QStringLiteral("slide"));
            map.insert("slideId", id);
            map.insert("group", slide.value("group").toString());
            map.insert("groupColor", slide.value("groupColor").toString());
        }
        cues.append(map);
    }

    return {
        {"duration", timeline.duration() > 0 ? timeline.duration() : kUsualDuration},
        {"loop", timeline.loop()},
        {"timecode", timeline.timecode_enable()},
        {"timecodeOffset", timeline.timecode_offset()},
        {"oldList", timeline.cues_v2_size() == 0 && timeline.cues_size() > 0},
        {"cues", cues},
        {"slides", slideMap},
    };
}

QList<int> passed(const QVariantList &cues, double from, double to)
{
    return inOrder(cues, [&](const QVariantMap &cue) {
        const double time = cue.value("time").toDouble();
        return time > from && time <= to;
    });
}

QList<Entry> entering(const QVariantList &cues, double at)
{
    // The slide in force: the last slide cue at or before `at`, the later in the file
    // of two at one time (the one that would have been triggered last)
    int slide = -1;
    for (int place = 0; place < cues.size(); ++place) {
        const QVariantMap cue = cues.at(place).toMap();
        if (cue.value("track").toString() != QLatin1String("slide") || cue.value("time").toDouble() > at)
            continue;
        if (slide < 0 || cue.value("time").toDouble() >= cues.at(slide).toMap().value("time").toDouble())
            slide = place;
    }
    QList<Entry> entries;
    for (int place : inOrder(cues, [](const QVariantMap &) { return true; })) {
        const QVariantMap cue = cues.at(place).toMap();
        const QString track = cue.value("track").toString();
        const double time = cue.value("time").toDouble();
        if (track == QLatin1String("slide")) {
            if (place == slide)
                entries << Entry {place, 0};
        } else if (track == QLatin1String("audio") || track == QLatin1String("media")) {
            const double length = cue.value("length").toDouble();
            if (time == at || (time < at && at < time + length))
                entries << Entry {place, at - time};
        } else if (time == at) {
            entries << Entry {place, 0};
        }
    }
    return entries;
}

double nextTime(const QVariantList &cues, double after)
{
    double next = -1;
    for (const QVariant &entry : cues) {
        const double time = entry.toMap().value("time").toDouble();
        if (time > after && (next < 0 || time < next))
            next = time;
    }
    return next;
}

int slidePlace(const QStringList &ids, const QString &slideId, int after)
{
    for (int place = qMax(0, after + 1); place < ids.size(); ++place) {
        if (ids.at(place) == slideId)
            return place;
    }
    return ids.indexOf(slideId);
}

bool addSlide(rv::data::Presentation *presentation, double time, const QString &slideId)
{
    const QHash<QString, QVariantMap> slides = slidesOf(*presentation);
    if (!slides.contains(slideId))
        return false;
    TimelineCue *cue = changedCues(changedTimeline(presentation))->Add();
    // (A time of 0 is left out of the file, as ProPresenter leaves it out.)
    if (time > 0)
        cue->set_trigger_time(time);
    cue->mutable_cue_id()->set_string(slideId.toStdString());
    cue->set_name(slides.value(slideId).value("name").toString().toStdString());
    return true;
}

void addAction(rv::data::Presentation *presentation, double time, const rv::data::Action &action, const QString &name)
{
    TimelineCue *cue = changedCues(changedTimeline(presentation))->Add();
    if (time > 0)
        cue->set_trigger_time(time);
    cue->set_name(name.toStdString());
    *cue->mutable_action() = action;
    if (cue->action().duration() <= 0 && mediaLength(action) > 0)
        cue->mutable_action()->set_duration(mediaLength(action));
}

bool move(rv::data::Presentation *presentation, int place, double time)
{
    if (!presentation->has_timeline() || place < 0 || place >= cuesOf(presentation->timeline()).size())
        return false;
    time = qMax(0.0, time);
    if (cuesOf(presentation->timeline()).at(place).trigger_time() == time)
        return false;
    TimelineCue *cue = changedCues(presentation->mutable_timeline())->Mutable(place);
    if (time > 0)
        cue->set_trigger_time(time);
    else
        cue->clear_trigger_time();
    return true;
}

bool remove(rv::data::Presentation *presentation, int place)
{
    if (!presentation->has_timeline() || place < 0 || place >= cuesOf(presentation->timeline()).size())
        return false;
    Cues *cues = changedCues(presentation->mutable_timeline());
    cues->erase(cues->begin() + place);
    // The last taken off a timeline that was in the old list empties that too: else the
    // old list, which is read where the new one has nothing, would be what it had again.
    if (cues->empty())
        presentation->mutable_timeline()->clear_cues();
    return true;
}

bool setDuration(rv::data::Presentation *presentation, double seconds)
{
    seconds = qMax(1.0, seconds);
    const double had = presentation->timeline().duration() > 0 ? presentation->timeline().duration() : kUsualDuration;
    if (presentation->has_timeline() && had == seconds)
        return false;
    changedTimeline(presentation)->set_duration(seconds);
    return true;
}

bool setLoop(rv::data::Presentation *presentation, bool loop)
{
    if (presentation->timeline().loop() == loop)
        return false;
    changedTimeline(presentation)->set_loop(loop);
    return true;
}

} // namespace timeline
