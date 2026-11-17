#include "mcp.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace mcp {

namespace {

const QString kVersionKey = QStringLiteral("io.modelcontextprotocol/protocolVersion");
const QString kCapabilitiesKey = QStringLiteral("io.modelcontextprotocol/clientCapabilities");
const QString kServerInfoKey = QStringLiteral("io.modelcontextprotocol/serverInfo");

// JSON-RPC's own errors, and MCP's (its specification's Error Codes)
constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kHeaderMismatch = -32020;
constexpr int kUnsupportedVersion = -32022;

// How long a list of the tools may be kept: they are the same until the app is another
// version of itself
constexpr int kToolsKeptMs = 3600000;

QByteArray written(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject errorOf(const QJsonValue &id, int code, const QString &message, const QJsonValue &data = QJsonValue())
{
    QJsonObject error {{QStringLiteral("code"), code}, {QStringLiteral("message"), message}};
    if (!data.isUndefined() && !data.isNull())
        error.insert(QStringLiteral("data"), data);
    QJsonObject made {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("error"), error}};
    // (An id that could not be read is left out, as the specification has it.)
    if (id.isString() || id.isDouble())
        made.insert(QStringLiteral("id"), id);
    return made;
}

QJsonObject resultOf(const QJsonValue &id, const QJsonObject &result)
{
    return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), id}, {QStringLiteral("result"), result}};
}

QJsonObject serverInfo(const Server &server)
{
    return {{QStringLiteral("name"), server.name}, {QStringLiteral("title"), server.title}, {QStringLiteral("version"), server.version}};
}

QJsonArray versions(const QStringList &list)
{
    QJsonArray made;
    for (const QString &version : list)
        made.append(version);
    return made;
}

// One message answered: what is to be sent back (null for nothing), and for HTTP the
// status it goes with
struct Answered
{
    QJsonValue reply;
    int status = 200;
};

Answered toolCall(const QJsonValue &id, const QJsonObject &params, const Caller &call, bool modern, const QJsonObject &meta)
{
    const QString name = params.value(QStringLiteral("name")).toString();
    const extendedapi::Operation *operation = extendedapi::operation(name.toLatin1());
    // (A protocol error and not a tool's: there is nothing a model can put right about a
    // tool that is not there. The specification's own example.)
    if (!operation || !operation->tool)
        return {errorOf(id, kInvalidParams, QStringLiteral("Unknown tool: %1").arg(name)), modern ? 400 : 200};
    const QJsonValue given = params.value(QStringLiteral("arguments"));
    if (!given.isUndefined() && !given.isNull() && !given.isObject())
        return {errorOf(id, kInvalidParams, QStringLiteral("A tool's arguments are an object")), modern ? 400 : 200};

    bool theme = false;
    QVariantMap path;
    QVariantMap query;
    QVariantMap body;
    QString error;
    Called called;
    if (!sorted(*operation, given.toObject().toVariantMap(), &theme, &path, &query, &body, &error))
        called = {400, QVariantMap {{QStringLiteral("error"), error}}};
    else
        called = call(*operation, theme, path, query, body);

    QJsonObject result = QJsonObject::fromVariantMap(toolResult(called));
    if (modern) {
        result.insert(QStringLiteral("resultType"), QStringLiteral("complete"));
        result.insert(QStringLiteral("_meta"), meta);
    }
    return {resultOf(id, result), 200};
}

Answered answerOne(const QJsonObject &message, const Headers &headers, const Server &server, const Caller &call)
{
    const QJsonValue id = message.value(QStringLiteral("id"));
    const bool hasId = id.isString() || id.isDouble();
    const QJsonValue method = message.value(QStringLiteral("method"));
    if (message.value(QStringLiteral("jsonrpc")).toString() != QLatin1String("2.0") || !method.isString()) {
        // (An answer to something this never asked: taken and let go)
        if (message.contains(QStringLiteral("result")) || message.contains(QStringLiteral("error")))
            return {QJsonValue(), 202};
        return {errorOf(id, kInvalidRequest, QStringLiteral("Not a JSON-RPC 2.0 request")), 400};
    }
    const QString name = method.toString();
    // A notification wants no answer: that the client is ready, that it has given up on
    // a request (every request here is answered before the next is read)
    if (!hasId)
        return {QJsonValue(), 202};

    const QJsonObject params = message.value(QStringLiteral("params")).toObject();
    const QJsonObject meta = params.value(QStringLiteral("_meta")).toObject();
    const QStringList all = modernVersions() + legacyVersions();
    const QJsonObject unsupported {{QStringLiteral("supported"), versions(all)}};

    // ---- modern: the request says which revision it speaks
    if (meta.contains(kVersionKey)) {
        const QString asked = meta.value(kVersionKey).toString();
        if (!modernVersions().contains(asked)) {
            QJsonObject data = unsupported;
            data.insert(QStringLiteral("requested"), asked);
            return {errorOf(id, kUnsupportedVersion, QStringLiteral("Unsupported protocol version"), data), 400};
        }
        if (!meta.value(kCapabilitiesKey).isObject())
            return {errorOf(id, kInvalidParams, QStringLiteral("The request's _meta has no %1").arg(kCapabilitiesKey)), 400};
        // Over HTTP, what the headers say must be what the body says
        if (headers.http) {
            if (headers.protocolVersion.isEmpty() || headers.method.isEmpty())
                return {errorOf(id, kHeaderMismatch, QStringLiteral("Header mismatch: MCP-Protocol-Version and Mcp-Method are required")), 400};
            if (QString::fromLatin1(headers.protocolVersion) != asked)
                return {errorOf(id, kHeaderMismatch, QStringLiteral("Header mismatch: MCP-Protocol-Version header value '%1' does not match body value '%2'")
                                                         .arg(QString::fromLatin1(headers.protocolVersion), asked)), 400};
            if (QString::fromUtf8(headers.method) != name)
                return {errorOf(id, kHeaderMismatch, QStringLiteral("Header mismatch: Mcp-Method header value '%1' does not match body value '%2'")
                                                         .arg(QString::fromUtf8(headers.method), name)), 400};
            if (name == QLatin1String("tools/call")) {
                const QString tool = params.value(QStringLiteral("name")).toString();
                if (headers.name.isEmpty() || QString::fromUtf8(headerValue(headers.name)) != tool)
                    return {errorOf(id, kHeaderMismatch, QStringLiteral("Header mismatch: Mcp-Name header value '%1' does not match body value '%2'")
                                                             .arg(QString::fromUtf8(headerValue(headers.name)), tool)), 400};
            }
        }
        const QJsonObject said {{kServerInfoKey, serverInfo(server)}};
        if (name == QLatin1String("server/discover")) {
            return {resultOf(id, QJsonObject {
                {QStringLiteral("resultType"), QStringLiteral("complete")},
                {QStringLiteral("supportedVersions"), versions(all)},
                {QStringLiteral("capabilities"), QJsonObject {{QStringLiteral("tools"), QJsonObject()}}},
                {QStringLiteral("_meta"), said},
                {QStringLiteral("instructions"), instructions()},
                {QStringLiteral("ttlMs"), kToolsKeptMs},
                {QStringLiteral("cacheScope"), QStringLiteral("public")},
            }), 200};
        }
        if (name == QLatin1String("tools/list")) {
            return {resultOf(id, QJsonObject {
                {QStringLiteral("resultType"), QStringLiteral("complete")},
                {QStringLiteral("tools"), QJsonArray::fromVariantList(tools())},
                {QStringLiteral("ttlMs"), kToolsKeptMs},
                {QStringLiteral("cacheScope"), QStringLiteral("public")},
                {QStringLiteral("_meta"), said},
            }), 200};
        }
        if (name == QLatin1String("tools/call"))
            return toolCall(id, params, call, true, said);
        // (`initialize` among them: this revision has none. The versions spoken are
        // named, a legacy client having nothing else to go by.)
        return {errorOf(id, kMethodNotFound, QStringLiteral("Method not found: %1").arg(name), unsupported), 404};
    }

    // ---- legacy: a conversation that begins with `initialize`
    // (But not one whose header says a revision that has no such conversation: there
    // the body is to say the revision too, and one that does not is at odds with its
    // header. A GUESS at the error: the specification names this one for a header and
    // a body that differ, and says nothing of a body that says nothing.)
    if (headers.http && modernVersions().contains(QString::fromLatin1(headers.protocolVersion)))
        return {errorOf(id, kHeaderMismatch, QStringLiteral("Header mismatch: MCP-Protocol-Version header value '%1' is not in the request's _meta (%2)")
                                                 .arg(QString::fromLatin1(headers.protocolVersion), kVersionKey), unsupported), 400};
    if (headers.http && !headers.protocolVersion.isEmpty() && !all.contains(QString::fromLatin1(headers.protocolVersion))) {
        QJsonObject data = unsupported;
        data.insert(QStringLiteral("requested"), QString::fromLatin1(headers.protocolVersion));
        return {errorOf(id, kUnsupportedVersion, QStringLiteral("Unsupported protocol version"), data), 400};
    }
    if (name == QLatin1String("initialize")) {
        // The revision asked for, if it is one of these; otherwise the newest of them,
        // for the client to take or leave
        const QString asked = params.value(QStringLiteral("protocolVersion")).toString();
        return {resultOf(id, QJsonObject {
            {QStringLiteral("protocolVersion"), legacyVersions().contains(asked) ? asked : legacyVersions().first()},
            {QStringLiteral("capabilities"), QJsonObject {{QStringLiteral("tools"), QJsonObject {{QStringLiteral("listChanged"), false}}}}},
            {QStringLiteral("serverInfo"), serverInfo(server)},
            {QStringLiteral("instructions"), instructions()},
        }), 200};
    }
    if (name == QLatin1String("ping"))
        return {resultOf(id, QJsonObject()), 200};
    if (name == QLatin1String("tools/list"))
        return {resultOf(id, QJsonObject {{QStringLiteral("tools"), QJsonArray::fromVariantList(tools())}}), 200};
    if (name == QLatin1String("tools/call"))
        return toolCall(id, params, call, false, QJsonObject());
    // A modern method with none of the modern _meta is a modern request that is wrong
    if (name == QLatin1String("server/discover"))
        return {errorOf(id, kInvalidParams, QStringLiteral("The request's _meta has no %1").arg(kVersionKey), unsupported), 400};
    return {errorOf(id, kMethodNotFound, QStringLiteral("Method not found: %1").arg(name)), 200};
}

// Whether calling a tool twice with the same arguments leaves things as calling it once
// does, and whether it can only add to what there is: what its annotations say
bool additive(const extendedapi::Operation &operation)
{
    return operation.name.startsWith("create_") || operation.name.startsWith("add_") || operation.name.startsWith("duplicate_");
}

bool idempotent(const extendedapi::Operation &operation)
{
    // (format_text can step a size by so much, which adds up)
    return operation.kind == extendedapi::Operation::Read
        || (!additive(operation) && operation.name != "format_text");
}

}

QStringList modernVersions()
{
    return {QStringLiteral("2026-07-28")};
}

QStringList legacyVersions()
{
    return {QStringLiteral("2025-11-25"), QStringLiteral("2025-06-18"), QStringLiteral("2025-03-26"), QStringLiteral("2024-11-05")};
}

QString instructions()
{
    return QStringLiteral(
        "Simple Presenter runs a church's or an event's show from a ProPresenter 7 workspace: libraries of presentations (songs, "
        "sermons, announcements), playlists that order them for a service, and themes that say how slides look. These tools read and "
        "change that workspace as it is open in the app, at once.\n\n"
        "Begin with get_workspace. Find a presentation with search_presentations or list_presentations, and read it with "
        "get_presentation before changing it. A presentation is groups (Verse 1, Chorus) of slides; a slide has elements (text "
        "boxes, shapes, pictures). For a song, create_presentation with `groups` or with `lyrics`, and a `theme` to dress it in. "
        "A slide's words are changed with update_slide `text`; anything else on it with update_element, sending back properties "
        "as get_slide shows them. Lay a service out with create_playlist or set_playlist_items. get_reference explains every "
        "property and action.\n\n"
        "What is deleted goes to the computer's Trash, and a copy of a file is kept before it is first changed. A presentation "
        "that is open in the app's editor cannot be changed until the editor is closed. Songs' words are their writers': use what "
        "the user gives, and do not write out lyrics from memory.");
}

QVariantList tools()
{
    QVariantList made;
    for (const extendedapi::Operation &operation : extendedapi::operations()) {
        if (!operation.tool)
            continue;
        const bool reads = operation.kind == extendedapi::Operation::Read;
        made.append(QVariantMap {
            {QStringLiteral("name"), QString::fromLatin1(operation.name)},
            {QStringLiteral("title"), QString::fromUtf8(operation.title)},
            {QStringLiteral("description"), QString::fromUtf8(operation.description)},
            {QStringLiteral("inputSchema"), extendedapi::inputSchema(operation)},
            {QStringLiteral("annotations"), QVariantMap {
                {QStringLiteral("title"), QString::fromUtf8(operation.title)},
                {QStringLiteral("readOnlyHint"), reads},
                {QStringLiteral("destructiveHint"), !reads && !additive(operation)},
                {QStringLiteral("idempotentHint"), idempotent(operation)},
                // (What it works on is the one workspace, and nothing of the world outside)
                {QStringLiteral("openWorldHint"), false},
            }},
        });
    }
    return made;
}

bool sorted(const extendedapi::Operation &operation, const QVariantMap &arguments, bool *theme, QVariantMap *params,
            QVariantMap *query, QVariantMap *body, QString *error)
{
    QVariantMap rest = arguments;
    // A theme's slide, where the operation is one for either and it is `theme` that is
    // given
    *theme = false;
    if (!operation.themePattern.isEmpty()) {
        const bool hasTheme = !rest.value(QStringLiteral("theme")).toString().isEmpty();
        const bool hasPresentation = !rest.value(QStringLiteral("presentation")).toString().isEmpty();
        if (hasTheme == hasPresentation) {
            *error = QStringLiteral("%1 needs `presentation`, for a presentation's slide, or `theme`, for a theme's: one of them").arg(QString::fromLatin1(operation.name));
            return false;
        }
        *theme = hasTheme;
        if (hasTheme)
            params->insert(QStringLiteral("theme"), rest.take(QStringLiteral("theme")));
        else
            rest.remove(QStringLiteral("theme"));
    }
    for (const extendedapi::Field &field : operation.fields) {
        const QString name = QString::fromLatin1(field.name);
        if (field.where == extendedapi::Field::Body)
            continue;
        if (*theme && (name == QLatin1String("presentation") || name == QLatin1String("library"))) {
            rest.remove(name);
            continue;
        }
        if (!rest.contains(name) || rest.value(name).isNull()) {
            rest.remove(name);
            if (field.where == extendedapi::Field::Path || field.required) {
                *error = QStringLiteral("%1 needs `%2`: %3").arg(QString::fromLatin1(operation.name), name, QString::fromUtf8(field.description));
                return false;
            }
            continue;
        }
        // (A number given for what a path takes as text, a slide's or an element's)
        const QVariant value = rest.take(name);
        const QString text = value.typeId() == QMetaType::Double && value.toDouble() == qRound64(value.toDouble())
                                 ? QString::number(qRound64(value.toDouble())) : value.toString();
        if (field.where == extendedapi::Field::Path && text.isEmpty()) {
            *error = QStringLiteral("%1 needs `%2`: %3").arg(QString::fromLatin1(operation.name), name, QString::fromUtf8(field.description));
            return false;
        }
        (field.where == extendedapi::Field::Path ? params : query)->insert(name, text);
    }
    *body = rest;
    return true;
}

QVariantMap toolResult(const Called &called)
{
    const bool failed = called.status < 200 || called.status >= 300;
    QString text;
    const QVariantMap map = called.data.toMap();
    if (failed) {
        text = map.value(QStringLiteral("error")).toString();
        if (text.isEmpty())
            text = QStringLiteral("That could not be done (%1)").arg(called.status);
    } else if (map.size() == 1 && map.contains(QStringLiteral("reference"))) {
        // (The reference is text already, and reads better as itself)
        text = map.value(QStringLiteral("reference")).toString();
    } else if (!called.data.isValid() || called.data.isNull()) {
        text = QStringLiteral("Done.");
    } else {
        text = QString::fromUtf8(QJsonDocument::fromVariant(called.data).toJson(QJsonDocument::Compact));
    }
    return {{QStringLiteral("content"), QVariantList {QVariantMap {{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), text}}}},
            {QStringLiteral("isError"), failed}};
}

QByteArray headerValue(const QByteArray &value)
{
    if (value.startsWith("=?base64?") && value.endsWith("?=") && value.size() >= 11)
        return QByteArray::fromBase64(value.mid(9, value.size() - 11));
    return value;
}

Reply handle(const QByteArray &message, const Headers &headers, const Server &server, const Caller &call)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(message, &error);
    if (error.error != QJsonParseError::NoError || (!document.isObject() && !document.isArray()))
        return {400, written(errorOf(QJsonValue(), kParseError, QStringLiteral("Not JSON")))};
    if (document.isObject()) {
        const Answered answered = answerOne(document.object(), headers, server, call);
        if (!answered.reply.isObject())
            return {202, {}};
        return {answered.status, written(answered.reply.toObject())};
    }
    // Several at once, which the revisions before 2025-06-18 allowed: each answered, the
    // answers together
    QJsonArray replies;
    for (const QJsonValue &each : document.array()) {
        if (!each.isObject()) {
            replies.append(errorOf(QJsonValue(), kInvalidRequest, QStringLiteral("Not a JSON-RPC 2.0 request")));
            continue;
        }
        const Answered answered = answerOne(each.toObject(), headers, server, call);
        if (answered.reply.isObject())
            replies.append(answered.reply);
    }
    if (replies.isEmpty())
        return {202, {}};
    return {200, QJsonDocument(replies).toJson(QJsonDocument::Compact)};
}

}
