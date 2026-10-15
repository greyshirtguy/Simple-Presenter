#pragma once

#include "bible.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

// api.bible (https://api.bible), asked for a passage each time one is wanted: what to
// ask and how to read the answers. Plain functions, with no network in them
// (tests/unit/tst_apibible.cpp, on answers made up in the shape the service gives);
// src/bibles.h does the asking.
//
// Why it is asked each time. api.bible lets an app keep what it has fetched for a while
// (fewer than 500 verses running, cleared at least every 30 days); the maintainer chose
// to keep nothing (2026-10-15: "I would not worry about caching any parts and do the
// lookups live every time"). So nothing fetched is written anywhere by this app, but
// in a presentation someone saves of it, which is theirs to keep or not.
//
// What it asks, in its terms:
//   - With a key, in a header ("api-key"), that api.bible gives an app that is
//     registered with it. Its free plan is for non-commercial use, gives every Bible
//     that is public domain or under Creative Commons and up to three licensed ones
//     chosen when registering (NIV, NKJV, NLT and others; not ESV, which Crossway gives
//     through its own service), and a number of requests (5,000; a day on one page of
//     its documentation, a month on another).
//   - The copyright of what is shown, shown with it: the answer has it, and the app
//     puts it on the line along the bottom of the Bible view, and (where a theme has a
//     box for it) on the slides.
//   - No more than 500 verses running shown at once; a request gives at most 200, and
//     a passage longer than that is asked for in parts.
//   - FUMS, its way of counting for the publishers what is shown: with each answer comes
//     a token, which is sent back to fums.api.bible once its words have been shown, with
//     an id for the device and one for the session, neither of which says anything of
//     the person. (Required of web pages; a native app is to send it by hand.)
namespace apibible {

// Where api.bible is. (A build with the test hook can be pointed elsewhere, at a
// stand-in of the tests' own: see Bibles.)
QUrl baseUrl();

// One Bible api.bible gives with a key
struct Translation
{
    QString id;           // api.bible's id for it
    QString name;
    QString abbreviation;
    QString language;
    QString description;
};

// The Bibles a key gives, from the answer to GET /v1/bibles; empty, with *error set,
// for an answer that is not one
QList<Translation> readBibles(const QByteArray &json, QString *error);

// What to ask for a reference: one passage id for each stretch, or for each chapter of a
// stretch of whole chapters ("JHN.3.16-JHN.3.18", "PSA.23")
QStringList passageIds(const bible::Reference &reference);
// The address of a passage: content as JSON, so that verses can be told apart and words
// of Jesus found; no notes or headings; verse numbers in, for telling verses apart; and
// FUMS's token.
QUrl passageUrl(const QString &bibleId, const QString &passageId);

// A passage read from an answer: its verses in order, each with its words (the words of
// Jesus marked red), the copyright, and FUMS's token. False, with *error set, if it
// has none.
struct Passage
{
    QList<bible::PassageVerse> verses;
    QString copyright;
    QString fumsToken;
};
bool readPassage(const QByteArray &json, Passage *passage, QString *error);

// The verses of these passages that the reference names, in order, each once: a passage
// asked for as whole chapters is cut down to the verses wanted
QList<bible::PassageVerse> versesOf(const QList<Passage> &passages, const bible::Reference &reference);

// What an error answer says, for the person: api.bible's own words where it gives them
QString errorOf(int status, const QByteArray &json);

// The address FUMS is told at, for these tokens
QUrl fumsUrl(const QStringList &tokens, const QString &deviceId, const QString &sessionId);

}
