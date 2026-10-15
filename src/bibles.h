#pragma once

#include "apibible.h"
#include "bible.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

// Bible mode's work, for the windows (qml/BiblePanel.qml): the Bibles there are to read
// from, a passage looked up in one, and slides made of it in a theme.
//
// THE BIBLES. Two kinds, side by side in one list, as the maintainer asked
// (2026-10-15: "importing an XML... and the live api.bible lookup"):
//   - Bibles the person has as XML files (biblexml.h), imported: a copy of the file
//     is kept with the app's own data, in Bibles/, beside a list of them (Bibles/
//     index.json: each file's name, the Bible's name and short name), so that the
//     list is there at the start without a file being read. A Bible is read when it is
//     first looked in, off the thread the windows run on, and kept while the app runs.
//   - The Bibles api.bible gives with the app's key (apibible.h), asked for the list
//     when Bible mode is first opened, and for each passage when it is looked up.
//     Nothing of a passage is kept but in the slides made of it (see below).
// The key is built in where the app was built with one (CMake's
// SIMPLEPRESENTER_API_BIBLE_KEY, or packaging/api-bible-key.txt, which git does not
// keep); a key of the person's own, typed in the settings, takes its place. With
// neither, only imported Bibles are offered, and the panel says why.
//
// A PASSAGE. lookup() reads what was typed (bible.h, with the Bible's own book names
// for a file in another language), finds or fetches its verses, and says what it found
// by found(): { request, translation, reference, verses, copyright, error }. step() is
// ProPresenter's Next Verse and Previous Verse, from a passage that was found.
//
// SLIDES. make() lays a passage that was found out on slides (bibleflow.h), each slide
// a copy of a theme slide with the passage's words in the text box chosen for them and
// its reference in the one chosen for that, or after the words, and writes them as a
// presentation into a folder of its own that goes when the app is closed. The window
// opens that presentation as it opens any, so that its slides are shown, clicked and
// stepped through as any are. saveAs() puts a copy of it into a library (ProPresenter's
// Save as Presentation, and with a playlist row after it, Save to Playlist); copyInto()
// puts its slides into another presentation after a slide of it (Copy to Current
// Presentation).
class Bibles : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Every Bible there is to read from: { id ("file:…" or "api:…"), name, abbreviation,
    // language, source ("file" or "api"), description }, imported ones first
    Q_PROPERTY(QVariantList translations READ translations NOTIFY translationsChanged)
    // Whether api.bible can be asked: a key built in, or one of the person's own
    Q_PROPERTY(bool hasKey READ hasKey NOTIFY keyChanged)
    Q_PROPERTY(bool builtInKey READ builtInKey CONSTANT)
    Q_PROPERTY(QString ownKey READ ownKey WRITE setOwnKey NOTIFY keyChanged)
    // What went wrong asking api.bible for its list of Bibles; "" for nothing
    Q_PROPERTY(QString apiProblem READ apiProblem NOTIFY translationsChanged)
    Q_PROPERTY(bool apiLoading READ apiLoading NOTIFY translationsChanged)
    // The workspace whose themes slides are made in
    Q_PROPERTY(QString workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged)

public:
    explicit Bibles(QObject *parent = nullptr);
    ~Bibles() override;

    QVariantList translations() const;
    bool hasKey() const { return !key().isEmpty(); }
    bool builtInKey() const;
    QString ownKey() const { return m_ownKey; }
    void setOwnKey(const QString &key);
    QString apiProblem() const { return m_apiProblem; }
    bool apiLoading() const { return m_apiLoading; }
    QString workspace() const { return m_workspace; }
    void setWorkspace(const QString &workspace);

    // An XML Bible imported: imported() says how it went, and with what id
    Q_INVOKABLE void importFile(const QUrl &file);
    // An imported Bible taken out again; what went wrong, or ""
    Q_INVOKABLE QString remove(const QString &id);
    // api.bible asked again for the Bibles the key gives
    Q_INVOKABLE void refreshApi();

    // The books of a Bible, for the Book menu: [{ number, name, chapters }]. A file's
    // own; for api.bible's, the 66 in English with their usual chapters.
    Q_INVOKABLE QVariantList books(const QString &id);
    // Looks a passage up; found() answers, with the number given here
    Q_INVOKABLE int lookup(const QString &id, const QString &typed);
    // The verse after (1) or before (-1) a passage that was found; found() answers
    Q_INVOKABLE int step(int request, int delta);

    // The text boxes of a theme slide, by name, in the order the slide has them
    Q_INVOKABLE QStringList textBoxes(const QString &place, const QString &slideId) const;
    // Slides of a passage that was found. `options`: { theme, themeSlide, verseBox,
    // referenceBox ("" for the reference after the words), verseNumbers, breakOnNewVerse,
    // translation (say which, with the reference), color (keep red letters), references
    // ("verse", "each", "last" or "none"), flow ("even" or "fill") }.
    // Answers { path, count, error }: the presentation they were written into.
    Q_INVOKABLE QVariantMap make(int request, const QVariantMap &options);
    // The slides last made, put into a library under the passage's name: { path, error }
    Q_INVOKABLE QVariantMap saveAs(const QString &library);
    // Or into a presentation, after one of its slides; what went wrong, or ""
    Q_INVOKABLE QString copyInto(const QString &presentation, const QString &afterSlideId);

signals:
    void translationsChanged();
    void keyChanged();
    void workspaceChanged();
    void imported(const QString &error, const QString &id);
    void found(int request, const QVariantMap &result);

private:
    struct Imported
    {
        QString file; // in Bibles/
        QString name;
        QString abbreviation;
    };
    // A passage that was found, kept for making slides of it and stepping from it
    struct Found
    {
        QString id;
        bible::Reference reference;
        QList<bible::PassageVerse> verses;
        QString copyright;
        QString abbreviation;
        QString bookName;
    };
    // A lookup on its way
    struct Pending
    {
        int request = 0;
        QString id;
        bible::Reference reference;
        QStringList ids;  // api.bible's passage ids still to come
        QList<apibible::Passage> passages;
        QString error;
        // Next Verse past a chapter's end, and Previous Verse from a chapter's first:
        // what to ask instead if this finds nothing, and to keep only the last verse
        bible::Reference instead;
        bool lastOnly = false;
    };

    QString key() const;
    QString folder() const;
    void readIndex();
    void writeIndex() const;
    // A file's Bible, read if it has not been: `ready` is called with it on this
    // thread, now or once it has been read on another
    void withBible(const QString &file, std::function<void(const bible::Bible *, const QString &error)> ready);
    void answer(int request, const QString &id, const bible::Reference &reference, QList<bible::PassageVerse> verses,
                const QString &copyright, const QString &error);
    void fetch(const std::shared_ptr<Pending> &pending);
    void fums(const QStringList &tokens);

    QList<Imported> m_imported;
    QList<apibible::Translation> m_api;
    QString m_apiProblem;
    bool m_apiLoading = false;
    bool m_apiAsked = false;
    QString m_ownKey;
    QString m_workspace;
    QHash<QString, std::shared_ptr<bible::Bible>> m_read;
    QHash<QString, QList<std::function<void(const bible::Bible *, const QString &)>>> m_reading;
    QHash<int, Found> m_found;
    int m_requests = 0;
    QNetworkAccessManager *m_network = nullptr;
    QString m_device;
    QString m_session;
    QTemporaryDir m_made;
    QString m_lastMade;
    QString m_lastName;
    int m_makes = 0;
};
