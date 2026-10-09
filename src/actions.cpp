#include "actions.h"

#include "timers.pb.h"
#include "workspacefiles.h"

#include <QVariantList>

namespace actions {

namespace {

using Action = rv::data::Action;
using Configuration = rv::data::Timer::Configuration;
using Identification = rv::data::CollectionElementType;

QString text(const std::string &value)
{
    return QString::fromStdString(value);
}

QString quoted(const QString &name)
{
    return name.isEmpty() ? QStringLiteral("(unnamed)") : QStringLiteral("“%1”").arg(name);
}

void identify(Identification *identification, const QString &id, const QString &name)
{
    if (id.isEmpty())
        identification->clear_parameter_uuid();
    else
        identification->mutable_parameter_uuid()->set_string(id.toStdString());
    identification->set_parameter_name(name.toStdString());
}

// Says which collection a prop or a macro is in, if that is known.
void identifyCollection(Identification *of, const QVariantMap &wanted)
{
    const QString id = wanted.value("collectionId").toString();
    const QString name = wanted.value("collectionName").toString();
    if (id.isEmpty() && name.isEmpty())
        of->clear_parent_collection();
    else
        identify(of->mutable_parent_collection(), id, name);
}

// A kind of action as words, from its name in the file: "ACTION_TYPE_AUDIENCE_LOOK"
// is "Audience look".
QString typeWords(Action::ActionType type)
{
    QString words = text(Action::ActionType_Name(type)).mid(int(qstrlen("ACTION_TYPE_"))).toLower().replace('_', ' ');
    if (!words.isEmpty())
        words[0] = words.at(0).toUpper();
    return words.isEmpty() ? QStringLiteral("Action") : words;
}

// What an action of a kind not understood here is to do with, if it says: the name
// of the look, the message, the clear group or the playlist.
QString subject(const Action &action)
{
    if (action.has_audience_look())
        return text(action.audience_look().identification().parameter_name());
    if (action.has_message())
        return text(action.message().message_identificaton().parameter_name());
    if (action.has_clear_group())
        return text(action.clear_group().identification().parameter_name());
    if (action.has_playlist_item())
        return text(action.playlist_item().playlist_name());
    return text(action.name());
}

void describeTimer(const Action::TimerType &timer, QVariantMap *map)
{
    static const char *const verbs[] = {"Start", "Stop", "Reset", "Reset and start", "Stop and reset", "Add time to"};
    const int action = int(timer.action_type());
    const QString name = text(timer.timer_identification().parameter_name());
    map->insert("action", action);
    map->insert("timerId", text(timer.timer_identification().parameter_uuid().string()));
    map->insert("timerName", name);
    map->insert("amount", timer.increment_amount());
    map->insert("set", timer.has_timer_configuration());
    if (timer.has_timer_configuration()) {
        const Configuration &configuration = timer.timer_configuration();
        map->insert("configuration", QString::fromLatin1(QByteArray::fromStdString(configuration.SerializeAsString()).toBase64()));
        map->insert("setKind", configuration.has_countdown_to_time() ? QStringLiteral("countdownTo")
                                 : configuration.has_elapsed_time() ? QStringLiteral("elapsed") : QStringLiteral("countdown"));
        map->insert("setDuration", configuration.countdown().duration());
        map->insert("setTimeOfDay", configuration.countdown_to_time().time_of_day());
        map->insert("setStartTime", configuration.elapsed_time().start_time());
        map->insert("setEndTime", configuration.elapsed_time().end_time());
        map->insert("setHasEndTime", configuration.elapsed_time().has_end_time());
        map->insert("setOverrun", configuration.allows_overrun());
    }
    map->insert("title", QStringLiteral("%1 the timer %2%3").arg(QLatin1String(action >= 0 && action <= 5 ? verbs[action] : "Do something to"),
                                                               quoted(name), timer.has_timer_configuration() ? QStringLiteral(", set anew")
                                                                                                             : QString()));
}

void buildTimer(const QVariantMap &wanted, Action::TimerType *timer)
{
    timer->set_action_type(Action::TimerType::TimerAction(qBound(0, wanted.value("action").toInt(), 5)));
    identify(timer->mutable_timer_identification(), wanted.value("timerId").toString(), wanted.value("timerName").toString());
    timer->set_increment_amount(wanted.value("amount").toDouble());
    if (!wanted.value("set").toBool()) {
        timer->clear_timer_configuration();
        return;
    }
    // Setting one kind up takes the others' settings away, as it does for a timer.
    Configuration *configuration = timer->mutable_timer_configuration();
    const QString kind = wanted.value("setKind").toString();
    if (kind == QLatin1String("countdownTo")) {
        configuration->mutable_countdown_to_time()->set_time_of_day(qBound(0.0, wanted.value("setTimeOfDay").toDouble(), 86399.0));
        configuration->mutable_countdown_to_time()->set_period(Configuration::TimerTypeCountdownToTime::TIME_PERIOD_24);
    } else if (kind == QLatin1String("elapsed")) {
        configuration->mutable_elapsed_time()->set_start_time(qMax(0.0, wanted.value("setStartTime").toDouble()));
        configuration->mutable_elapsed_time()->set_end_time(qMax(0.0, wanted.value("setEndTime").toDouble()));
        configuration->mutable_elapsed_time()->set_has_end_time(wanted.value("setHasEndTime").toBool());
    } else {
        configuration->mutable_countdown()->set_duration(qMax(0.0, wanted.value("setDuration").toDouble()));
    }
    configuration->set_allows_overrun(wanted.value("setOverrun").toBool());
}

// The layers there are to clear, as words; and whether clearing that one is done here.
QString layerWords(const Action::ClearType &clear, bool *done)
{
    using Clear = Action::ClearType;
    *done = false;
    if (clear.content_destination() == Action::CONTENT_DESTINATION_ANNOUNCEMENTS)
        return QStringLiteral("Clear the announcements");
    switch (clear.target_layer()) {
    case Clear::CLEAR_TARGET_LAYER_ALL:
        *done = true;
        return QStringLiteral("Clear everything");
    case Clear::CLEAR_TARGET_LAYER_BACKGROUND:
        *done = true;
        return QStringLiteral("Clear the media");
    case Clear::CLEAR_TARGET_LAYER_PROP:
        *done = true;
        return QStringLiteral("Clear the props");
    case Clear::CLEAR_TARGET_LAYER_SLIDE:
        *done = true;
        return QStringLiteral("Clear the slide");
    case Clear::CLEAR_TARGET_LAYER_AUDIO:
        return QStringLiteral("Clear the audio");
    case Clear::CLEAR_TARGET_LAYER_LIVE_VIDEO:
        return QStringLiteral("Clear the video input");
    case Clear::CLEAR_TARGET_LAYER_LOGO:
        return QStringLiteral("Clear to the logo");
    case Clear::CLEAR_TARGET_LAYER_MESSAGES:
        return QStringLiteral("Clear the messages");
    case Clear::CLEAR_TARGET_LAYER_AUDIO_EFFECTS:
        return QStringLiteral("Clear the audio effects");
    default:
        return QStringLiteral("Clear a layer");
    }
}

} // namespace

bool listed(const rv::data::Action &action)
{
    const bool slide = action.has_slide();
    const bool visualMedia = action.has_media() && (action.media().element().has_video() || action.media().element().has_image());
    return !slide && !visualMedia;
}

QVariantMap describe(const rv::data::Action &action)
{
    QVariantMap map {{"id", text(action.uuid().string())}, {"done", true}};
    if (action.has_timer()) {
        map.insert("kind", QStringLiteral("timer"));
        describeTimer(action.timer(), &map);
    } else if (action.has_clear()) {
        bool done = false;
        map.insert("kind", QStringLiteral("clear"));
        map.insert("layer", int(action.clear().target_layer()));
        map.insert("title", layerWords(action.clear(), &done));
        map.insert("done", done);
    } else if (action.has_stage()) {
        QVariantList assignments;
        QStringList layouts;
        for (const rv::data::Stage::ScreenAssignment &assignment : action.stage().stage_screen_assignments()) {
            const QString layout = text(assignment.layout().parameter_name());
            assignments.append(QVariantMap {
                {"screenId", text(assignment.screen().parameter_uuid().string())},
                {"screenName", text(assignment.screen().parameter_name())},
                {"layoutId", text(assignment.layout().parameter_uuid().string())},
                {"layoutName", layout},
            });
            if (!layout.isEmpty())
                layouts.append(quoted(layout));
        }
        map.insert("kind", QStringLiteral("stage"));
        map.insert("assignments", assignments);
        map.insert("title", layouts.isEmpty() ? QStringLiteral("Stage: no change")
                                              : QStringLiteral("Give the stage the layout %1").arg(layouts.join(QStringLiteral(", "))));
    } else if (action.has_prop()) {
        const Identification &prop = action.prop().identification();
        map.insert("kind", QStringLiteral("prop"));
        map.insert("propId", text(prop.parameter_uuid().string()));
        map.insert("propName", text(prop.parameter_name()));
        map.insert("collectionId", text(prop.parent_collection().parameter_uuid().string()));
        map.insert("collectionName", text(prop.parent_collection().parameter_name()));
        map.insert("clear", action.prop().has_clear());
        map.insert("title", QStringLiteral("%1 the prop %2").arg(action.prop().has_clear() ? QStringLiteral("Clear") : QStringLiteral("Show"),
                                                               quoted(text(prop.parameter_name()))));
    } else if (action.has_audience_look()) {
        const Identification &look = action.audience_look().identification();
        map.insert("kind", QStringLiteral("look"));
        map.insert("lookId", text(look.parameter_uuid().string()));
        map.insert("lookName", text(look.parameter_name()));
        map.insert("title", QStringLiteral("Go over to the look %1").arg(quoted(text(look.parameter_name()))));
    } else if (action.has_macro()) {
        const Identification &macro = action.macro().identification();
        map.insert("kind", QStringLiteral("macro"));
        map.insert("macroId", text(macro.parameter_uuid().string()));
        map.insert("macroName", text(macro.parameter_name()));
        map.insert("collectionId", text(macro.parent_collection().parameter_uuid().string()));
        map.insert("collectionName", text(macro.parent_collection().parameter_name()));
        map.insert("title", QStringLiteral("Run the macro %1").arg(quoted(text(macro.parameter_name()))));
    } else {
        const QString about = subject(action);
        map.insert("kind", QStringLiteral("other"));
        map.insert("title", typeWords(action.type()) + (about.isEmpty() ? QString() : QStringLiteral(": ") + about));
        map.insert("done", false);
    }
    return map;
}

QString build(const QVariantMap &wanted, rv::data::Action *action)
{
    const QString kind = wanted.value("kind").toString();
    if (action->uuid().string().empty())
        action->mutable_uuid()->set_string(workspace::newUuid());
    action->set_isenabled(true);
    if (kind == QLatin1String("timer")) {
        action->set_type(Action::ACTION_TYPE_TIMER);
        buildTimer(wanted, action->mutable_timer());
    } else if (kind == QLatin1String("clear")) {
        action->set_type(Action::ACTION_TYPE_CLEAR);
        action->mutable_clear()->set_target_layer(Action::ClearType::ClearTargetLayer(qBound(0, wanted.value("layer").toInt(), 8)));
    } else if (kind == QLatin1String("stage")) {
        action->set_type(Action::ACTION_TYPE_STAGE_LAYOUT);
        // ProPresenter gives these this name, and no other kind one.
        action->set_name("Stage");
        Action::StageLayoutType *stage = action->mutable_stage();
        stage->clear_stage_screen_assignments();
        const QVariantList assignments = wanted.value("assignments").toList();
        for (const QVariant &entry : assignments) {
            const QVariantMap assignment = entry.toMap();
            rv::data::Stage::ScreenAssignment *made = stage->add_stage_screen_assignments();
            identify(made->mutable_screen(), assignment.value("screenId").toString(), assignment.value("screenName").toString());
            // A screen that is left as it is has a layout with nothing in it.
            made->mutable_layout();
            if (!assignment.value("layoutId").toString().isEmpty() || !assignment.value("layoutName").toString().isEmpty())
                identify(made->mutable_layout(), assignment.value("layoutId").toString(), assignment.value("layoutName").toString());
        }
    } else if (kind == QLatin1String("look")) {
        action->set_type(Action::ACTION_TYPE_AUDIENCE_LOOK);
        identify(action->mutable_audience_look()->mutable_identification(), wanted.value("lookId").toString(), wanted.value("lookName").toString());
    } else if (kind == QLatin1String("prop")) {
        action->set_type(Action::ACTION_TYPE_PROP);
        Identification *prop = action->mutable_prop()->mutable_identification();
        identify(prop, wanted.value("propId").toString(), wanted.value("propName").toString());
        identifyCollection(prop, wanted);
        if (wanted.value("clear").toBool())
            action->mutable_prop()->mutable_clear();
        else if (!action->prop().has_trigger())
            action->mutable_prop()->mutable_trigger();
    } else if (kind == QLatin1String("macro")) {
        action->set_type(Action::ACTION_TYPE_MACRO);
        Identification *macro = action->mutable_macro()->mutable_identification();
        identify(macro, wanted.value("macroId").toString(), wanted.value("macroName").toString());
        identifyCollection(macro, wanted);
    } else {
        return QStringLiteral("That kind of action cannot be made here");
    }
    return {};
}

} // namespace actions
