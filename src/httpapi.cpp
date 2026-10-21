#include "httpapi.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLocale>
#include <QUrl>
#include <QUuid>

#include <optional>

namespace httpapi {

namespace {

// "a%20b" -> "a b" ("+" is a plus: these are paths and ProPresenter's own parameters)
QString decoded(const QByteArray &text)
{
    return QUrl::fromPercentEncoding(text);
}

}

bool Request::hasQuery(const QString &name) const
{
    for (const auto &[key, value] : query) {
        if (key == name)
            return true;
    }
    return false;
}

QString Request::queryValue(const QString &name) const
{
    for (const auto &[key, value] : query) {
        if (key == name)
            return value;
    }
    return {};
}

bool Request::queryOn(const QString &name) const
{
    if (!hasQuery(name))
        return false;
    const QString value = queryValue(name).toLower();
    return value != QLatin1String("false") && value != QLatin1String("0");
}

bool Request::keepAlive() const
{
    const QByteArray connection = headers.value("connection").toLower();
    if (version == "HTTP/1.0")
        return connection.contains("keep-alive");
    return !connection.contains("close");
}

bool Parser::feed(const QByteArray &bytes)
{
    if (m_error)
        return false;
    m_buffer += bytes;
    while (parseOne()) {
    }
    return !m_error;
}

// One request off the front of what has come, if it is all there
bool Parser::parseOne()
{
    const qsizetype end = m_buffer.indexOf("\r\n\r\n");
    if (end < 0) {
        if (m_buffer.size() > maxHead)
            m_error = 431;
        return false;
    }
    if (end > maxHead) {
        m_error = 431;
        return false;
    }
    const QList<QByteArray> lines = m_buffer.left(end).split('\n');
    Request request;
    const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
    if (first.size() != 3 || !first[2].startsWith("HTTP/1.")) {
        m_error = 400;
        return false;
    }
    request.method = first[0];
    request.version = first[2];
    const QByteArray target = first[1];
    const qsizetype mark = target.indexOf('?');
    request.path = decoded(mark < 0 ? target : target.left(mark));
    if (mark >= 0) {
        for (const QByteArray &pair : target.mid(mark + 1).split('&')) {
            if (pair.isEmpty())
                continue;
            const qsizetype equals = pair.indexOf('=');
            request.query.append({decoded(equals < 0 ? pair : pair.left(equals)), equals < 0 ? QString() : decoded(pair.mid(equals + 1))});
        }
    }
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines[i].trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0) {
            m_error = 400;
            return false;
        }
        request.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }
    // (A body comes with a length; one sent chunked is not taken: no client of the API sends one.)
    if (request.headers.contains("transfer-encoding")) {
        m_error = 400;
        return false;
    }
    bool ok = true;
    const qint64 length = request.headers.contains("content-length") ? request.headers.value("content-length").toLongLong(&ok) : 0;
    if (!ok || length < 0) {
        m_error = 400;
        return false;
    }
    if (length > maxBody) {
        m_error = 413;
        return false;
    }
    if (m_buffer.size() < end + 4 + length)
        return false;
    request.body = m_buffer.mid(end + 4, length);
    m_buffer.remove(0, end + 4 + length);
    m_ready.append(request);
    return true;
}

QByteArray reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 413: return "Payload Too Large";
    case 431: return "Request Header Fields Too Large";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    }
    return "Unknown";
}

QByteArray httpDate(const QDateTime &when)
{
    return QLocale::c().toString(when.toUTC(), QStringLiteral("ddd, dd MMM yyyy hh:mm:ss")).toLatin1() + " GMT";
}

Headers common(const Request &request)
{
    Headers headers {{"vary", "Origin, Access-Control-Request-Method, Access-Control-Request-Headers"}};
    if (request.headers.contains("origin"))
        headers.append({"access-control-allow-origin", request.headers.value("origin")});
    return headers;
}

QByteArray head(int status, const Headers &headers)
{
    QByteArray made = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason(status) + "\r\n";
    for (const auto &[name, value] : headers)
        made += name + ": " + value + "\r\n";
    return made + "\r\n";
}

QByteArray json(const QVariant &value, bool separated)
{
    // (QJsonDocument writes objects and arrays only: a bare string, number or true is
    // written as the one element of an array, and the brackets taken off.)
    const QJsonValue made = QJsonValue::fromVariant(value);
    QByteArray text;
    if (made.isObject())
        text = QJsonDocument(made.toObject()).toJson(QJsonDocument::Compact);
    else if (made.isArray())
        text = QJsonDocument(made.toArray()).toJson(QJsonDocument::Compact);
    else
        text = QJsonDocument(QJsonArray {made}).toJson(QJsonDocument::Compact).mid(1).chopped(1);
    return separated ? text + "\r\n\r\n" : text;
}

QByteArray chunk(const QByteArray &data)
{
    return QByteArray::number(data.size(), 16) + "\r\n" + data + "\r\n";
}

QByteArray lastChunk()
{
    return "0\r\n\r\n";
}

const QList<Route> &routes()
{
    // Streams: every GET ProPresenter's documentation gives `chunked` to (48 of them),
    // each answered or not by what the app has, so that a client asking for many at once
    // in status/updates is not refused for one the app has nothing behind. The rest:
    // what reads and operates the show (the maintainer's first round, 2026-10-17); what
    // makes, changes or deletes things in ProPresenter's files, and thumbnails, are not
    // here yet.
    static const QList<Route> all = [] {
        QList<Route> made;
        auto get = [&made](const QByteArray &pattern) { made.append({"GET", QString::fromLatin1(pattern)}); };
        auto stream = [&made](const QByteArray &pattern) { made.append({"GET", QString::fromLatin1(pattern), true}); };
        auto clocked = [&made](const QByteArray &pattern) { made.append({"GET", QString::fromLatin1(pattern), true, true}); };
        auto put = [&made](const QByteArray &pattern) { made.append({"PUT", QString::fromLatin1(pattern)}); };
        auto post = [&made](const QByteArray &pattern) { made.append({"POST", QString::fromLatin1(pattern)}); };
        auto remove = [&made](const QByteArray &pattern) { made.append({"DELETE", QString::fromLatin1(pattern)}); };

        get("version");
        // Status
        stream("status/layers");
        stream("status/slide");
        stream("status/screens");
        stream("status/audience_screens");
        put("status/audience_screens");
        stream("status/stage_screens");
        put("status/stage_screens");
        post("status/updates");
        // Presentations
        stream("presentation/active");
        stream("presentation/focused");
        stream("presentation/slide_index");
        clocked("presentation/active/timeline");
        clocked("presentation/focused/timeline");
        stream("presentation/chord_chart/updates");
        get("presentation/{uuid}");
        for (const char *which : {"active", "focused", "{uuid}"}) {
            const QByteArray at = QByteArray("presentation/") + which;
            get(at + "/trigger");
            get(at + "/next/trigger");
            get(at + "/previous/trigger");
            get(at + "/{index}/trigger");
            get(at + "/group/{group_id}/trigger");
            if (qstrcmp(which, "focused") != 0)
                get(at + "/focus");
        }
        get("presentation/next/focus");
        get("presentation/previous/focus");
        // Triggers
        get("trigger/next");
        get("trigger/previous");
        get("trigger/media/next");
        get("trigger/media/previous");
        get("trigger/audio/next");
        get("trigger/audio/previous");
        // Libraries
        get("libraries");
        stream("library/{library_id}");
        get("library/{library_id}/{presentation_id}/trigger");
        get("library/{library_id}/{presentation_id}/{index}/trigger");
        // Playlists
        stream("playlists");
        get("playlist/{playlist_id}");
        stream("playlist/{playlist_id}/updates");
        stream("playlist/active");
        stream("playlist/focused");
        get("playlist/focused/next");
        get("playlist/focused/previous");
        clocked("playlist/focused/{index}/timeline");
        for (const char *which : {"focused", "{playlist_id}"}) {
            const QByteArray at = QByteArray("playlist/") + which;
            get(at + "/trigger");
            get(at + "/next/trigger");
            get(at + "/previous/trigger");
            get(at + "/{index}/trigger");
            get(at + "/{index}/{cue_index}/trigger");
        }
        get("playlist/{playlist_id}/focus");
        get("playlist/next/focus");
        get("playlist/previous/focus");
        for (const char *destination : {"presentation", "announcement"}) {
            const QByteArray at = QByteArray("playlist/active/") + destination;
            get(at + "/focus");
            get(at + "/trigger");
            get(at + "/{index}/trigger");
            get(at + "/{index}/{cue_index}/trigger");
            clocked(at + "/{index}/timeline");
        }
        // Announcements
        stream("announcement/active");
        stream("announcement/slide_index");
        clocked("announcement/active/timeline");
        get("announcement/active/trigger");
        get("announcement/active/next/trigger");
        get("announcement/active/previous/trigger");
        get("announcement/active/{index}/trigger");
        get("announcement/active/focus");
        // Clearing
        get("clear/layer/{layer}");
        get("clear/groups");
        get("clear/group/{id}");
        get("clear/group/{id}/trigger");
        // Global groups
        get("groups");
        // Looks
        stream("looks");
        stream("look/current");
        get("look/{id}");
        get("look/{id}/trigger");
        // Macros
        stream("macros");
        get("macro/{id}");
        get("macro/{id}/trigger");
        stream("macro_collections");
        get("macro_collection/{id}");
        // Masks
        get("masks");
        get("mask/{id}");
        // Media playlists, and audio's
        for (const char *kind : {"media", "audio"}) {
            const QByteArray at = QByteArray(kind) + "/playlist";
            stream(QByteArray(kind) + "/playlists");
            get(at + "/{playlist_id}");
            stream(at + "/{playlist_id}/updates");
            stream(at + "/active");
            stream(at + "/focused");
            for (const char *which : {"active", "focused", "{playlist_id}"}) {
                get(at + '/' + which + "/trigger");
                get(at + '/' + which + "/next/trigger");
                get(at + '/' + which + "/previous/trigger");
                get(at + '/' + which + (qstrcmp(kind, "media") == 0 ? "/{media_id}/trigger" : "/{id}/trigger"));
            }
            get(at + "/{playlist_id}/focus");
            get(at + "/active/focus");
            get(at + "/next/focus");
            get(at + "/previous/focus");
        }
        // Messages
        stream("messages");
        get("message/{id}");
        post("message/{id}/trigger");
        get("message/{id}/clear");
        // Props
        stream("props");
        get("prop/{id}");
        get("prop/{id}/trigger");
        get("prop/{id}/clear");
        stream("prop_collections");
        get("prop_collection/{id}");
        // Stage
        stream("stage/screens");
        stream("stage/layouts");
        stream("stage/layout_map");
        put("stage/layout_map");
        get("stage/screen/{id}/layout");
        get("stage/screen/{id}/layout/{layout_id}");
        stream("stage/message");
        put("stage/message");
        remove("stage/message");
        // Timers and clocks
        stream("timers");
        clocked("timers/current");
        get("timers/{operation}");
        get("timer/{id}");
        get("timer/{id}/{operation}");
        get("timer/{id}/increment/{time}");
        clocked("timer/system_time");
        clocked("timer/video_countdown");
        // Transport: what plays on a layer
        stream("transport/{layer}/current");
        clocked("transport/{layer}/time");
        put("transport/{layer}/time");
        get("transport/{layer}/play");
        get("transport/{layer}/pause");
        get("transport/{layer}/skip_forward/{time}");
        get("transport/{layer}/skip_backward/{time}");
        get("transport/{layer}/go_to_end");
        get("transport/{layer}/auto_advance");
        remove("transport/{layer}/auto_advance");
        // What the app has nothing of yet, but which a client may stream with the rest
        clocked("capture/status");
        // Video inputs
        get("video_inputs");
        get("video_inputs/{id}/trigger");
        return made;
    }();
    return all;
}

namespace {

QStringList parts(const QString &path)
{
    return path.split(u'/', Qt::SkipEmptyParts);
}

// How well a route's pattern fits these parts: null if it does not; otherwise which of
// its parts are words, first first (true beats false at the first difference)
std::optional<QList<bool>> fit(const QStringList &pattern, const QStringList &path, QVariantMap *params)
{
    if (pattern.size() != path.size())
        return std::nullopt;
    QList<bool> words;
    QVariantMap found;
    for (qsizetype i = 0; i < pattern.size(); ++i) {
        const QString &part = pattern[i];
        if (part.startsWith(u'{') && part.endsWith(u'}')) {
            found.insert(part.mid(1, part.size() - 2), path[i]);
            words.append(false);
        } else if (part == path[i]) {
            words.append(true);
        } else {
            return std::nullopt;
        }
    }
    if (params)
        *params = found;
    return words;
}

Match best(const QByteArray &method, const QStringList &path, bool streamsOnly)
{
    Match made;
    QList<bool> bestWords;
    for (const Route &route : routes()) {
        if (route.method != method || (streamsOnly && !route.streams))
            continue;
        QVariantMap params;
        const auto words = fit(parts(route.pattern), path, &params);
        if (!words)
            continue;
        if (!made.route || *words > bestWords) {
            made.route = &route;
            made.params = params;
            bestWords = *words;
        }
    }
    return made;
}

}

Match match(const QByteArray &method, const QString &path)
{
    if (path == QLatin1String("/version"))
        return best(method, {QStringLiteral("version")}, false);
    if (!path.startsWith(QLatin1String("/v1/")))
        return {};
    const QStringList asked = parts(path.mid(4));
    // ("version" is only ever /version)
    if (asked == QStringList {QStringLiteral("version")})
        return {};
    return best(method, asked, false);
}

Match updatesPath(const QString &path)
{
    // Bare paths only, as ProPresenter takes them: not "/v1/...", and no query
    if (path.startsWith(u'/') || path.startsWith(QLatin1String("v1/")) || path.contains(u'?') || path.contains(u'#'))
        return {};
    const QString asked = path == QLatin1String("presentation/current") ? QStringLiteral("presentation/active") : path;
    return best("GET", parts(asked), true);
}

bool updatesPaths(const QByteArray &body, QStringList *paths)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return false;
    QStringList found;
    for (const QJsonValue &value : document.array()) {
        if (!value.isString() || !updatesPath(value.toString()))
            return false;
        found.append(value.toString());
    }
    if (paths)
        *paths = found;
    return true;
}

int resolve(const QList<std::pair<QString, QString>> &items, const QString &id)
{
    if (id.isEmpty())
        return -1;
    // (UUIDs are matched without regard to case or braces.)
    const QUuid uuid = QUuid::fromString(id);
    if (!uuid.isNull()) {
        for (qsizetype i = 0; i < items.size(); ++i) {
            if (QUuid::fromString(items[i].first) == uuid)
                return int(i);
        }
    }
    for (qsizetype i = 0; i < items.size(); ++i) {
        if (items[i].second == id)
            return int(i);
    }
    bool number = false;
    const int index = id.toInt(&number);
    if (number && index >= 0 && index < items.size())
        return index;
    return -1;
}

}
