#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <utility>

// The rules of ProPresenter Remote's connection (Renewed Vision's phone and tablet app),
// in plain C++ with nothing of the network or the windows in it: the key a password
// makes, how a message is sealed and opened, the WebSocket it all goes over, and what a
// message asks. The server that uses them is HttpApi (src/httpapiserver.h); the routes
// it answers are the HTTP API's own (src/httpapi.h).
//
// What ProPresenter does, as Renewed Vision describes it (pasted by the maintainer on
// 2026-11-13) and as was seen of a ProPresenter 22 that day:
//
//  - It advertises itself on the network by DNS-SD (Bonjour) as a service of the type
//    `_proapiv1ws._tcp`, named with its Network Name, at the API's port, with the text
//    record `path=/v1/ws`. (See dnssd.h for how that is done here.)
//  - The Remote opens a WebSocket at that path on the API's port: an ordinary HTTP
//    request upgraded (RFC 6455), answered "101 Switching Protocols" with the API's
//    usual vary and origin headers.
//  - Every message, both ways, is AES-256 in CTR mode, under a key that is the
//    SHA-512/256 hash of the Remote's password as set in ProPresenter's Network
//    settings; a random 16-byte IV goes in front of each. A text frame carries that in
//    base64, a binary frame as it is, and an answer comes in the kind of frame its
//    request came in.
//  - On connecting, ProPresenter sends one message at once: its answer to /version
//    ({"data": ..., "url": "version"}), so that the Remote can tell whether its password
//    opens it. A message that does not open (CTR has no check of its own: one that is not
//    a JSON object once opened) is not answered at all.
//  - A message is the JSON of ProPresenter's TCP/IP API: {"url", "method", "body",
//    "chunked"}, the url a path of the HTTP API without its leading slash ("v1/looks",
//    and "version" bare), a query on it if it likes; method GET unless there is a body,
//    then POST; the body JSON itself, not a string of it. An answer is {"url": the url as
//    it was asked, query and all, "data": ...}, or {"url", "error": "404 Not Found"}
//    (the status and its words), and nothing at all for a 204. The Remote marks each
//    request with ?id=<a UUID> and a queryKey of its own, which come back in the url.
//  - A request with `chunked` (the member, or chunked=true in its query, which is how the
//    Remote asks) for a path that streams is answered at once and then again whenever
//    its answer would be different, each time with the same url; for one that does not
//    stream, "400 Bad Request".
namespace proremote {

// The key a password makes: the SHA-512/256 hash of its UTF-8 (32 bytes, for AES-256)
QByteArray key(const QString &password);
// A message sealed under a key: a 16-byte IV (random, unless one is given, for a test),
// then the bytes encrypted with AES-256 in CTR mode from it
QByteArray seal(const QByteArray &key, const QByteArray &plain, const QByteArray &iv = {});
// The bytes a sealed message opens to under a key (empty if it is too short to be one).
// CTR opens anything: whether it was the right key is whether what comes out makes sense.
QByteArray open(const QByteArray &key, const QByteArray &sealed);

// The WebSocket (RFC 6455)

// Whether an HTTP request asks to be a WebSocket: Upgrade: websocket, Connection:
// upgrade, a key, and version 13. (The headers by name in lower case.)
struct Upgrade
{
    QByteArray upgrade;
    QByteArray connection;
    QByteArray key;
    QByteArray version;
};
bool isUpgrade(const Upgrade &asked);
// The Sec-WebSocket-Accept that answers a Sec-WebSocket-Key
QByteArray acceptKey(const QByteArray &clientKey);

enum Opcode { Continuation = 0, Text = 1, Binary = 2, Close = 8, Ping = 9, Pong = 10 };

// One frame, whole: the server's unmasked; a client's masked with the four bytes given
// (only a test, playing the client, gives them)
QByteArray frame(Opcode opcode, const QByteArray &payload, const QByteArray &mask = {});

// What a peer sends, read as it comes: each message once it is whole (a text or binary
// message put together from its fragments) and each control frame as it comes.
class FrameReader
{
public:
    struct Message
    {
        Opcode opcode = Text;
        QByteArray payload;
    };
    // The most a message may be: more is refused (1009, "too big") and nothing more read
    static constexpr qsizetype most = 1024 * 1024;

    // Adds bytes. False once what has come cannot be read as frames; error() then says
    // with which close code (1002, 1009), and nothing more is read.
    bool feed(const QByteArray &bytes);
    bool hasMessage() const { return !m_ready.isEmpty(); }
    Message take() { return m_ready.takeFirst(); }
    int error() const { return m_error; }

private:
    bool readOne();

    QByteArray m_buffer;
    QList<Message> m_ready;
    // A message whose first fragment has come and whose last has not
    bool m_open = false;
    Message m_partial;
    int m_error = 0;
};

// A message asked: what ProPresenter's TCP/IP API reads of its JSON
struct Request
{
    // The url as it came, for the answer to give back
    QString url;
    QByteArray method;
    // The path as the HTTP API has it ("/v1/looks", "/version"), its %-escapes undone,
    // and the query, its escapes undone
    QString path;
    QList<std::pair<QString, QString>> query;
    // The body, as JSON (a string's JSON for a string: "\"Hello\"")
    QByteArray body;
    // Whether it asks for a stream: the member, or chunked=true in its query
    bool chunked = false;
};
// The request a message opens to; false if it is not a JSON object with a url
bool read(const QByteArray &json, Request *request);

// An answer's JSON: {"data": ..., "url": ...}, the data already JSON
QByteArray answer(const QString &url, const QByteArray &dataJson);
// A failure's: {"error": "404 Not Found", "url": ...}
QByteArray failure(const QString &url, const QByteArray &statusText);

}
