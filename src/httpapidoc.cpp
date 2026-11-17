#include "httpapidoc.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>

namespace httpapidoc {

namespace {

QString keyOf(const QByteArray &method, const QString &pattern)
{
    return QString::fromLatin1(method) + u' ' + pattern;
}

// What each route does, by "METHOD pattern". The words are the app's own, of what the
// app does (which is not always all that ProPresenter does by the same path: the
// transport's routes work one layer here), and none of them is from ProPresenter's
// documentation. The families of routes are said in the loops httpapi::routes() makes
// them by, so that a route added to one of those is said here by the same change.
const QHash<QString, QString> &summaries()
{
    static const QHash<QString, QString> all = [] {
        QHash<QString, QString> made;
        const auto say = [&made](const char *method, const QString &pattern, const QString &text) {
            made.insert(keyOf(method, pattern), text);
        };
        const auto get = [&say](const QString &pattern, const QString &text) { say("GET", pattern, text); };
        const QString timeline = QStringLiteral(": whether it is running, and where its playhead is.");
        const QString works = QStringLiteral("Plays, pauses or rewinds ");

        // ---- Status
        get(QStringLiteral("version"), QStringLiteral("What is answering: the computer's network name, its platform, and the app and its version."));
        get(QStringLiteral("status/layers"),
            QStringLiteral("Which layers have something on them: the slide, media, announcements, props, messages, audio and the video input."));
        get(QStringLiteral("status/slide"), QStringLiteral("The slide that is live and the slide after it: their words, their notes and their ids."));
        get(QStringLiteral("status/screens"), QStringLiteral("The screens that are set up, the audience's and the stage's."));
        get(QStringLiteral("status/audience_screens"), QStringLiteral("Whether the audience screens are switched on."));
        say("PUT", QStringLiteral("status/audience_screens"), QStringLiteral("Switches the audience screens on or off."));
        get(QStringLiteral("status/stage_screens"), QStringLiteral("Whether the stage screens are switched on."));
        say("PUT", QStringLiteral("status/stage_screens"), QStringLiteral("Switches the stage screens on or off."));
        say("POST", QStringLiteral("status/updates"),
            QStringLiteral("Streams several things on one connection. Any request that takes `chunked` can be asked for, by its path without "
                           "\"/v1/\"; each is sent at first and again whenever it changes, as {url, data}. The computer's clock is sent "
                           "every second, whatever was asked for."));

        // ---- Presentations
        get(QStringLiteral("presentation/active"), QStringLiteral("The presentation that is live: its groups, its slides and its arrangements."));
        get(QStringLiteral("presentation/focused"), QStringLiteral("The presentation that is open in the app's window."));
        get(QStringLiteral("presentation/slide_index"), QStringLiteral("Where the show is: the index of the live slide, and the presentation it is in."));
        get(QStringLiteral("presentation/active/timeline"), QStringLiteral("The live presentation's timeline") + timeline);
        get(QStringLiteral("presentation/focused/timeline"), QStringLiteral("The timeline of the presentation open in the window") + timeline);
        get(QStringLiteral("presentation/active/timeline/{operation}"), works + QStringLiteral("the live presentation's timeline."));
        get(QStringLiteral("presentation/focused/timeline/{operation}"), works + QStringLiteral("the timeline of the presentation open in the window."));
        get(QStringLiteral("presentation/{uuid}/timeline/{operation}"), works + QStringLiteral("a presentation's timeline."));
        get(QStringLiteral("presentation/{uuid}"), QStringLiteral("A presentation, by its UUID: its groups, its slides and its arrangements."));
        get(QStringLiteral("presentation/{uuid}/thumbnail/{index}"), QStringLiteral("A picture of one slide of a presentation, as the screens show it."));
        get(QStringLiteral("playlist/{playlist_id}/{index}/thumbnail/{cue_index}"), QStringLiteral("A picture of one slide of an item of a playlist."));
        get(QStringLiteral("playlist/active/presentation/{index}/thumbnail/{cue_index}"),
            QStringLiteral("A picture of one slide of an item of the playlist that is live."));
        get(QStringLiteral("playlist/active/announcement/{index}/thumbnail/{cue_index}"),
            QStringLiteral("A picture of one slide of an item of the announcements' playlist that is live."));
        for (const char *which : {"active", "focused", "{uuid}"}) {
            const QString at = QStringLiteral("presentation/") + QLatin1String(which);
            const QString of = qstrcmp(which, "active") == 0 ? QStringLiteral("the live presentation")
                             : qstrcmp(which, "focused") == 0 ? QStringLiteral("the presentation open in the window") : QStringLiteral("a presentation");
            // (One by its UUID need not be where the show is: then it is begun from its top)
            const QString from = qstrcmp(which, "{uuid}") == 0 ? QStringLiteral(": counted from the live slide if the show is in it, and its first slide if not.")
                                                              : QStringLiteral(".");
            get(at + QStringLiteral("/trigger"), QStringLiteral("Shows the first slide of %1.").arg(of));
            get(at + QStringLiteral("/next/trigger"), QStringLiteral("Shows the next slide of %1%2").arg(of, from));
            get(at + QStringLiteral("/previous/trigger"), QStringLiteral("Shows the previous slide of %1%2").arg(of, from));
            get(at + QStringLiteral("/{index}/trigger"), QStringLiteral("Shows one slide of %1, by its index.").arg(of));
            get(at + QStringLiteral("/group/{group_id}/trigger"), QStringLiteral("Shows the first slide of a group of %1.").arg(of));
            if (qstrcmp(which, "focused") != 0)
                get(at + QStringLiteral("/focus"), QStringLiteral("Opens %1 in the window.").arg(of));
        }
        get(QStringLiteral("presentation/next/focus"), QStringLiteral("Opens the presentation after the one open in the window, in its library or its playlist."));
        get(QStringLiteral("presentation/previous/focus"), QStringLiteral("Opens the presentation before the one open in the window, in its library or its playlist."));

        // ---- Triggers
        get(QStringLiteral("trigger/next"), QStringLiteral("The next slide, as the right arrow key does it."));
        get(QStringLiteral("trigger/previous"), QStringLiteral("The previous slide, as the left arrow key does it."));
        get(QStringLiteral("trigger/media/next"), QStringLiteral("Plays the item after the one playing, in the media playlist that is playing."));
        get(QStringLiteral("trigger/media/previous"), QStringLiteral("Plays the item before the one playing, in the media playlist that is playing."));
        get(QStringLiteral("trigger/audio/next"), QStringLiteral("Plays the item after the one playing, in the audio playlist that is playing."));
        get(QStringLiteral("trigger/audio/previous"), QStringLiteral("Plays the item before the one playing, in the audio playlist that is playing."));

        // ---- Libraries
        get(QStringLiteral("libraries"), QStringLiteral("The libraries."));
        get(QStringLiteral("library/{library_id}"), QStringLiteral("A library's presentations."));
        get(QStringLiteral("library/{library_id}/{presentation_id}/trigger"), QStringLiteral("Shows the first slide of a presentation of a library."));
        get(QStringLiteral("library/{library_id}/{presentation_id}/{index}/trigger"),
            QStringLiteral("Shows one slide of a presentation of a library, by its index."));

        // ---- Playlists
        get(QStringLiteral("playlists"), QStringLiteral("The playlists and their folders, as a tree."));
        get(QStringLiteral("playlist/{playlist_id}"), QStringLiteral("A playlist's items."));
        get(QStringLiteral("playlist/{playlist_id}/updates"),
            QStringLiteral("A playlist's items, for following them: with `chunked` they are sent again whenever they change."));
        get(QStringLiteral("playlist/active"),
            QStringLiteral("The playlists that are live, the presentations' and the announcements', and the item of each that the show is at."));
        get(QStringLiteral("playlist/focused"), QStringLiteral("The playlist open in the window, and the item of it that is open."));
        get(QStringLiteral("playlist/focused/next"), QStringLiteral("The item after the one open in the window, in its playlist."));
        get(QStringLiteral("playlist/focused/previous"), QStringLiteral("The item before the one open in the window, in its playlist."));
        get(QStringLiteral("playlist/focused/{index}/timeline"), QStringLiteral("The timeline of an item of the playlist open in the window") + timeline);
        get(QStringLiteral("playlist/focused/{index}/timeline/{operation}"),
            works + QStringLiteral("the timeline of an item of the playlist open in the window."));
        get(QStringLiteral("playlist/{playlist_id}/{index}/timeline/{operation}"), works + QStringLiteral("the timeline of an item of a playlist."));
        for (const char *which : {"focused", "{playlist_id}"}) {
            const bool focused = qstrcmp(which, "focused") == 0;
            const QString at = QStringLiteral("playlist/") + QLatin1String(which);
            const QString of = focused ? QStringLiteral("the playlist open in the window") : QStringLiteral("a playlist");
            get(at + QStringLiteral("/trigger"), QStringLiteral("Shows the first item of %1.").arg(of));
            get(at + QStringLiteral("/next/trigger"),
                focused ? QStringLiteral("Shows the item after the one open in the window, in its playlist, and opens it there.")
                        : QStringLiteral("Shows the next item of a playlist: counted from the live item if the show is in the playlist, and its first item if not."));
            get(at + QStringLiteral("/previous/trigger"),
                focused ? QStringLiteral("Shows the item before the one open in the window, in its playlist, and opens it there.")
                        : QStringLiteral("Shows the previous item of a playlist: counted from the live item if the show is in the playlist, and its first item if not."));
            get(at + QStringLiteral("/{index}/trigger"), QStringLiteral("Shows one item of %1, by its index.").arg(of));
            get(at + QStringLiteral("/{index}/{cue_index}/trigger"), QStringLiteral("Shows one slide of one item of %1.").arg(of));
        }
        get(QStringLiteral("playlist/{playlist_id}/focus"), QStringLiteral("Opens a playlist in the window."));
        get(QStringLiteral("playlist/next/focus"), QStringLiteral("Opens the playlist after the one open in the window."));
        get(QStringLiteral("playlist/previous/focus"), QStringLiteral("Opens the playlist before the one open in the window."));
        for (const char *destination : {"presentation", "announcement"}) {
            const QString at = QStringLiteral("playlist/active/") + QLatin1String(destination);
            const QString of = qstrcmp(destination, "presentation") == 0 ? QStringLiteral("the playlist that is live")
                                                                        : QStringLiteral("the announcements' playlist that is live");
            get(at + QStringLiteral("/focus"), QStringLiteral("Opens %1 in the window, at the item the show is at.").arg(of));
            get(at + QStringLiteral("/trigger"), QStringLiteral("Shows the first item of %1.").arg(of));
            get(at + QStringLiteral("/{index}/trigger"), QStringLiteral("Shows one item of %1, by its index.").arg(of));
            get(at + QStringLiteral("/{index}/{cue_index}/trigger"), QStringLiteral("Shows one slide of one item of %1.").arg(of));
            get(at + QStringLiteral("/{index}/timeline"), QStringLiteral("The timeline of an item of %1").arg(of) + timeline);
            get(at + QStringLiteral("/{index}/timeline/{operation}"), works + QStringLiteral("the timeline of an item of %1.").arg(of));
        }

        // ---- Announcements
        get(QStringLiteral("announcement/active"), QStringLiteral("The announcements' presentation that is live: its groups, its slides and its arrangements."));
        get(QStringLiteral("announcement/slide_index"),
            QStringLiteral("Where the announcements are: the index of their live slide, and the presentation it is in."));
        get(QStringLiteral("announcement/active/timeline"), QStringLiteral("The live announcements' timeline") + timeline);
        get(QStringLiteral("announcement/active/timeline/{operation}"), works + QStringLiteral("the live announcements' timeline."));
        get(QStringLiteral("announcement/active/trigger"), QStringLiteral("Shows the first slide of the announcements' presentation that is live."));
        get(QStringLiteral("announcement/active/next/trigger"), QStringLiteral("Shows the next slide of the announcements."));
        get(QStringLiteral("announcement/active/previous/trigger"), QStringLiteral("Shows the previous slide of the announcements."));
        get(QStringLiteral("announcement/active/{index}/trigger"), QStringLiteral("Shows one slide of the announcements' presentation that is live, by its index."));
        get(QStringLiteral("announcement/active/focus"), QStringLiteral("Opens the announcements' presentation that is live in the window."));

        // ---- Clearing
        get(QStringLiteral("clear/layer/{layer}"), QStringLiteral("Clears a layer."));
        get(QStringLiteral("clear/groups"), QStringLiteral("The clear groups."));
        get(QStringLiteral("clear/group/{id}"), QStringLiteral("A clear group."));
        get(QStringLiteral("clear/group/{id}/trigger"), QStringLiteral("Clears what a clear group clears."));

        // ---- Global groups, looks, macros, masks
        get(QStringLiteral("groups"), QStringLiteral("The workspace's groups: the names and colours a group of slides can be given."));
        get(QStringLiteral("looks"), QStringLiteral("The looks."));
        get(QStringLiteral("look/current"), QStringLiteral("The look that is live."));
        get(QStringLiteral("look/{id}"), QStringLiteral("A look."));
        get(QStringLiteral("look/{id}/trigger"), QStringLiteral("Makes a look the live look."));
        get(QStringLiteral("macros"), QStringLiteral("The macros."));
        get(QStringLiteral("macro/{id}"), QStringLiteral("A macro."));
        get(QStringLiteral("macro/{id}/trigger"), QStringLiteral("Runs a macro."));
        get(QStringLiteral("macro_collections"), QStringLiteral("The collections of macros."));
        get(QStringLiteral("macro_collection/{id}"), QStringLiteral("A collection of macros."));
        get(QStringLiteral("masks"), QStringLiteral("The masks."));
        get(QStringLiteral("mask/{id}"), QStringLiteral("A mask."));

        // ---- Media playlists, and audio's
        for (const char *kind : {"media", "audio"}) {
            const bool media = qstrcmp(kind, "media") == 0;
            const QString a = media ? QStringLiteral("media playlist") : QStringLiteral("audio playlist");
            const QString at = QLatin1String(kind) + QStringLiteral("/playlist");
            get(QLatin1String(kind) + QStringLiteral("/playlists"), QStringLiteral("The %1s and their folders, as a tree.").arg(a));
            get(at + QStringLiteral("/{playlist_id}"), QStringLiteral("The items of a %1.").arg(a));
            get(at + QStringLiteral("/{playlist_id}/updates"),
                QStringLiteral("The items of a %1, for following them: with `chunked` they are sent again whenever they change.").arg(a));
            get(at + QStringLiteral("/active"), QStringLiteral("The %1 that is playing, and the item of it that is.").arg(a));
            get(at + QStringLiteral("/focused"), QStringLiteral("The %1 open in the window.").arg(a));
            for (const char *which : {"active", "focused", "{playlist_id}"}) {
                const QString of = qstrcmp(which, "active") == 0 ? QStringLiteral("the %1 that is playing").arg(a)
                                 : qstrcmp(which, "focused") == 0 ? QStringLiteral("the %1 open in the window").arg(a) : QStringLiteral("a %1").arg(a);
                const QString in = at + u'/' + QLatin1String(which);
                get(in + QStringLiteral("/trigger"), QStringLiteral("Plays the first item of %1.").arg(of));
                get(in + QStringLiteral("/next/trigger"), QStringLiteral("Plays the item after the one playing, in %1.").arg(of));
                get(in + QStringLiteral("/previous/trigger"), QStringLiteral("Plays the item before the one playing, in %1.").arg(of));
                get(in + (media ? QStringLiteral("/{media_id}/trigger") : QStringLiteral("/{id}/trigger")), QStringLiteral("Plays one item of %1.").arg(of));
            }
            get(at + QStringLiteral("/{playlist_id}/focus"), QStringLiteral("Opens a %1 in the window.").arg(a));
            get(at + QStringLiteral("/active/focus"), QStringLiteral("Opens the %1 that is playing in the window.").arg(a));
            get(at + QStringLiteral("/next/focus"), QStringLiteral("Opens the %1 after the one open in the window.").arg(a));
            get(at + QStringLiteral("/previous/focus"), QStringLiteral("Opens the %1 before the one open in the window.").arg(a));
        }

        // ---- Messages and props
        get(QStringLiteral("messages"), QStringLiteral("The messages."));
        get(QStringLiteral("message/{id}"), QStringLiteral("A message."));
        say("POST", QStringLiteral("message/{id}/trigger"), QStringLiteral("Shows a message. The body may give the words of its text tokens."));
        get(QStringLiteral("message/{id}/clear"), QStringLiteral("Hides a message."));
        get(QStringLiteral("props"), QStringLiteral("The props."));
        get(QStringLiteral("prop/{id}"), QStringLiteral("A prop."));
        get(QStringLiteral("prop/{id}/trigger"), QStringLiteral("Puts a prop on."));
        get(QStringLiteral("prop/{id}/clear"), QStringLiteral("Takes a prop off."));
        get(QStringLiteral("prop_collections"), QStringLiteral("The collections of props."));
        get(QStringLiteral("prop_collection/{id}"), QStringLiteral("A collection of props."));

        // ---- The stage
        get(QStringLiteral("stage/screens"), QStringLiteral("The stage screens."));
        get(QStringLiteral("stage/layouts"), QStringLiteral("The stage layouts."));
        get(QStringLiteral("stage/layout_map"), QStringLiteral("Which layout each stage screen has."));
        say("PUT", QStringLiteral("stage/layout_map"), QStringLiteral("Gives stage screens their layouts."));
        get(QStringLiteral("stage/screen/{id}/layout"), QStringLiteral("The layout a stage screen has."));
        get(QStringLiteral("stage/screen/{id}/layout/{layout_id}"), QStringLiteral("Gives a stage screen a layout."));
        get(QStringLiteral("stage/message"), QStringLiteral("The stage message that is showing."));
        say("PUT", QStringLiteral("stage/message"), QStringLiteral("Shows a stage message."));
        say("DELETE", QStringLiteral("stage/message"), QStringLiteral("Hides the stage message."));

        // ---- Timers and clocks
        get(QStringLiteral("timers"), QStringLiteral("The timers, as they are set up."));
        get(QStringLiteral("timers/current"), QStringLiteral("Every timer's time now, and whether it is running."));
        get(QStringLiteral("timers/{operation}"), QStringLiteral("Starts, stops or resets every timer."));
        get(QStringLiteral("timer/{id}"), QStringLiteral("A timer, as it is set up."));
        get(QStringLiteral("timer/{id}/{operation}"), QStringLiteral("Starts, stops or resets a timer."));
        say("PUT", QStringLiteral("timer/{id}"), QStringLiteral("Sets a timer up: its kind, its times, and whether it may overrun."));
        say("PUT", QStringLiteral("timer/{id}/{operation}"), QStringLiteral("Sets a timer up, and then starts, stops or resets it."));
        get(QStringLiteral("timer/{id}/increment/{time}"), QStringLiteral("Gives a timer more time, or with a negative number less."));
        get(QStringLiteral("timer/system_time"), QStringLiteral("The computer's clock, in seconds since 1970."));
        get(QStringLiteral("timer/video_countdown"), QStringLiteral("The time left of the video that is playing."));

        // ---- Transport: what plays on a layer
        get(QStringLiteral("transport/{layer}/current"), QStringLiteral("What is playing on a layer, and whether it is playing or paused."));
        get(QStringLiteral("transport/{layer}/time"), QStringLiteral("How far what is playing on a layer has got, in seconds."));
        say("PUT", QStringLiteral("transport/{layer}/time"), QStringLiteral("Moves the video that is playing to a time."));
        get(QStringLiteral("transport/{layer}/play"), QStringLiteral("Plays the video that is on the layer."));
        get(QStringLiteral("transport/{layer}/pause"), QStringLiteral("Pauses the video that is playing."));
        get(QStringLiteral("transport/{layer}/skip_forward/{time}"), QStringLiteral("Moves the video that is playing on by so many seconds."));
        get(QStringLiteral("transport/{layer}/skip_backward/{time}"), QStringLiteral("Moves the video that is playing back by so many seconds."));
        get(QStringLiteral("transport/{layer}/go_to_end"),
            QStringLiteral("Moves the video that is playing to its end, or with `time` to so many seconds before its end."));
        get(QStringLiteral("transport/{layer}/auto_advance"), QStringLiteral("Whether a slide's Go To Next Timer is waiting on a layer."));

        // ---- Capture, and video inputs
        get(QStringLiteral("capture/status"), QStringLiteral("Whether the app is capturing, which it never is: it has no capture."));
        get(QStringLiteral("video_inputs"), QStringLiteral("The video inputs."));
        get(QStringLiteral("video_inputs/{id}/trigger"), QStringLiteral("Puts a video input on the video input layer."));
        return made;
    }();
    return all;
}

// The headings, each with what the page says under it. The names are ProPresenter's for
// the parts of its API; the order is its page's, which is the alphabet's. (Two of its
// headings have nothing under them here and are left out: its Miscellaneous and its
// Theme.)
struct Heading
{
    const char *name;
    const char *about;
};
const QList<Heading> &headings()
{
    static const QList<Heading> all {
        {"Announcement", "The announcements layer, which has a presentation of its own."},
        {"Audio", "The audio bin: its playlists, and playing what is in them."},
        {"Capture", "Capture, which the app has not: it says so, for a client that asks."},
        {"Clear", "Clearing a layer, and the clear groups."},
        {"Global Groups", "The groups a slide can be put in."},
        {"Library", "The libraries and their presentations."},
        {"Looks", "The looks, and which is live."},
        {"Macro", "The macros and their collections."},
        {"Masks", "The masks."},
        {"Media", "The media bin: its playlists, and playing what is in them."},
        {"Message", "The messages, shown and hidden."},
        {"Playlist", "The playlists of presentations: their items, which is live and which is open, and showing an item."},
        {"Presentation", "The presentation that is live, the one open in the window, and any by its UUID: their slides shown, their timelines, and "
                         "pictures of their slides."},
        {"Prop", "The props and their collections."},
        {"Stage", "The stage screens, their layouts, and the stage message."},
        {"Status", "What is showing now, the screens, and many things followed on one connection."},
        {"Timer", "The timers and the clocks."},
        {"Transport", "What plays on a layer. Every layer can be asked what is playing on it; only the presentation layer's video can be played, "
                      "paused and moved through here."},
        {"Trigger", "Next and previous."},
        {"Video Input", "The video inputs."},
    };
    return all;
}

// Which heading a route is under, by the first word of its path
QString headingOf(const QString &pattern)
{
    static const QHash<QString, QString> byWord {
        {QStringLiteral("version"), QStringLiteral("Status")},
        {QStringLiteral("status"), QStringLiteral("Status")},
        {QStringLiteral("presentation"), QStringLiteral("Presentation")},
        {QStringLiteral("trigger"), QStringLiteral("Trigger")},
        {QStringLiteral("libraries"), QStringLiteral("Library")},
        {QStringLiteral("library"), QStringLiteral("Library")},
        {QStringLiteral("playlists"), QStringLiteral("Playlist")},
        {QStringLiteral("playlist"), QStringLiteral("Playlist")},
        {QStringLiteral("announcement"), QStringLiteral("Announcement")},
        {QStringLiteral("clear"), QStringLiteral("Clear")},
        {QStringLiteral("groups"), QStringLiteral("Global Groups")},
        {QStringLiteral("looks"), QStringLiteral("Looks")},
        {QStringLiteral("look"), QStringLiteral("Looks")},
        {QStringLiteral("macros"), QStringLiteral("Macro")},
        {QStringLiteral("macro"), QStringLiteral("Macro")},
        {QStringLiteral("macro_collections"), QStringLiteral("Macro")},
        {QStringLiteral("macro_collection"), QStringLiteral("Macro")},
        {QStringLiteral("masks"), QStringLiteral("Masks")},
        {QStringLiteral("mask"), QStringLiteral("Masks")},
        {QStringLiteral("media"), QStringLiteral("Media")},
        {QStringLiteral("audio"), QStringLiteral("Audio")},
        {QStringLiteral("messages"), QStringLiteral("Message")},
        {QStringLiteral("message"), QStringLiteral("Message")},
        {QStringLiteral("props"), QStringLiteral("Prop")},
        {QStringLiteral("prop"), QStringLiteral("Prop")},
        {QStringLiteral("prop_collections"), QStringLiteral("Prop")},
        {QStringLiteral("prop_collection"), QStringLiteral("Prop")},
        {QStringLiteral("stage"), QStringLiteral("Stage")},
        {QStringLiteral("timers"), QStringLiteral("Timer")},
        {QStringLiteral("timer"), QStringLiteral("Timer")},
        {QStringLiteral("transport"), QStringLiteral("Transport")},
        {QStringLiteral("capture"), QStringLiteral("Capture")},
        {QStringLiteral("video_inputs"), QStringLiteral("Video Input")},
    };
    return byWord.value(pattern.section(u'/', 0, 0));
}

QVariantMap text(const QString &description)
{
    return {{QStringLiteral("type"), QStringLiteral("string")}, {QStringLiteral("description"), description}};
}

// One of the parts of a route's path that stand for something: what it is, and how it
// is written. (ProPresenter finds a thing by its UUID, then by its name, then by its
// index, and so does the app: httpapi::resolve.)
QVariantMap pathPart(const QString &pattern, const QString &name)
{
    const QString found = QStringLiteral(": its UUID, its name or its index (from 0), looked for in that order.");
    QVariantMap schema {{QStringLiteral("type"), QStringLiteral("string")}};
    QString about;
    if (name == QLatin1String("uuid")) {
        about = QStringLiteral("The presentation's UUID.");
    } else if (name == QLatin1String("index")) {
        // (Of a playlist it is an item's; anywhere else a slide's)
        const bool item = pattern.startsWith(QLatin1String("playlist/"));
        about = item ? QStringLiteral("The item's index in the playlist, from 0.") : QStringLiteral("The slide's index, from 0.");
        schema = {{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 0}};
    } else if (name == QLatin1String("cue_index")) {
        about = QStringLiteral("The slide's index in the item, from 0.");
        schema = {{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 0}};
    } else if (name == QLatin1String("operation")) {
        const bool timeline = pattern.contains(QLatin1String("/timeline/"));
        about = timeline ? QStringLiteral("What to do with the timeline.") : QStringLiteral("What to do with the timer.");
        schema.insert(QStringLiteral("enum"), timeline ? QStringList {QStringLiteral("play"), QStringLiteral("pause"), QStringLiteral("rewind")}
                                                       : QStringList {QStringLiteral("start"), QStringLiteral("stop"), QStringLiteral("reset")});
    } else if (name == QLatin1String("layer")) {
        const bool clear = pattern.startsWith(QLatin1String("clear/"));
        about = clear ? QStringLiteral("The layer to clear.") : QStringLiteral("The layer.");
        schema.insert(QStringLiteral("enum"),
                      clear ? QStringList {QStringLiteral("audio"), QStringLiteral("props"), QStringLiteral("messages"), QStringLiteral("announcements"),
                                           QStringLiteral("slide"), QStringLiteral("media"), QStringLiteral("video_input")}
                            : QStringList {QStringLiteral("presentation"), QStringLiteral("announcement"), QStringLiteral("audio")});
    } else if (name == QLatin1String("time")) {
        about = pattern.contains(QLatin1String("/increment/")) ? QStringLiteral("Seconds to give it; a negative number takes them away.")
                                                               : QStringLiteral("Seconds.");
        schema = {{QStringLiteral("type"), QStringLiteral("number")}};
    } else if (name == QLatin1String("playlist_id")) {
        about = QStringLiteral("The playlist: its UUID or its name wherever it is, its index among those in no folder, or its names from the top "
                               "(\"Folder/Playlist\", the slash written %2F).");
    } else if (name == QLatin1String("library_id")) {
        about = QStringLiteral("The library") + found;
    } else if (name == QLatin1String("presentation_id")) {
        about = QStringLiteral("The presentation") + found;
    } else if (name == QLatin1String("group_id")) {
        about = QStringLiteral("The group") + found;
    } else if (name == QLatin1String("layout_id")) {
        about = QStringLiteral("The stage layout") + found;
    } else {
        // "id", "media_id": whatever the path is of
        about = QStringLiteral("Which one") + found;
    }
    return {{QStringLiteral("name"), name}, {QStringLiteral("in"), QStringLiteral("path")}, {QStringLiteral("required"), true},
            {QStringLiteral("description"), about}, {QStringLiteral("schema"), schema}};
}

QVariantMap queryPart(const QString &name, const QString &about, const QVariantMap &schema)
{
    return {{QStringLiteral("name"), name}, {QStringLiteral("in"), QStringLiteral("query")}, {QStringLiteral("required"), false},
            {QStringLiteral("description"), about}, {QStringLiteral("schema"), schema}};
}

// What a route that takes a body takes: what it is, its form, and one to try
struct Body
{
    QString about;
    QVariantMap schema;
    QVariant example;
    bool required = true;
};
Body bodyOf(const httpapi::Route &route)
{
    const QString &pattern = route.pattern;
    const QVariantMap id {{QStringLiteral("type"), QStringLiteral("object")},
                          {QStringLiteral("properties"), QVariantMap {{QStringLiteral("uuid"), text(QStringLiteral("Its UUID"))},
                                                                      {QStringLiteral("name"), text(QStringLiteral("or its name"))}}}};
    if (pattern == QLatin1String("status/audience_screens") || pattern == QLatin1String("status/stage_screens"))
        return {QStringLiteral("On, or off."), {{QStringLiteral("type"), QStringLiteral("boolean")}}, true};
    if (pattern == QLatin1String("status/updates")) {
        return {QStringLiteral("The paths to follow, each without \"/v1/\"."),
                {{QStringLiteral("type"), QStringLiteral("array")}, {QStringLiteral("items"), QVariantMap {{QStringLiteral("type"), QStringLiteral("string")}}}},
                QStringList {QStringLiteral("status/slide"), QStringLiteral("timers/current")}};
    }
    if (pattern == QLatin1String("message/{id}/trigger")) {
        const QVariantMap token {{QStringLiteral("type"), QStringLiteral("object")},
                                 {QStringLiteral("properties"),
                                  QVariantMap {{QStringLiteral("name"), text(QStringLiteral("The token's name"))},
                                               {QStringLiteral("text"),
                                                QVariantMap {{QStringLiteral("type"), QStringLiteral("object")},
                                                             {QStringLiteral("properties"),
                                                              QVariantMap {{QStringLiteral("text"), text(QStringLiteral("Its words"))}}}}}}}};
        return {QStringLiteral("The words of the message's text tokens, for those that are to be other than they are. May be left out."),
                {{QStringLiteral("type"), QStringLiteral("array")}, {QStringLiteral("items"), token}},
                QVariantList {QVariantMap {{QStringLiteral("name"), QStringLiteral("Name")},
                                           {QStringLiteral("text"), QVariantMap {{QStringLiteral("text"), QStringLiteral("Sam")}}}}},
                false};
    }
    if (pattern == QLatin1String("stage/layout_map")) {
        const QVariantMap entry {{QStringLiteral("type"), QStringLiteral("object")},
                                 {QStringLiteral("properties"), QVariantMap {{QStringLiteral("screen"), id}, {QStringLiteral("layout"), id}}}};
        return {QStringLiteral("Each stage screen that is to change, with the layout it is to have."),
                {{QStringLiteral("type"), QStringLiteral("array")}, {QStringLiteral("items"), entry}},
                QVariantList {QVariantMap {{QStringLiteral("screen"), QVariantMap {{QStringLiteral("name"), QStringLiteral("Stage")}}},
                                           {QStringLiteral("layout"), QVariantMap {{QStringLiteral("name"), QStringLiteral("Lyrics")}}}}}};
    }
    if (pattern == QLatin1String("stage/message"))
        return {QStringLiteral("The message's words, as a JSON string."), {{QStringLiteral("type"), QStringLiteral("string")}}, QStringLiteral("Two minutes")};
    if (pattern == QLatin1String("transport/{layer}/time"))
        return {QStringLiteral("The time to move to, in seconds."), {{QStringLiteral("type"), QStringLiteral("number")}}, 30};
    if (pattern.startsWith(QLatin1String("timer/{id}"))) {
        return {QStringLiteral("The timer as `GET /v1/timer/{id}` answers it, with what is to be other than it is changed: the answer to that "
                               "request, altered and sent back, is the way to make one."),
                {{QStringLiteral("type"), QStringLiteral("object")}}, QVariant()};
    }
    return {};
}

QString operationId(const httpapi::Route &route)
{
    QString made = QString::fromLatin1(route.method).toLower();
    for (const QString &part : route.pattern.split(u'/')) {
        QString word = part;
        word.remove(u'{').remove(u'}');
        made += u'_' + word;
    }
    return made;
}

}

QString summary(const httpapi::Route &route)
{
    return summaries().value(keyOf(route.method, route.pattern));
}

QStringList summarised()
{
    return summaries().keys();
}

QString tag(const httpapi::Route &route)
{
    return headingOf(route.pattern);
}

QStringList tags()
{
    QStringList names;
    for (const Heading &heading : headings())
        names.append(QString::fromLatin1(heading.name));
    return names;
}

QStringList unanswered()
{
    // The chord chart, which the app has none of to send; and the cancelling of a
    // slide's Go To Next Timer, which has not been built (the timer can be asked after)
    return {QStringLiteral("GET presentation/chord_chart/updates"), QStringLiteral("DELETE transport/{layer}/auto_advance")};
}

QVariantMap openApi(const QString &version, const QString &extendedPage)
{
    // (What operates the show is answered 204, with nothing, as ProPresenter answers it)
    const QVariantMap done {{QStringLiteral("description"), QStringLiteral("Done. Nothing is sent back.")}};
    const QVariantMap none {{QStringLiteral("description"),
                             QStringLiteral("Not there: what the path names (an id, a name, an index) is nothing the app has, or there is nothing "
                                            "live or open for it to be about. The answer is empty, as ProPresenter's is.")}};
    const QVariantMap wrong {{QStringLiteral("description"), QStringLiteral("The body is not what this takes. The answer is empty.")}};
    const QVariantMap json {{QStringLiteral("application/json"), QVariantMap {{QStringLiteral("schema"), QVariantMap()}}}};

    QVariantMap paths;
    for (const httpapi::Route &route : httpapi::routes()) {
        const QString said = summary(route);
        if (said.isEmpty())
            continue;
        const QString &pattern = route.pattern;
        const QString path = pattern == QLatin1String("version") ? QStringLiteral("/version") : QStringLiteral("/v1/") + pattern;

        QVariantList parameters;
        for (const QString &part : pattern.split(u'/')) {
            if (part.startsWith(u'{') && part.endsWith(u'}'))
                parameters.append(pathPart(pattern, part.mid(1, part.size() - 2)));
        }
        if (route.streams) {
            parameters.append(queryPart(QStringLiteral("chunked"),
                                        QStringLiteral("`true` to follow it: the answer stays open, and the thing is sent at first and again each "
                                                       "time it changes, each piece followed by an empty line. (This page waits for an answer to "
                                                       "end, so it is for a program to ask and not for trying here.)"),
                                        {{QStringLiteral("type"), QStringLiteral("boolean")}}));
        }
        if (route.picture) {
            parameters.append(queryPart(QStringLiteral("quality"),
                                        QStringLiteral("The length of the picture's longer side, in pixels: 400 unless asked, and between 16 and 1920."),
                                        {{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 16},
                                         {QStringLiteral("maximum"), 1920}, {QStringLiteral("default"), 400}}));
            parameters.append(queryPart(QStringLiteral("thumbnail_type"), QStringLiteral("`png` for a PNG; a JPEG unless asked."),
                                        {{QStringLiteral("type"), QStringLiteral("string")},
                                         {QStringLiteral("enum"), QStringList {QStringLiteral("jpeg"), QStringLiteral("png")}}}));
        }
        if (pattern == QLatin1String("transport/{layer}/go_to_end")) {
            parameters.append(queryPart(QStringLiteral("time"), QStringLiteral("Seconds before the end to move to; the end itself unless asked."),
                                        {{QStringLiteral("type"), QStringLiteral("number")}}));
        }

        // What it answers: a picture, something as JSON, or nothing but that it was done
        QVariantMap responses;
        const bool reads = route.method == "GET" && !httpapi::operates(route);
        if (route.picture) {
            const QVariantMap binary {{QStringLiteral("schema"), QVariantMap {{QStringLiteral("type"), QStringLiteral("string")},
                                                                              {QStringLiteral("format"), QStringLiteral("binary")}}}};
            responses.insert(QStringLiteral("200"),
                             QVariantMap {{QStringLiteral("description"), QStringLiteral("The picture.")},
                                          {QStringLiteral("content"),
                                           QVariantMap {{QStringLiteral("image/jpeg"), binary}, {QStringLiteral("image/png"), binary}}}});
        } else if (reads || pattern == QLatin1String("status/updates")) {
            responses.insert(QStringLiteral("200"),
                             QVariantMap {{QStringLiteral("description"), pattern == QLatin1String("status/updates")
                                                                              ? QStringLiteral("A stream that stays open: each piece is {url, data}, followed by an empty line.")
                                                                              : QStringLiteral("What was asked for, as JSON.")},
                                          {QStringLiteral("content"), json}});
        } else {
            responses.insert(QStringLiteral("204"), done);
        }
        if (!parameters.isEmpty() || pattern.contains(QLatin1String("active")) || pattern.contains(QLatin1String("focused")))
            responses.insert(QStringLiteral("404"), none);

        QVariantMap operation {{QStringLiteral("operationId"), operationId(route)},
                               {QStringLiteral("summary"), said},
                               {QStringLiteral("tags"), QStringList {tag(route)}}};
        if (!parameters.isEmpty())
            operation.insert(QStringLiteral("parameters"), parameters);
        const Body body = bodyOf(route);
        if (route.method != "GET" && route.method != "DELETE" && !body.about.isEmpty()) {
            QVariantMap content {{QStringLiteral("schema"), body.schema}};
            if (body.example.isValid())
                content.insert(QStringLiteral("example"), body.example);
            operation.insert(QStringLiteral("requestBody"),
                             QVariantMap {{QStringLiteral("description"), body.about},
                                          {QStringLiteral("required"), body.required},
                                          {QStringLiteral("content"), QVariantMap {{QStringLiteral("application/json"), content}}}});
            responses.insert(QStringLiteral("400"), wrong);
        }
        operation.insert(QStringLiteral("responses"), responses);

        QVariantMap methods = paths.value(path).toMap();
        methods.insert(QString::fromLatin1(route.method).toLower(), operation);
        paths.insert(path, methods);
    }

    QVariantList described;
    for (const Heading &heading : headings())
        described.append(QVariantMap {{QStringLiteral("name"), QString::fromLatin1(heading.name)},
                                      {QStringLiteral("description"), QString::fromUtf8(heading.about)}});

    // The page's opening words (in Markdown, which the page draws). What they must say:
    // that this is not ProPresenter's documentation, and that what is listed is what
    // the app answers and no more.
    QString about = QStringLiteral(
        "The part of ProPresenter's network API that Simple Presenter answers, by ProPresenter's own paths and in its JSON, so that a program "
        "made for ProPresenter can follow and operate a show here.\n\n"
        "This page lists **only what the app answers**. ProPresenter's API has more, which answers 404 here, as ProPresenter answers what it "
        "does not know. The requests under each heading are tried from this page against the app that served it: one that shows a slide or "
        "clears a layer **does so on the screens**.\n\n"
        "Simple Presenter is not ProPresenter and has nothing to do with Renewed Vision. It is one person's hobby, shared as an example and "
        "not as a product: nothing here is promised to stay as it is.");
    if (!extendedPage.isEmpty()) {
        about += QStringLiteral("\n\nThe app has an API of its own beside this one, for making and changing presentations, playlists and themes: "
                                "[the extended API](%1) has its own page.").arg(extendedPage);
    } else {
        about += QStringLiteral("\n\nThe app has an API of its own beside this one, for making and changing presentations, playlists and themes. "
                                "It is switched off: switched on (Settings, Network, Enable Extended API and MCP), it has a page of its own "
                                "like this one.");
    }

    return {{QStringLiteral("openapi"), QStringLiteral("3.0.3")},
            {QStringLiteral("info"), QVariantMap {{QStringLiteral("title"), QStringLiteral("Simple Presenter: ProPresenter's API, as far as it is answered")},
                                                  {QStringLiteral("version"), version},
                                                  {QStringLiteral("description"), about}}},
            // (Asked of whatever served the page: an address with nothing before its
            // first slash is that computer's own)
            {QStringLiteral("servers"), QVariantList {QVariantMap {{QStringLiteral("url"), QStringLiteral("/")},
                                                                   {QStringLiteral("description"), QStringLiteral("The app that served this page")}}}},
            {QStringLiteral("tags"), described},
            {QStringLiteral("paths"), paths}};
}

QByteArray asScript(const QVariantMap &description)
{
    // (JSON is JavaScript but for two characters that end a line in a script and not in
    // a JSON string, U+2028 and U+2029: Qt writes them as they are, so they are written
    // as escapes here. And "</" is written "<\/", for a page that might hold this in a
    // script element.)
    QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(description)).toJson(QJsonDocument::Compact);
    json.replace("\xe2\x80\xa8", "\\u2028");
    json.replace("\xe2\x80\xa9", "\\u2029");
    json.replace("</", "<\\/");
    return "var openapi_spec = " + json + ";\n";
}

File file(const QString &name)
{
    // The page itself is the app's own (src/apidoc.html); the rest is Swagger UI's, as
    // it comes (third_party/swagger-ui), with its licence and its notice beside it
    static const QHash<QString, QByteArray> types {
        {QStringLiteral("index.html"), "text/html; charset=utf-8"},
        {QStringLiteral("swagger-ui.css"), "text/css"},
        {QStringLiteral("swagger-ui-bundle.js"), "text/javascript"},
        {QStringLiteral("swagger-ui-bundle.js.LICENSE.txt"), "text/plain; charset=utf-8"},
        {QStringLiteral("LICENSE"), "text/plain; charset=utf-8"},
        {QStringLiteral("NOTICE"), "text/plain; charset=utf-8"},
    };
    const auto found = types.constFind(name);
    if (found == types.constEnd())
        return {};
    return {QStringLiteral(":/apidoc/") + name, found.value()};
}

QString nameIn(const QString &folder, const QString &path, bool *redirect)
{
    *redirect = false;
    if (path == folder) {
        *redirect = true;
        return QStringLiteral("");
    }
    if (!path.startsWith(folder + u'/'))
        return {};
    const QString name = path.mid(folder.size() + 1);
    // (A name, and not a way to anywhere else)
    if (name.contains(u'/') || name.contains(QLatin1String("..")))
        return {};
    return name.isEmpty() ? QStringLiteral("") : name;
}

}
