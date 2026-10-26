#include "decklink.h"

#include "sessionlog.h"

#include <DeckLinkAPI.h>
#include <DeckLinkAPIVideoOutput_v15_3_1.h>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickGraphicsDevice>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QTimer>

#include <cstdlib>
#include <vector>

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_PACK_ROW_LENGTH
#define GL_PACK_ROW_LENGTH 0x0D02
#endif

// decklinkformats.cpp writes the SDK's codes for its formats out itself, so that its
// rules can be built without the SDK; here they are held against the SDK's own.
static_assert(bmdModeHD1080p24 == 0x32347073 && bmdModeHD1080p25 == 0x48703235 && bmdModeHD1080p2997 == 0x48703239
              && bmdModeHD1080p30 == 0x48703330 && bmdModeHD1080p50 == 0x48703530 && bmdModeHD1080p5994 == 0x48703539
              && bmdModeHD1080p6000 == 0x48703630 && bmdModeHD720p50 == 0x68703530 && bmdModeHD720p5994 == 0x68703539
              && bmdModeHD720p60 == 0x68703630,
              "decklinkformats.cpp's codes are not the SDK's");

// Which of the SDK's interfaces are asked for. A new version of the SDK gives some of
// its interfaces new ids, and a card's driver answers only to the ids of its own version
// and the ones before: a driver older than the SDK does not know the newest. Asked for by
// SDK 16.0's ids, as this file did at first, Desktop Video 15.0 answered for none of its
// connectors, and no output was found at all. So the app asks for the ids of Desktop
// Video 14.3 to 15.3.1, which the SDK keeps (its files with _v15_3_1 in the name) so
// that programs built for them go on working with later drivers: every driver from 14.3
// on answers to them, 16.0's and later's included. Nothing this app does needs what 16.0
// changed. (The other interfaces used here, IDeckLink, IDeckLinkIterator,
// IDeckLinkAPIInformation, IDeckLinkKeyer and the frames, have kept their ids since 14.3.)
using DeckLinkAttributes = IDeckLinkProfileAttributes_v15_3_1;
using DeckLinkOutput = IDeckLinkOutput_v15_3_1;
using DeckLinkBuffer = IDeckLinkVideoBuffer_v15_3_1;
static const REFIID &iidAttributes = IID_IDeckLinkProfileAttributes_v15_3_1;
static const REFIID &iidOutput = IID_IDeckLinkOutput_v15_3_1;
static const REFIID &iidBuffer = IID_IDeckLinkVideoBuffer_v15_3_1;
// The oldest Desktop Video those ids are known to, as BMDDeckLinkAPIVersion writes it
constexpr int64_t oldestVersion = 0x0e030000;

namespace decklink {

// The card's side of a screen sent out through it: the output opened, frames of the
// card's own to draw into, and a frame handed to the card. open() and the writing are
// done on the screen's thread, display() on a thread of its own, since the card may keep
// it until the frame is shown.
class Card
{
public:
    virtual ~Card() = default;
    // Opens the output with this id (or failing that, of this name) in this format, keyed
    // or not; answers with what went wrong, or nothing.
    virtual QString open(const QString &id, const QString &name, const decklinkformats::Format &format, bool keyed, bool external,
                         int blend) = 0;
    // How many frames there are to take turns with, and how many bytes a row of one has
    virtual int frames() const = 0;
    virtual int rowBytes() const = 0;
    // A frame's bytes, to be written; and then to be read (for picture())
    virtual uchar *beginWriting(int frame) = 0;
    virtual void endWriting(int frame) = 0;
    virtual const uchar *bytes(int frame) = 0;
    // Hands a frame to the card, to be its next. Says whether the card took it.
    virtual bool display(int frame) = 0;
    virtual void close() = 0;
};

}

namespace {

using decklink::Card;
using decklinkformats::Format;

struct Output
{
    QString id;
    QString name;
    QString model;
    bool internal = false;
    bool external = false;
    QStringList formats;
    // Why a connector is not an output, for the log, when it is not
    QString left;
};

// What was found the last time the outputs were looked for. (One for the app, as the
// card is: every screen and the settings see the same.)
struct Found
{
    bool looked = false;
    bool available = false;
    QString version;
    QString problem;
    QList<Output> outputs;
};

Found &found()
{
    static Found kept;
    return kept;
}

bool pretending()
{
    return qEnvironmentVariableIsSet("SP_TEST_DECKLINK");
}

// A name the library hands over, which is the caller's to let go of
QString taken(const char *text)
{
    const QString made = QString::fromUtf8(text ? text : "");
    std::free(const_cast<char *>(text));
    return made;
}

// An output's id: the persistent id Desktop Video gives a connector, or failing that the
// place the card is in, or failing that its name
QString idOf(DeckLinkAttributes *attributes, const QString &name)
{
    int64_t value = 0;
    if (attributes->GetInt(BMDDeckLinkPersistentID, &value) == S_OK)
        return QStringLiteral("p") + QString::number(qulonglong(value), 16);
    if (attributes->GetInt(BMDDeckLinkTopologicalID, &value) == S_OK)
        return QStringLiteral("t") + QString::number(qulonglong(value), 16);
    return name;
}

// A card's device as an output, or one with no id if it is not one (and why, in left):
// it sends no video, or it is a connector a linked pair has taken (Desktop Video says it
// is inactive), or the driver did not answer for it.
Output describe(IDeckLink *deckLink)
{
    Output made;
    const char *text = nullptr;
    if (deckLink->GetDisplayName(&text) == S_OK)
        made.name = taken(text);
    if (deckLink->GetModelName(&text) == S_OK)
        made.model = taken(text);
    DeckLinkAttributes *attributes = nullptr;
    if (deckLink->QueryInterface(iidAttributes, reinterpret_cast<void **>(&attributes)) != S_OK) {
        made.left = QStringLiteral("Desktop Video would not say what it can do");
        return made;
    }
    int64_t io = 0;
    int64_t duplex = 0;
    const bool sends = attributes->GetInt(BMDDeckLinkVideoIOSupport, &io) == S_OK && (io & bmdDeviceSupportsPlayback);
    const bool inactive = attributes->GetInt(BMDDeckLinkDuplex, &duplex) == S_OK && duplex == bmdDuplexInactive;
    attributes->GetFlag(BMDDeckLinkSupportsInternalKeying, &made.internal);
    attributes->GetFlag(BMDDeckLinkSupportsExternalKeying, &made.external);
    const QString id = idOf(attributes, made.name);
    attributes->Release();
    if (!sends) {
        made.left = QStringLiteral("it does not send video");
        return made;
    }
    if (inactive) {
        made.left = QStringLiteral("Desktop Video has it inactive (taken by a linked pair, perhaps)");
        return made;
    }
    DeckLinkOutput *output = nullptr;
    if (deckLink->QueryInterface(iidOutput, reinterpret_cast<void **>(&output)) != S_OK) {
        made.left = QStringLiteral("Desktop Video would not give its output");
        return made;
    }
    for (const Format &format : decklinkformats::formats()) {
        bool supported = false;
        if (output->DoesSupportVideoMode(bmdVideoConnectionUnspecified, format.mode, bmdFormat8BitBGRA, bmdNoVideoOutputConversion,
                                         bmdSupportedVideoModeDefault, nullptr, &supported) == S_OK && supported)
            made.formats << format.name;
    }
    output->Release();
    made.id = id;
    return made;
}

void look()
{
    Found &kept = found();
    kept.looked = true;
    kept.outputs.clear();
    kept.problem.clear();
    if (pretending()) {
        QStringList all;
        for (const Format &format : decklinkformats::formats())
            all << format.name;
        kept.available = true;
        kept.version = QStringLiteral("(pretended, for the tests)");
        kept.outputs = {{QStringLiteral("pretend-1"), QStringLiteral("DeckLink Duo (1)"), QStringLiteral("DeckLink Duo 2"), true, true, all},
                        {QStringLiteral("pretend-3"), QStringLiteral("DeckLink Duo (3)"), QStringLiteral("DeckLink Duo 2"), false, false,
                         all.mid(0, 7)}};
        return;
    }
    // (The first of these is what finds Blackmagic's library: see DeckLinkAPIDispatch.cpp.)
    int64_t version = 0;
    if (IDeckLinkAPIInformation *information = CreateDeckLinkAPIInformationInstance()) {
        if (information->GetInt(BMDDeckLinkAPIVersion, &version) == S_OK)
            kept.version = QStringLiteral("%1.%2.%3").arg((version >> 24) & 0xff).arg((version >> 16) & 0xff).arg((version >> 8) & 0xff);
        else
            version = 0;
        information->Release();
    }
    if (version && version < oldestVersion) {
        kept.available = false;
        kept.problem = QStringLiteral("Blackmagic's Desktop Video here is version %1, and this needs 14.3 or later: it comes with the "
                                      "card's drivers, from Blackmagic Design.").arg(kept.version);
        SessionLog::write("decklink", QStringLiteral("no DeckLink outputs: %1").arg(kept.problem));
        return;
    }
    IDeckLinkIterator *iterator = CreateDeckLinkIteratorInstance();
    if (!iterator) {
        kept.available = false;
        kept.problem = QStringLiteral("Blackmagic's Desktop Video is not on this computer, or is older than version 14.3: it comes with the "
                                      "card's drivers, from Blackmagic Design.");
        SessionLog::write("decklink", QStringLiteral("no DeckLink outputs: %1").arg(kept.problem));
        return;
    }
    kept.available = true;
    IDeckLink *deckLink = nullptr;
    QStringList said;
    QStringList left;
    while (iterator->Next(&deckLink) == S_OK) {
        const Output output = describe(deckLink);
        deckLink->Release();
        if (output.id.isEmpty()) {
            left << QStringLiteral("\"%1\" (%2)").arg(output.name, output.left);
            continue;
        }
        kept.outputs << output;
        said << QStringLiteral("\"%1\" (%2; %3 formats; %4)").arg(output.name, output.model).arg(output.formats.size())
                    .arg(output.internal && output.external ? QStringLiteral("keys internally and externally")
                         : output.external ? QStringLiteral("keys externally") : output.internal ? QStringLiteral("keys internally")
                                                                                   : QStringLiteral("does not key"));
    }
    iterator->Release();
    SessionLog::write("decklink", QStringLiteral("Desktop Video %1: %2%3").arg(kept.version,
                                  said.isEmpty() ? QStringLiteral("no outputs found") : said.join(QStringLiteral("; ")),
                                  left.isEmpty() ? QString() : QStringLiteral("; left out: ") + left.join(QStringLiteral("; "))));
}

const Found &looked()
{
    if (!found().looked)
        look();
    return found();
}

// What the card answers, in words
QString said(HRESULT result)
{
    if (result == E_ACCESSDENIED)
        return QStringLiteral("it is in use by another program");
    if (result == E_OUTOFMEMORY)
        return QStringLiteral("the card had no room for it");
    if (result == E_INVALIDARG)
        return QStringLiteral("the card was asked for something it cannot do");
    return QStringLiteral("the card said no (%1)").arg(result);
}

// A card, through Blackmagic's library
class RealCard : public Card
{
public:
    ~RealCard() override { close(); }

    QString open(const QString &id, const QString &name, const Format &format, bool keyed, bool external, int blend) override
    {
        IDeckLinkIterator *iterator = CreateDeckLinkIteratorInstance();
        if (!iterator)
            return found().problem.isEmpty() ? QStringLiteral("Blackmagic's Desktop Video is not on this computer") : found().problem;
        // By its id, or failing that its name (the ids can change when Desktop Video
        // Setup arranges the connectors otherwise)
        IDeckLink *byName = nullptr;
        IDeckLink *deckLink = nullptr;
        while (iterator->Next(&deckLink) == S_OK) {
            const Output output = describe(deckLink);
            if (!output.id.isEmpty() && output.id == id) {
                if (byName)
                    byName->Release();
                byName = nullptr;
                m_deckLink = deckLink;
                break;
            }
            if (!output.id.isEmpty() && output.name == name && !byName)
                byName = deckLink;
            else
                deckLink->Release();
        }
        iterator->Release();
        if (!m_deckLink)
            m_deckLink = byName;
        if (!m_deckLink)
            return QStringLiteral("there is no Blackmagic output \"%1\" on this computer now").arg(name);
        if (m_deckLink->QueryInterface(iidOutput, reinterpret_cast<void **>(&m_output)) != S_OK)
            return QStringLiteral("\"%1\" does not send video").arg(name);
        bool supported = false;
        if (m_output->DoesSupportVideoMode(bmdVideoConnectionUnspecified, format.mode, bmdFormat8BitBGRA, bmdNoVideoOutputConversion,
                                           keyed ? bmdSupportedVideoModeKeying : bmdSupportedVideoModeDefault, nullptr, &supported) != S_OK
            || !supported)
            return QStringLiteral("\"%1\" cannot send %2%3").arg(name, format.name, keyed ? QStringLiteral(" keyed") : QString());
        HRESULT result = m_output->EnableVideoOutput(format.mode, bmdVideoOutputFlagDefault);
        if (result != S_OK)
            return QStringLiteral("\"%1\" could not be started: %2").arg(name, said(result));
        m_enabled = true;
        // Keyed: the card makes the key from the frames' opacity. (A GUESS at the order the
        // SDK wants: the keyer after the output is started, as its guide shows it.)
        if (keyed) {
            if (m_deckLink->QueryInterface(IID_IDeckLinkKeyer, reinterpret_cast<void **>(&m_keyer)) != S_OK)
                return QStringLiteral("\"%1\" cannot key").arg(name);
            result = m_keyer->Enable(external);
            if (result != S_OK)
                return QStringLiteral("\"%1\" would not key %2ly: %3").arg(name, external ? QStringLiteral("external") : QStringLiteral("internal"),
                                                                            said(result));
            m_keyer->SetLevel(uint8_t(qBound(0, blend, 100) * 255 / 100));
        }
        int32_t row = 0;
        if (m_output->RowBytesForPixelFormat(bmdFormat8BitBGRA, format.width, &row) != S_OK)
            row = format.width * 4;
        m_rowBytes = row;
        for (int i = 0; i < 3; ++i) {
            IDeckLinkMutableVideoFrame *frame = nullptr;
            result = m_output->CreateVideoFrame(format.width, format.height, row, bmdFormat8BitBGRA, bmdFrameFlagDefault, &frame);
            if (result != S_OK)
                return QStringLiteral("\"%1\" had no frames to give: %2").arg(name, said(result));
            DeckLinkBuffer *buffer = nullptr;
            if (frame->QueryInterface(iidBuffer, reinterpret_cast<void **>(&buffer)) != S_OK) {
                frame->Release();
                return QStringLiteral("\"%1\" gave a frame that cannot be drawn into").arg(name);
            }
            m_frames.push_back(frame);
            m_buffers.push_back(buffer);
        }
        return {};
    }

    int frames() const override { return int(m_frames.size()); }
    int rowBytes() const override { return m_rowBytes; }

    uchar *beginWriting(int frame) override
    {
        void *bytes = nullptr;
        if (m_buffers.at(frame)->StartAccess(bmdBufferAccessWrite) != S_OK || m_buffers.at(frame)->GetBytes(&bytes) != S_OK)
            return nullptr;
        return static_cast<uchar *>(bytes);
    }

    void endWriting(int frame) override { m_buffers.at(frame)->EndAccess(bmdBufferAccessWrite); }

    const uchar *bytes(int frame) override
    {
        void *bytes = nullptr;
        m_buffers.at(frame)->StartAccess(bmdBufferAccessRead);
        m_buffers.at(frame)->GetBytes(&bytes);
        m_buffers.at(frame)->EndAccess(bmdBufferAccessRead);
        return static_cast<const uchar *>(bytes);
    }

    bool display(int frame) override { return m_output->DisplayVideoFrameSync(m_frames.at(frame)) == S_OK; }

    void close() override
    {
        if (m_keyer) {
            m_keyer->Disable();
            m_keyer->Release();
            m_keyer = nullptr;
        }
        if (m_output && m_enabled)
            m_output->DisableVideoOutput();
        m_enabled = false;
        for (DeckLinkBuffer *buffer : m_buffers)
            buffer->Release();
        m_buffers.clear();
        for (IDeckLinkMutableVideoFrame *frame : m_frames)
            frame->Release();
        m_frames.clear();
        if (m_output)
            m_output->Release();
        m_output = nullptr;
        if (m_deckLink)
            m_deckLink->Release();
        m_deckLink = nullptr;
    }

private:
    IDeckLink *m_deckLink = nullptr;
    DeckLinkOutput *m_output = nullptr;
    IDeckLinkKeyer *m_keyer = nullptr;
    bool m_enabled = false;
    int m_rowBytes = 0;
    std::vector<IDeckLinkMutableVideoFrame *> m_frames;
    std::vector<DeckLinkBuffer *> m_buffers;
};

// The tests' card: frames in memory, taken as soon as they are handed over
class PretendCard : public Card
{
public:
    QString open(const QString &id, const QString &name, const Format &format, bool keyed, bool, int) override
    {
        const Output *output = nullptr;
        for (const Output &candidate : looked().outputs) {
            if (candidate.id == id || (!output && candidate.name == name))
                output = &candidate;
        }
        if (!output)
            return QStringLiteral("there is no Blackmagic output \"%1\" on this computer now").arg(name);
        if (!output->formats.contains(format.name) || (keyed && !output->internal && !output->external))
            return QStringLiteral("\"%1\" cannot send %2%3").arg(output->name, format.name, keyed ? QStringLiteral(" keyed") : QString());
        m_rowBytes = format.width * 4;
        for (auto &frame : m_frames)
            frame.assign(size_t(m_rowBytes) * size_t(format.height), 0);
        return {};
    }
    int frames() const override { return 3; }
    int rowBytes() const override { return m_rowBytes; }
    uchar *beginWriting(int frame) override { return m_frames[frame].data(); }
    void endWriting(int) override {}
    const uchar *bytes(int frame) override { return m_frames[frame].data(); }
    bool display(int) override { return true; }
    void close() override {}

private:
    int m_rowBytes = 0;
    std::vector<uchar> m_frames[3];
};

}

namespace decklink {

// Every screen going out through a card, drawn in the same passes (see decklink.h, and
// the beat in decklinkformats.h): one clock and one timer for them all. There is one of
// these, made the first time a screen goes out through a card.
class Passes : public QObject
{
public:
    static Passes &the()
    {
        static QPointer<Passes> made;
        if (!made)
            made = new Passes(QCoreApplication::instance());
        return *made;
    }

    qint64 now() const { return m_clock.nsecsElapsed(); }

    void join(DeckLinkScreen *screen)
    {
        if (!m_screens.contains(screen))
            m_screens << screen;
    }

    void leave(DeckLinkScreen *screen) { m_screens.removeAll(screen); }

    // A pass when the soonest of the screens waiting is due, unless one is coming sooner.
    // (Asked at every change of every screen, which is many a frame in a transition: it
    // only looks at the few screens there are.)
    void schedule()
    {
        QList<decklinkformats::Waiting> waiting;
        for (DeckLinkScreen *screen : std::as_const(m_screens)) {
            if (screen->waiting())
                waiting << screen->beat();
        }
        const qint64 due = decklinkformats::passDue(waiting);
        if (due < 0)
            return;
        const int wait = int(qMax<qint64>(0, due - now()) / 1'000'000);
        if (m_timer.isActive() && m_timer.remainingTime() <= wait)
            return;
        m_timer.start(wait);
    }

private:
    explicit Passes(QObject *parent)
        : QObject(parent)
    {
        m_timer.setSingleShot(true);
        m_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_timer, &QTimer::timeout, this, &Passes::pass);
        m_clock.start();
    }

    // Each screen the pass takes is drawn, then each is read back into a frame of its
    // card, then all are handed over at once.
    void pass()
    {
        const qint64 now = this->now();
        QList<decklinkformats::Waiting> waiting;
        for (DeckLinkScreen *screen : std::as_const(m_screens)) {
            if (screen->waiting())
                waiting << screen->beat();
        }
        // Not yet: it waits for a card to give a frame back (or the clock woke it early)
        if (decklinkformats::passDue(waiting) - now > 1'000'000) {
            schedule();
            return;
        }
        QList<DeckLinkScreen *> taken;
        for (DeckLinkScreen *screen : std::as_const(m_screens)) {
            if (screen->waiting() && decklinkformats::drawnInPass(screen->beat(), now))
                taken << screen;
        }
        if (!taken.isEmpty()) {
            ++m_passes;
            QElapsedTimer took;
            took.start();
            QList<DeckLinkScreen *> drawn;
            for (DeckLinkScreen *screen : std::as_const(taken)) {
                if (screen->render())
                    drawn << screen;
            }
            QList<int> frames;
            for (DeckLinkScreen *screen : std::as_const(drawn))
                frames << screen->readBack();
            const qint64 spent = took.nsecsElapsed();
            for (qsizetype i = 0; i < drawn.size(); ++i) {
                if (frames[i] >= 0)
                    drawn[i]->handOver(frames[i], m_passes, spent);
            }
            for (DeckLinkScreen *screen : std::as_const(taken))
                screen->m_next = decklinkformats::nextAfter(screen->beat(), now);
        }
        // Any still waiting: one not due yet, or one that changed again as it was drawn
        schedule();
    }

    QList<DeckLinkScreen *> m_screens;
    QTimer m_timer;
    QElapsedTimer m_clock;
    int m_passes = 0;
};

}

using decklink::Passes;

DeckLink::DeckLink(QObject *parent)
    : QObject(parent)
{
}

bool DeckLink::available() const
{
    return found().available;
}

QString DeckLink::version() const
{
    return found().version;
}

QString DeckLink::problem() const
{
    return found().problem;
}

bool DeckLink::looked() const
{
    return found().looked;
}

QVariantList DeckLink::outputs() const
{
    QVariantList list;
    for (const Output &output : found().outputs) {
        list << QVariantMap {{"id", output.id}, {"name", output.name}, {"model", output.model}, {"internal", output.internal},
                             {"external", output.external}, {"formats", output.formats}};
    }
    return list;
}

void DeckLink::refresh()
{
    look();
    emit changed();
}

DeckLinkScreen::DeckLinkScreen(QObject *parent)
    : QObject(parent)
{
}

DeckLinkScreen::~DeckLinkScreen()
{
    stop();
    // The scene goes first, while the window it is in is still there (see NdiScreen).
    delete m_scene.data();
    releaseDrawing();
}

double DeckLinkScreen::frameInterval() const
{
    const decklinkformats::Format *format = decklinkformats::formatNamed(m_format);
    return format ? decklinkformats::period(*format) / 1e6 : 1000.0 / 30;
}

void DeckLinkScreen::setOutput(const QString &output)
{
    if (m_output == output)
        return;
    m_output = output;
    emit settingsChanged();
    settleSoon();
}

void DeckLinkScreen::setFormat(const QString &format)
{
    if (m_format == format)
        return;
    m_format = format;
    if (const decklinkformats::Format *known = decklinkformats::formatNamed(format))
        m_size = QSize(known->width, known->height);
    fit();
    emit settingsChanged();
    settleSoon();
}

void DeckLinkScreen::setAlpha(const QString &alpha)
{
    if (m_alpha == alpha)
        return;
    m_alpha = alpha;
    emit settingsChanged();
    settleSoon();
}

void DeckLinkScreen::setKey(const QString &key)
{
    if (m_key == key)
        return;
    m_key = key;
    emit settingsChanged();
    settleSoon();
}

void DeckLinkScreen::setBlend(int blend)
{
    blend = qBound(0, blend, 100);
    if (m_blend == blend)
        return;
    m_blend = blend;
    emit settingsChanged();
    settleSoon();
}

void DeckLinkScreen::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();
    settleSoon();
}

void DeckLinkScreen::setScene(QQuickItem *scene)
{
    if (m_scene == scene)
        return;
    m_scene = scene;
    // In the window nobody sees from the start, sending or not, as an NDI screen's is
    if (m_scene && prepareDrawing()) {
        m_scene->setParentItem(m_window->contentItem());
        m_scene->setPosition({0, 0});
        m_scene->setSize(m_size);
    }
    emit sceneChanged();
    settleSoon();
}

void DeckLinkScreen::fail(const QString &why)
{
    if (m_problem == why)
        return;
    m_problem = why;
    if (!why.isEmpty())
        SessionLog::write("PROBLEM", QStringLiteral("A screen is not being sent through the Blackmagic output \"%1\": %2").arg(m_outputName, why));
    emit statusChanged();
}

// What it is sending as, in a few words: a change to any of it is a start again
static QString wanted(const DeckLinkScreen *screen)
{
    return QStringList {screen->output(), screen->format(), screen->alpha(), screen->key(), QString::number(screen->blend())}.join(u'|');
}

void DeckLinkScreen::componentComplete()
{
    m_complete = true;
    settle();
}

void DeckLinkScreen::settleSoon()
{
    if (m_settling)
        return;
    m_settling = true;
    QMetaObject::invokeMethod(this, [this] {
        m_settling = false;
        settle();
    }, Qt::QueuedConnection);
}

void DeckLinkScreen::settle()
{
    if (!m_complete)
        return;
    const bool want = m_active && m_scene && !m_output.isEmpty();
    if (m_sending && (!want || m_sendingAs != wanted(this)))
        stop();
    if (want && !m_sending)
        start();
    if (!want)
        fail(QString());
}

// The window nobody sees, and what draws it: NdiScreen's, again.
bool DeckLinkScreen::prepareDrawing()
{
    if (m_window)
        return true;
    if (QQuickWindow::sceneGraphBackend() == QLatin1String("software") || QGuiApplication::platformName() == QLatin1String("offscreen")) {
        fail(QStringLiteral("the app is drawing without the graphics card, and a screen cannot be drawn for a Blackmagic card that way."));
        return false;
    }
    m_control = new QQuickRenderControl(this);
    m_window = new QQuickWindow(m_control);
    m_window->setColor(Qt::black);
    m_context = new QOpenGLContext(this);
    m_context->setFormat(QSurfaceFormat::defaultFormat());
    if (QOpenGLContext *shared = QOpenGLContext::globalShareContext())
        m_context->setShareContext(shared);
    if (!m_context->create()) {
        fail(QStringLiteral("the graphics card would not give the app somewhere to draw it (no OpenGL context)."));
        releaseDrawing();
        return false;
    }
    m_window->setSurfaceType(QSurface::OpenGLSurface);
    m_window->setFormat(m_context->format());
    m_window->setGeometry(0, 0, m_size.width(), m_size.height());
    m_window->create();
    m_surface = m_window;
    if (!m_context->makeCurrent(m_surface)) {
        fail(QStringLiteral("the graphics card would not let the app draw out of sight."));
        releaseDrawing();
        return false;
    }
    m_window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(m_context));
    connect(m_control, &QQuickRenderControl::renderRequested, this, [this] {
        m_changed = true;
        schedule();
    });
    connect(m_control, &QQuickRenderControl::sceneChanged, this, [this] {
        m_changed = true;
        schedule();
    });
    return true;
}

void DeckLinkScreen::releaseDrawing()
{
    if (m_context && m_surface && m_context->isValid())
        m_context->makeCurrent(m_surface);
    if (m_window)
        m_window->setRenderTarget(QQuickRenderTarget());
    delete m_target;
    m_target = nullptr;
    if (m_scene && m_window && m_scene->parentItem() == m_window->contentItem())
        m_scene->setParentItem(nullptr);
    delete m_window;
    m_window = nullptr;
    delete m_control;
    m_control = nullptr;
    if (m_context)
        m_context->doneCurrent();
    m_surface = nullptr;
    delete m_context;
    m_context = nullptr;
    m_initialised = false;
}

void DeckLinkScreen::fit()
{
    if (m_window) {
        m_window->setGeometry(0, 0, m_size.width(), m_size.height());
        m_window->contentItem()->setSize(m_size);
    }
    if (m_scene)
        m_scene->setSize(m_size);
}

bool DeckLinkScreen::start()
{
    const decklinkformats::Format *format = decklinkformats::formatNamed(m_format);
    if (!format) {
        fail(QStringLiteral("\"%1\" is not a format the app sends").arg(m_format));
        return false;
    }
    if (!prepareDrawing())
        return false;
    if (QQuickWindow::graphicsApi() != QSGRendererInterface::OpenGL && QQuickWindow::graphicsApi() != QSGRendererInterface::Unknown) {
        fail(QStringLiteral("the app is drawing with something other than OpenGL, which is what a screen is drawn for a card with."));
        return false;
    }
    if (!m_context->makeCurrent(m_surface)) {
        fail(QStringLiteral("the graphics card would not let the app draw it."));
        return false;
    }
    if (!m_initialised) {
        if (!m_control->initialize()) {
            fail(QStringLiteral("the app's drawing could not be started for it."));
            return false;
        }
        m_initialised = true;
    }
    // Keyed, it is drawn on nothing, so that what is not drawn on is not there in the
    // key; otherwise on black, as any screen is.
    const bool keyed = m_alpha == QLatin1String("premultiplied") || m_alpha == QLatin1String("straight");
    m_window->setColor(keyed ? QColor(Qt::transparent) : QColor(Qt::black));
    // (The window lets go of the picture it drew into before that goes: see NdiScreen.)
    m_window->setRenderTarget(QQuickRenderTarget());
    delete m_target;
    m_target = new QOpenGLFramebufferObject(m_size, QOpenGLFramebufferObject::CombinedDepthStencil);
    if (!m_target->isValid()) {
        fail(QStringLiteral("the graphics card has no room for a picture of %1 x %2.").arg(m_size.width()).arg(m_size.height()));
        return false;
    }
    QQuickRenderTarget target = QQuickRenderTarget::fromOpenGLTexture(m_target->texture(), m_size);
    target.setMirrorVertically(true);
    m_window->setRenderTarget(target);
    fit();
    if (m_scene)
        m_scene->setParentItem(m_window->contentItem());

    // The card last, once there is something to send it
    looked();
    m_card = pretending() ? std::unique_ptr<decklink::Card>(new PretendCard) : std::unique_ptr<decklink::Card>(new RealCard);
    const QString error = m_card->open(m_output, m_outputName, *format, keyed, m_key != QLatin1String("internal"), m_blend);
    if (!error.isEmpty()) {
        m_card->close();
        m_card.reset();
        fail(error);
        return false;
    }

    m_free.clear();
    for (int i = 0; i < m_card->frames(); ++i)
        m_free << i;
    m_slotPass = QList<int>(m_card->frames(), -1);
    m_worker = new QObject;
    m_worker->moveToThread(&m_thread);
    m_thread.start();
    m_sending = true;
    ++m_run;
    m_sendingAs = wanted(this);
    m_sentName = m_outputName;
    m_sentSize = m_size;
    m_framesSent = 0;
    m_lastSlot = -1;
    m_changed = true;
    m_next = 0;
    m_period = decklinkformats::period(*format);
    m_drawings = 0;
    m_drawingTotal = 0;
    m_drawingLongest = 0;
    m_problem.clear();
    SessionLog::write("decklink", QStringLiteral("a screen goes out through \"%1\" as %2%3").arg(m_outputName, m_format,
                                  keyed ? QStringLiteral(", keyed %1ly, %2, at %3%").arg(m_key, m_alpha).arg(m_blend) : QString()));
    emit statusChanged();
    emit framesSentChanged();
    Passes::the().join(this);
    schedule();
    return true;
}

void DeckLinkScreen::stop()
{
    if (!m_sending)
        return;
    Passes::the().leave(this);
    // The thread that hands frames to the card finishes what it has first.
    m_thread.quit();
    m_thread.wait();
    delete m_worker;
    m_worker = nullptr;
    if (m_card)
        m_card->close();
    m_card.reset();
    m_free.clear();
    m_sending = false;
    m_lastSlot = -1;
    // (With how long drawing took, the other screens going out through a card with it:
    // what an older computer can keep up with is in that.)
    SessionLog::write("decklink", QStringLiteral("the screen sent through \"%1\" has stopped, after %2 %3%4").arg(m_sentName).arg(m_framesSent)
                                  .arg(m_framesSent == 1 ? QStringLiteral("frame") : QStringLiteral("frames"))
                                  .arg(m_drawings > 0 ? QStringLiteral(", each drawn (with any other screens going out through a card) in "
                                                                         "%1 ms on average, %2 ms at most")
                                                                .arg(double(m_drawingTotal) / m_drawings / 1e6, 0, 'f', 1)
                                                                .arg(double(m_drawingLongest) / 1e6, 0, 'f', 1)
                                                        : QString()));
    emit statusChanged();
}

bool DeckLinkScreen::waiting() const
{
    // (Not while what is wanted has changed and the card is about to be started again
    // for it: the size may no longer be the card's.)
    return m_sending && m_changed && m_sendingAs == wanted(this);
}

decklinkformats::Waiting DeckLinkScreen::beat() const
{
    return {m_next, m_period, !m_free.isEmpty()};
}

bool DeckLinkScreen::render()
{
    if (!m_context->makeCurrent(m_surface)) {
        m_changed = false;
        fail(QStringLiteral("the graphics card would not let the app draw it."));
        return false;
    }
    m_control->polishItems();
    m_control->beginFrame();
    m_control->sync();
    m_control->render();
    m_control->endFrame();
    m_changed = false;
    // Sent to the graphics chip now, to be drawn while the others are
    m_context->functions()->glFlush();
    return true;
}

int DeckLinkScreen::readBack()
{
    const int slot = m_free.takeFirst();
    // Read back straight into the card's own frame: blue, green, red, opacity, the
    // order the card takes them in. Premultiplied is how the app draws, so only straight
    // has anything done to it.
    uchar *bytes = m_context->makeCurrent(m_surface) ? m_card->beginWriting(slot) : nullptr;
    if (!bytes) {
        m_free.prepend(slot);
        return -1;
    }
    QOpenGLFunctions *gl = m_context->functions();
    gl->glBindFramebuffer(GL_FRAMEBUFFER, m_target->handle());
    gl->glPixelStorei(GL_PACK_ALIGNMENT, 4);
    if (m_card->rowBytes() != m_sentSize.width() * 4)
        gl->glPixelStorei(GL_PACK_ROW_LENGTH, m_card->rowBytes() / 4);
    gl->glReadPixels(0, 0, m_sentSize.width(), m_sentSize.height(), GL_BGRA, GL_UNSIGNED_BYTE, bytes);
    gl->glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    gl->glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (m_alpha == QLatin1String("straight")) {
        for (int row = 0; row < m_sentSize.height(); ++row)
            decklinkformats::unpremultiplied(bytes + qsizetype(row) * m_card->rowBytes(), m_sentSize.width());
    }
    m_card->endWriting(slot);
    return slot;
}

void DeckLinkScreen::handOver(int slot, int pass, qint64 took)
{
    m_slotPass[slot] = pass;
    ++m_drawings;
    m_drawingTotal += took;
    m_drawingLongest = qMax(m_drawingLongest, took);
    decklink::Card *card = m_card.get();
    QPointer<DeckLinkScreen> self(this);
    const int run = m_run;
    QMetaObject::invokeMethod(m_worker, [card, run, slot, self] {
        const bool ok = card->display(slot);
        if (self)
            QMetaObject::invokeMethod(self.data(), [self, run, slot, ok] {
                if (self)
                    self->shown(run, slot, ok);
            }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

void DeckLinkScreen::schedule()
{
    if (m_sending && m_changed)
        Passes::the().schedule();
}

void DeckLinkScreen::shown(int run, int slot, bool ok)
{
    if (!m_sending || run != m_run)
        return;
    if (m_lastSlot >= 0 && !m_free.contains(m_lastSlot))
        m_free << m_lastSlot;
    // (The frame shown last is kept out of the turns until the next is shown: it is what
    // picture() reads.)
    m_lastSlot = slot;
    if (ok) {
        ++m_framesSent;
        emit framesSentChanged();
        fail(QString());
    } else {
        fail(QStringLiteral("the card did not take a frame."));
    }
    // A change that waited for a frame to be free
    schedule();
}

int DeckLinkScreen::shownPass() const
{
    return m_lastSlot >= 0 && m_lastSlot < m_slotPass.size() ? m_slotPass[m_lastSlot] : -1;
}

QImage DeckLinkScreen::picture() const
{
    if (!m_card || m_lastSlot < 0)
        return QImage();
    // As the card was given it: the bytes taken as they are, opacity and all
    return QImage(m_card->bytes(m_lastSlot), m_sentSize.width(), m_sentSize.height(), m_card->rowBytes(), QImage::Format_ARGB32).copy();
}
