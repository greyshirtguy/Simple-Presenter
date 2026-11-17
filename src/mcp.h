#pragma once

#include "extendedapi.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

// MCP, the Model Context Protocol, served on top of the extended API (extendedapi.h):
// the way an AI model's program (Claude, an editor's assistant, a command line agent)
// is told what tools a server has and calls them. Here the tools are the extended
// API's operations, one each, by the same names, taking the same things: so that
// whatever a programmer can set up in a workspace through the API, a person can ask a
// model to set up, in their own words. (The maintainer, 2026-11-17: "an average user
// can use an LLM with the MCP to do the same".)
//
// This is the protocol alone, in plain C++ with nothing of the network or the windows
// in it (tests/unit/tst_mcp): a message in, a message out, and a function it is given
// to carry a tool out with. Two things carry messages to it:
//   - the HTTP API's server, at /mcp on its port (MCP's "Streamable HTTP": each message
//     a POST, each answer one JSON object), for programs that are given an address;
//   - the app started as `SimplePresenter --mcp` (mcpstdio.h), which a program starts
//     itself and talks to over its standard input and output (MCP's "stdio"), and which
//     passes each tool's call to the running app's API.
//
// Which MCP. The protocol has revisions, named by dates, and as this was written
// (2026-11-17) its current one, 2026-07-28, had just changed how a conversation
// begins, so both ways are answered here (the specification calls that "dual-era"):
//   - 2026-07-28 ("modern"): no beginning. Every request says in its `_meta` which
//     revision it speaks and what the client can do, and is answered by itself. A
//     server says what it is in answer to `server/discover`. Every result says its
//     `resultType`, and a list says how long it may be kept. Over HTTP, a request's
//     method and a tool's name are sent as headers too (Mcp-Method, Mcp-Name) and must
//     agree with its body; and what is wrong with a request has an HTTP status as well
//     as a JSON-RPC error (400, or 404 for a method that is not known).
//   - 2025-11-25 and earlier ("legacy": 2025-06-18, 2025-03-26, 2024-11-05): a
//     conversation begins with `initialize`, which settles the revision, and the client
//     then says `notifications/initialized`. A request with none of the modern `_meta`
//     is taken to be one of these.
// The specification is at modelcontextprotocol.io/specification; what is done here was
// read from its pages for 2026-07-28 and 2025-06-18.
//
// What is served of it: tools, and nothing else. No resources, prompts, sampling,
// logging or subscriptions (the list of tools never changes while the app runs), no
// sessions, and no streams: every answer is one message. A method that is not served
// is answered as unknown.
//
// A tool's result is text: the operation's answer as JSON, which is what a model
// reads; or, for an operation that did not go through, the reason, marked as an error
// (`isError`), so that the model can put it right and try again.
namespace mcp {

// The revisions spoken, the newest first: the modern ones and the legacy ones
QStringList modernVersions();
QStringList legacyVersions();

// What the server says it is
struct Server
{
    QString name = QStringLiteral("simplepresenter");
    QString title = QStringLiteral("Simple Presenter");
    QString version;
};

// What is said to a model about using the tools, once, as a conversation begins
QString instructions();

// What a tool was carried out with: the status and the answer of the operation
struct Called
{
    int status = 500;
    QVariant data;
};
// Carries a tool out: an operation, whether by its path for a theme's slide, and what it
// is given in its path, its query and its body
using Caller = std::function<Called(const extendedapi::Operation &operation, bool theme, const QVariantMap &params,
                                    const QVariantMap &query, const QVariantMap &body)>;

// The headers of an HTTP request that MCP reads; `http` false for a message that came
// by standard input, which has none
struct Headers
{
    bool http = false;
    QByteArray protocolVersion;
    QByteArray method;
    QByteArray name;
};

struct Reply
{
    // For HTTP: 200 with the body; 202 with none, for what wants no answer (a
    // notification); 400 or 404 with a JSON-RPC error as the body
    int status = 200;
    // One JSON object (or, answering a legacy batch, a list of them) on one line, with
    // no line break in it, as standard output must have it; empty for no answer
    QByteArray body;
};

// Answers a message: one JSON-RPC request or notification (or, from a legacy client, a
// list of them)
Reply handle(const QByteArray &message, const Headers &headers, const Server &server, const Caller &call);

// The tools, as `tools/list` gives them: one for each operation that is offered as a
// tool, in the table's order, each { name, title, description, inputSchema,
// annotations }
QVariantList tools();

// A tool's arguments sorted into what its operation takes in its path, its query and
// its body; `*theme` whether it is the operation on a theme's slide (which it is when
// `theme` is given and `presentation` is not). False, with *error, when a part of the
// path has nothing given for it.
bool sorted(const extendedapi::Operation &operation, const QVariantMap &arguments, bool *theme, QVariantMap *params,
            QVariantMap *query, QVariantMap *body, QString *error);

// A tool's result, of what its operation answered: { content: [{ type: "text", text }],
// isError }
QVariantMap toolResult(const Called &called);

// A value sent in a header that could not be sent as it is, read back: MCP's
// "=?base64?…?=" (a tool's name with a letter outside ASCII in it), or the value as it is
QByteArray headerValue(const QByteArray &value);

}
