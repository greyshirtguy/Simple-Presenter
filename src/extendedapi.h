#pragma once

#include "httpapi.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// The extended API: this app's own addition to the HTTP API (httpapi.h), for making
// and changing what a workspace holds. ProPresenter's API operates a show and can make
// and change some of what goes with one (its playlists, looks, props, macros), but it
// has nothing for a presentation itself: its slides, their words and what is on them.
// This has, with the libraries, the playlists and the themes beside it, so that a
// program can set up a whole service: make the presentations, in a theme or not, put
// them in a playlist under headings, and change any of it later. (The maintainer,
// 2026-11-17: "our own extended API for CRUD operations for ALL
// documents/presentations", "a programmer can use the API to setup ANYTHING in a
// playlist", "and also an average user can use an LLM with the MCP to do the same".)
//
// It is the app's own and not ProPresenter's, so nothing of it is under ProPresenter's
// "/v1/": its paths begin "/sp/v1/", where no path of ProPresenter's, now or to come,
// can be in its way. And it is in its own words: lower-case names with underscores, as
// ProPresenter's JSON has them; colours as "#rrggbb" (or "#rrggbbaa", as a web page
// writes one that is partly see-through); an error as a status and {"error": why}.
//
// One table says what there is (operations(), below). Each operation is a method and a
// path, with what it is given in the path, the query and the body, and words for what it
// does. Three things are made of the table, so that they cannot differ:
//   - the routes the server answers (match());
//   - the description a programmer reads, GET /sp/v1/openapi.json (openApi()), in the
//     OpenAPI form that tools for trying and generating clients take;
//   - the tools an AI model is offered through MCP (mcp.h): one for each operation, its
//     name the operation's and its arguments the operation's fields (inputSchema()).
// What each operation does is in extendedapirun.h, which carries them out; the shapes
// of what is sent and answered are in extendedapijson.h; and the page for people is
// docs/features/extended-api.md, which the app also carries (GET /sp/v1/reference).
//
// Who may ask. ProPresenter's API answers anyone on the network, and any web page open
// in a browser on it, and this app's does the same for ProPresenter's paths. This part
// can change and remove what is in the workspace's files, so it is held closer
// (allowed(), below):
//   - It is off until it is switched on by itself (Settings, Network), and then answers
//     only programs on this computer, unless it is also opened to the network.
//   - A request that comes from a web page says so (its Origin). One from a page of
//     this computer's own is answered (served at one of its addresses or by its name:
//     the API's own page of documentation is one); with the network opened, one from a
//     page served by a computer of the local network too; one from any other page is
//     refused, so that a page somewhere on the internet, open in a browser here, cannot
//     reach it. A page that will not say where it is from ("null": one in a sandbox, a
//     form sent without its origin) is from anywhere, and is refused.
//   - The address it was asked at (its Host) must be one a local network could give
//     this computer: a number, "localhost", the computer's own name, or a name that
//     only a local network can give (one word, or one ending ".local", ".lan" and the
//     like, which no name of the internet's can). That is what stops a page from
//     having its own name point here (DNS rebinding), which MCP's specification asks
//     servers to guard against.
// A GUESS at what is wanted, and the app's own: ProPresenter has no such thing to go by.
namespace extendedapi {

// What begins every path of it ("/sp/v1/"), and the path MCP is served at
QString prefix();
QString mcpPath();

// One of the things an operation is given
struct Field
{
    enum Where { Path, Query, Body };

    QByteArray name;
    Where where = Body;
    // "string", "integer", "number", "boolean", "object", "array", or "any"
    QByteArray type;
    bool required = false;
    QByteArray description;
    // What an array holds ("string", "object", ...), for one that is of one kind
    QByteArray items;
    // The values it can be, for one that is one of a few words
    QList<QByteArray> choices;
};

struct Operation
{
    enum Kind {
        Read,    // answers, and changes nothing
        Change,  // makes or changes something
        Remove,  // takes something away
    };

    // What it is called: an MCP tool's name, and its operationId
    QByteArray name;
    QByteArray method;
    // Its path after the prefix, its parts in braces: "presentation/{presentation}/slides"
    QByteArray pattern;
    // A second path that is the same operation on a theme's slide, where a
    // presentation's slide and a theme's are changed alike; empty otherwise
    QByteArray themePattern;
    Kind kind = Read;
    // A few words, and all there is to say to whoever is to use it
    QByteArray title;
    QByteArray description;
    QList<Field> fields;
    // Whether an AI model is offered it through MCP (a few are for programs alone: the
    // OpenAPI description, and lists that get_workspace gives a model in one)
    bool tool = true;
};

// Every operation, in the order they are described in
const QList<Operation> &operations();
// The one of that name, or null
const Operation *operation(const QByteArray &name);

struct Match
{
    const Operation *operation = nullptr;
    // The parts of the path in braces, by name
    QVariantMap params;
    // Whether it was the operation's path for a theme's slide that fitted
    bool theme = false;
    explicit operator bool() const { return operation; }
};

// The operation a request is for: none if its path is not under the prefix, or no
// operation has that path and method. Where two would do, the one whose parts are words
// wins at the first place they differ, as with ProPresenter's routes.
//
// The path is taken as it came, its %-escapes still in it (httpapi::Request's rawPath),
// and each part's are undone after it is cut at its slashes: so a part may itself have
// a slash in it, written "%2F", which a theme in a folder has ("Samples/Black Box") and
// a name may have.
Match match(const QByteArray &method, const QString &rawPath);
// Whether a path is under the prefix or is MCP's: one of this app's own, held to
// allowed() whatever becomes of it
bool own(const QString &path);
// The methods a path under the prefix can be asked with, for a request made with
// another to be told (405, with them in `allow`); none for a path that is no operation's
QList<QByteArray> methodsOf(const QString &rawPath);

// The path of an operation with its parts filled in from `values` (each escaped), under
// the prefix: what MCP's tools and the app's tests ask by. `theme` picks the theme's
// path. Empty if a part has no value.
QString pathOf(const Operation &operation, const QVariantMap &values, bool theme = false);

// What an MCP tool for the operation takes: a JSON Schema of an object, its properties
// the operation's fields, in their order
QVariantMap inputSchema(const Operation &operation);
// The whole of it as an OpenAPI 3.1 document, for a server of that name and version
QVariantMap openApi(const QString &version);
// The part of the page of documentation an operation is listed in (httpapidoc.h), which
// is the reference's heading for it; and the parts there are, in the page's order
QString sectionOf(const Operation &operation);
QStringList sections();

// ---- who may ask (see above)

struct Asker
{
    // Whether the request came from this computer itself (a loopback address)
    bool local = false;
    // Its Origin and Host headers, as they came ("" for none)
    QByteArray origin;
    QByteArray host;
};

struct Access
{
    // Switched on at all, and opened to the network
    bool enabled = false;
    bool network = false;
    // The computer's own name, which a Host may be; and its own addresses on its
    // networks, as text, which a page of its own may have been served at
    QString machineName;
    QStringList ownAddresses;
};

// Why a request is not answered: empty if it is. (The server answers 404 while the
// extended API is off, as for a path there is nothing at, and 403 with the reason
// otherwise.)
QString refusal(const Asker &asker, const Access &access);
// Whether it is off altogether, which is answered as a path with nothing at it
bool off(const Access &access);
// Whether an Origin is one of this computer's own pages, or with the network opened one
// served from a computer of the local network; and whether a Host is one this computer
// could be asked for by: the two halves of refusal(), for the tests
bool originAllowed(const QByteArray &origin, const Access &access);
bool hostAllowed(const QByteArray &host, const QString &machineName);

}
