#pragma once

#include "httpapi.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// The API's pages of documentation: ProPresenter serves one at /v1/doc/index.html, a
// page that lists every request its API takes, says what each is for and lets it be
// tried from the page. The app serves the same page for what it answers of that API, at
// the same address, and another for its own extended API (extendedapi.h) at
// /sp/v1/doc/index.html. Plain rules, with nothing of the network or the windows in
// them (tests/unit/tst_httpapidoc); the server (httpapiserver.h) reads the files and
// sends them.
//
// How ProPresenter's page is made, seen of a ProPresenter 22 on 2026-11-17 by asking it
// for the page: it is Swagger UI, a library for a web page (github.com/swagger-api/
// swagger-ui, under the Apache licence), which draws an API from a description of it in
// OpenAPI's form. Beside index.html its folder has Swagger UI's own files and
// "swagger.json", which for all its name is not JSON but a line of JavaScript, `var
// openapi_spec = {…}`, that the page loads as a script. /v1/doc itself answers 302 to
// /v1/doc/index.html; /v1/doc/ and a file that is not there answer 404.
//
// What is the same here: the addresses, Swagger UI (its official release, in
// third_party/swagger-ui: none of ProPresenter's copy), swagger.json in that form, and
// the redirect. What is not, and must not be: the description. ProPresenter's is of
// all of ProPresenter's API, and its words are Renewed Vision's. This one is made here,
// from the table of routes the app answers (httpapi::routes()), so that the page lists
// what the app does and nothing it does not (the maintainer, 2026-11-17: "only document
// the API endpoints that we implemented"); and every word of it is the app's own, in
// summaries(). The headings requests are listed under are ProPresenter's own names for
// the parts of its API ("Announcement", "Global Groups"), which is how a person who
// knows its page finds a thing on this one.
//
// A route new to the table needs a line here: the unit test fails on one with none.
namespace httpapidoc {

// What a route does, in a sentence; "" for one of unanswered()
QString summary(const httpapi::Route &route);
// The heading it is listed under, which is ProPresenter's for that part of its API
QString tag(const httpapi::Route &route);
// The headings there are, in the order the page has them
QStringList tags();

// The routes that have a summary, each as "METHOD pattern": for the test that holds
// them to the table
QStringList summarised();

// Routes that are in the table and answer nothing but 404 (as "METHOD pattern"): they
// are there so that a client that asks for many streams at once is not refused for one
// the app has nothing behind (see httpapi::routes()), or wait on something the app does
// not do yet. They are left out of the description: it lists what is answered.
QStringList unanswered();

// ProPresenter's API as the app answers it, in OpenAPI's form: every route but those
// unanswered, with its summary, the parts of its path, what it takes in its query and
// its body, and its heading. `version` is the app's. `extendedPage` is the address of
// the extended API's page, to point to from this one's opening words, or "" while that
// API is switched off.
QVariantMap openApi(const QString &version, const QString &extendedPage);

// A description as the page loads it: "swagger.json", which is JavaScript that sets
// `openapi_spec` (see above)
QByteArray asScript(const QVariantMap &description);

// One of the page's files that is the same whoever asks
struct File
{
    // Where it is among the app's resources, "" if there is no such file; and what kind
    // of thing it is, as a response says it
    QString resource;
    QByteArray type;
    explicit operator bool() const { return !resource.isEmpty(); }
};
// The file of this name in the page's folder ("index.html", "swagger-ui.css"), but for
// swagger.json, which is made (asScript)
File file(const QString &name);

// The name a path asks for within a page's folder: "index.html" for
// "/v1/doc/index.html" under "/v1/doc". Null (QString()) if the path is not in the
// folder at all; "" for the folder itself with a slash after it. `*redirect` is set
// where it is the folder's own name, which is answered by sending the browser on to
// index.html.
QString nameIn(const QString &folder, const QString &path, bool *redirect);

}
