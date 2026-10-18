#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <utility>

// The rules of the HTTP API (see httpapiserver.h for the server), in plain C++ with
// nothing of the network or the windows in it: how a request is read, how a response
// and its streamed chunks are written, and which route a path is.
//
// The API is ProPresenter's own, as ProPresenter 7 vends it on the network for clients
// on the same network (Bitfocus Companion's module, a phone, a script): its paths, its
// JSON, and its ways of streaming. Its specification is the documentation ProPresenter
// serves itself (at /v1/doc), and what is written here that the specification does not
// say was seen of a ProPresenter 21.4 on 2026-10-17, by asking it for state:
//
//  - A JSON body is followed by CR LF CR LF, which its content-length counts: "so a
//    client can parse out complete JSON responses if they are split across chunks"
//    (the maintainer). Every v1 response does it; /version does not.
//  - Any GET that takes `chunked` (true) streams instead: the connection stays open,
//    the body is chunked, and each chunk is one whole response, sent at first and then
//    each time it would be different.
//  - POST /v1/status/updates streams several at once: the body asked with is a JSON
//    array of the paths of such GETs, bare ("status/slide", not "/v1/status/slide"), and
//    each chunk is {"url": the path as asked, "data": its response}. ProPresenter
//    sends "timer/system_time" in it every second whatever was asked for, and answers a
//    path it does not stream, or a body that is not such an array, with 400 and nothing.
//    "presentation/current" is another name for "presentation/active" (its
//    documentation lists the one and links to the other).
//  - Anything it does not know (a path, a method, an id) is 404 with an empty body.
//  - CORS, for clients in a browser: every response says `vary: Origin, ...`, and one to
//    a request with an Origin gives that origin back as allowed. A preflight (OPTIONS) is
//    answered 200 with the methods and the headers asked for.
//  - Header names are written in lower case, as it writes them. (HTTP does not mind.)
//
// Not done, and why: the TCP/IP form of the API (the maintainer: HTTP alone is fine);
// server-sent events (`sse`), the same.
//
// JSON is written by Qt, which puts an object's keys in alphabetical order and writes a
// whole number with no ".0" (ProPresenter writes 1.0 and its own key order): the same
// JSON to anything that reads it as JSON.
namespace httpapi {

struct Request
{
    QByteArray method;
    // The path, its %-escapes undone, without the query
    QString path;
    QList<std::pair<QString, QString>> query;
    // By name, in lower case
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
    QByteArray version;

    bool hasQuery(const QString &name) const;
    QString queryValue(const QString &name) const;
    // Whether the connection stays open after the response (HTTP/1.1 unless it says
    // close; 1.0 only if it says keep-alive)
    bool keepAlive() const;
    // A query switch such as chunked: there, and not "false" or "0"
    bool queryOn(const QString &name) const;
};

// The most a request may be, head and body: more is refused (431, 413) and the
// connection closed. (Nothing the API takes comes near.)
constexpr int maxHead = 16 * 1024;
constexpr int maxBody = 1024 * 1024;

// Takes what a client sends, as it comes, and gives back each request once it is whole.
class Parser
{
public:
    // Adds bytes. False once what has come cannot be read as HTTP; error() then says
    // with which status to answer (400, 413, 431), and nothing more is read.
    bool feed(const QByteArray &bytes);
    bool hasRequest() const { return !m_ready.isEmpty(); }
    Request take() { return m_ready.takeFirst(); }
    int error() const { return m_error; }

private:
    bool parseOne();

    QByteArray m_buffer;
    QList<Request> m_ready;
    int m_error = 0;
};

using Headers = QList<std::pair<QByteArray, QByteArray>>;

// The words of a status, "OK" for 200
QByteArray reason(int status);
// An HTTP date ("Sat, 17 Oct 2026 12:37:56 GMT")
QByteArray httpDate(const QDateTime &when);
// The headers every response has: vary, and the origin asked from if there was one
Headers common(const Request &request);
// A response's status line and headers, and the blank line after them
QByteArray head(int status, const Headers &headers);

// A JSON value written compactly, followed by CR LF CR LF if `separated`
QByteArray json(const QVariant &value, bool separated);
// One chunk of a chunked body, and the last (empty) one
QByteArray chunk(const QByteArray &data);
QByteArray lastChunk();

// A route: ProPresenter's path, without "/v1/", with its parts in braces
// ("presentation/{uuid}/{index}/trigger"; "version" for "/version"), and the method.
struct Route
{
    QByteArray method;
    QString pattern;
    // It takes `chunked`, and may be asked for in status/updates
    bool streams = false;
    // What it says changes with time alone (a clock, a timer, a playing video), so a
    // stream of it is looked at again often, and not only when the show changes
    bool clocked = false;
};

// Every route answered (anything else is 404)
const QList<Route> &routes();

struct Match
{
    const Route *route = nullptr;
    // The parts in braces, by name
    QVariantMap params;
    explicit operator bool() const { return route; }
};

// The route a request is for. Where two would do, the one whose parts are words wins
// at the first place they differ ("presentation/active/trigger" over
// "presentation/{uuid}/trigger").
Match match(const QByteArray &method, const QString &path);
// One of the paths asked for in status/updates, as the route it streams; none if it is
// not one that streams
Match updatesPath(const QString &path);
// The paths asked for in a status/updates body: false if it is not a JSON array of
// strings, each a path that streams
bool updatesPaths(const QByteArray &body, QStringList *paths);

// Which of these (each its UUID and its name) an id in a path is: by UUID, then by
// name, then by index, ProPresenter's "order of interpretation and search priority";
// -1 if none
int resolve(const QList<std::pair<QString, QString>> &items, const QString &id);

}
