#include "show.h"

#include "chords.h"

#include "proconvert.h"
#include "richtext.h"
#include "sessionlog.h"

#include <QJSValue>
#include <cstdio>

namespace {

show::Cue cueFrom(const QVariantMap &map)
{
    show::Cue cue;
    cue.key = map.value("key").toString();
    cue.playlistId = map.value("playlistId").toString();
    cue.presentation = map.value("presentation").toString();
    cue.index = map.value("index", -1).toInt();
    cue.count = map.value("count").toInt();
    cue.slide = map.value("slide").toMap();
    cue.next = map.value("next").toMap();
    return cue;
}

// The map QML handed over where it might have handed null instead
QVariantMap mapOf(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QJSValue>())
        return value.value<QJSValue>().toVariant().toMap();
    return value.toMap();
}

// A map, or null for an empty one: how QML is handed something that may not be there.
QVariant orNull(const QVariantMap &map)
{
    return map.isEmpty() ? QVariant::fromValue(nullptr) : QVariant(map);
}

}

void Show::change(const std::function<show::Effects()> &what)
{
    const show::State before = m_state;
    const show::Effects effects = what();
    // What is live first, so that whatever is told to show something finds it so.
    const bool slide = before.atSlide != m_state.atSlide || before.index != m_state.index || before.key != m_state.key
                    || before.playlistId != m_state.playlistId || before.cleared != m_state.cleared
                    || before.clearedByCue != m_state.clearedByCue;
    const bool words = slide || before.slide != m_state.slide || before.next != m_state.next;
    if (slide)
        emit slideChanged();
    if (before.hasMedia != m_state.hasMedia || before.media != m_state.media || before.mediaPlaylistId != m_state.mediaPlaylistId)
        emit mediaChanged();
    if (before.props != m_state.props)
        emit propsChanged();
    if (before.stageLayouts != m_state.stageLayouts)
        emit screenLayoutsChanged();
    if (before.lookId != m_state.lookId)
        emit lookChanged();
    if (words) {
        ++m_revision;
        emit changed();
    }
    for (const show::Effect &effect : effects) {
        switch (effect.kind) {
        case show::Effect::Slide:
            emit slideShown(Alone, {});
            break;
        case show::Effect::SlideOverMedia:
            emit slideShown(OverMedia, {});
            break;
        case show::Effect::SlideWithMedia:
            emit slideShown(WithMedia, effect.media);
            break;
        case show::Effect::NoSlide:
            emit slideCleared();
            break;
        case show::Effect::Media:
            emit mediaShown(effect.media);
            break;
        case show::Effect::NoMedia:
            emit mediaCleared();
            break;
        case show::Effect::Timer:
            emit timerAction(effect.action);
            break;
        case show::Effect::Look:
            emit lookAsked(effect.text);
            break;
        case show::Effect::Note:
            SessionLog::write(effect.topic.toLatin1().constData(), effect.text);
            break;
        case show::Effect::Problem:
            fprintf(stderr, "SimplePresenter: %s\n", qPrintable(effect.text));
            SessionLog::write("PROBLEM", effect.text);
            break;
        }
    }
}

QVariant Show::liveMedia() const
{
    return m_state.hasMedia ? QVariant(m_state.media) : QVariant::fromValue(nullptr);
}

QVariantMap Show::screenLayouts() const
{
    QVariantMap map;
    for (auto layout = m_state.stageLayouts.constBegin(); layout != m_state.stageLayouts.constEnd(); ++layout)
        map.insert(layout.key(), layout.value());
    return map;
}

QString Show::stageLayoutId() const
{
    return m_state.stageLayouts.value(show::stageScreen(m_workspace).value("id").toString());
}

void Show::setStageLayoutId(const QString &id)
{
    setStageLayout(show::stageScreen(m_workspace).value("id").toString(), id);
}

void Show::setStageLayout(const QString &screenId, const QString &layoutId)
{
    change([&] { m_state.setStageLayout(screenId, layoutId); return show::Effects(); });
}

void Show::setProps(const QVariantList &props)
{
    m_workspace.props = props;
    emit workspaceChanged();
    // A prop that is no longer there is no longer on.
    change([&] { m_state.dropMissingProps(m_workspace); return show::Effects(); });
}

void Show::setMacros(const QVariantList &macros)
{
    m_workspace.macros = macros;
    emit workspaceChanged();
}

void Show::setStageLayouts(const QVariantList &layouts)
{
    m_workspace.stageLayouts = layouts;
    emit workspaceChanged();
}

void Show::setStageScreens(const QVariantList &screens)
{
    m_workspace.stageScreens = screens;
    emit workspaceChanged();
    // Which screen is the first may have changed, and with it what stageLayoutId says.
    emit screenLayoutsChanged();
}

void Show::setLooks(const QVariantList &looks)
{
    m_workspace.looks = looks;
    emit workspaceChanged();
    // A look that is no longer there is no longer live.
    if (!m_state.lookId.isEmpty() && show::findLook(m_state.lookId, QString(), m_workspace).isEmpty())
        change([&] { m_state.lookId.clear(); return show::Effects(); });
}

void Show::setLookId(const QString &id)
{
    change([&] { return m_state.setLook(id, m_workspace); });
}

void Show::adoptLook(const QString &id)
{
    change([&] {
        m_state.adoptLook(show::findLook(id, QString(), m_workspace).isEmpty() ? QString() : id);
        return show::Effects();
    });
}

void Show::goLive(const QVariantMap &cue, bool withoutMedia)
{
    change([&] { return m_state.goLive(cueFrom(cue), withoutMedia, m_workspace); });
}

int Show::stepTarget(const QString &key, const QString &playlistId, int delta) const
{
    return m_state.stepTarget(key, playlistId, delta);
}

void Show::follow(const QVariantMap &cue)
{
    change([&] { m_state.follow(cueFrom(cue)); return show::Effects(); });
}

int Show::placeAfterReload(const QStringList &before, int index, const QStringList &after) const
{
    return show::placeAfterReload(before, index, after);
}

void Show::leavePresentation()
{
    change([&] { m_state.leavePresentation(); return show::Effects(); });
}

void Show::showMedia(const QVariantMap &media, const QString &playlist)
{
    change([&] { return m_state.showMedia(media, playlist); });
}

bool Show::alreadyPlaying(const QVariantMap &media) const
{
    return m_state.alreadyPlaying(media);
}

void Show::clearSlide()
{
    change([&] { return m_state.clearSlide(); });
}

void Show::clearMedia()
{
    change([&] { return m_state.clearMedia(); });
}

void Show::clearProps()
{
    change([&] { return m_state.clearProps(); });
}

void Show::clearAll()
{
    change([&] { return m_state.clearAll(); });
}

void Show::toggleProp(const QString &id)
{
    change([&] { return m_state.toggleProp(id, m_workspace); });
}

void Show::setProp(const QString &id, bool on)
{
    change([&] { return m_state.setProp(id, on, m_workspace); });
}

void Show::runMacro(const QString &id)
{
    change([&] { return m_state.runMacro(id, m_workspace); });
}

QVariant Show::propOf(const QVariantMap &action) const
{
    return orNull(show::propOf(action, m_workspace));
}

QVariant Show::stageLayoutOf(const QVariantMap &action) const
{
    return orNull(show::stageLayoutOf(action, m_workspace));
}

QVariantMap Show::stageScreen() const
{
    return show::stageScreen(m_workspace);
}

int Show::stageAssignmentOf(const QVariantMap &action) const
{
    return show::stageAssignmentOf(action, m_workspace);
}

QVariantList Show::stageAssignments(const QVariant &existing, const QVariant &layout) const
{
    return show::stageAssignments(mapOf(existing), mapOf(layout), m_workspace);
}

QVariantMap Show::stageLayoutsOf(const QVariantMap &action) const
{
    QVariantMap answer;
    const QMap<QString, QVariantMap> layouts = show::stageLayoutsOf(action, m_workspace);
    for (auto layout = layouts.constBegin(); layout != layouts.constEnd(); ++layout)
        answer.insert(layout.key(), orNull(layout.value()));
    return answer;
}

QVariantList Show::stageAssignmentsFor(const QVariant &existing, const QVariantMap &layouts) const
{
    QMap<QString, QVariantMap> chosen;
    for (auto layout = layouts.constBegin(); layout != layouts.constEnd(); ++layout)
        chosen.insert(layout.key(), mapOf(layout.value()));
    return show::stageAssignments(mapOf(existing), chosen, m_workspace);
}

void Show::setOriginalKey(const QString &key)
{
    if (key == m_originalKey)
        return;
    m_originalKey = key;
    // The words have not changed, but what is drawn over them has.
    ++m_revision;
    emit changed();
}

void Show::setChordKey(const QString &key)
{
    if (key == m_chordKey)
        return;
    m_chordKey = key;
    ++m_revision;
    emit changed();
}

QVariantList Show::chordLines(bool next, int source, const QString &name, int transform, int notation) const
{
    const QVariantMap slide = next ? nextSlide() : currentSlide();
    QVariantList lines;
    const auto plain = [&lines](const QString &text) {
        const QStringList parts = text.split(u'\n');
        for (const QString &part : parts)
            lines.append(QVariantMap {{"text", part}, {"chords", QVariantList()}});
    };
    if (source == Notes)
        return lines;
    if (transform != 0) {
        const QString text = slideText(next, source, name, transform);
        if (!text.isEmpty())
            plain(text);
        return lines;
    }
    // The same elements, in the same order, as the slide's words are made of (see
    // proconvert::toSlideMap), each with the chords it has of its own.
    const QVariantList elements = slide.value("elements").toList();
    for (const QVariant &entry : elements) {
        const QVariantMap element = entry.toMap();
        if (!element.value("visible").toBool())
            continue;
        const QString kind = element.value("linkKind").toString();
        if (source == ElementNamed ? element.value("name").toString().compare(name, Qt::CaseInsensitive) != 0
                                   : kind != QLatin1String("none"))
            continue;
        const QString shown = element.value("displayText").value<RichText>().plainText();
        // Chords belong to the element's own words: one that shows other words has none.
        const bool own = shown == element.value("text").value<RichText>().plainText();
        const QVariantList chordList = own ? element.value("chords").toList() : QVariantList();
        // No words is nothing to show, unless there are chords over the nothing: an
        // intro or a turnaround is a slide of chords alone, hung on spaces (see
        // chords::placeholders), and its chords are the whole point of it.
        if (shown.trimmed().isEmpty() && chordList.isEmpty())
            continue;
        int start = 0;
        QVariantList elementLines;
        const QStringList parts = shown.split(u'\n');
        for (const QString &part : parts) {
            QVariantList onLine;
            for (const QVariant &chord : chordList) {
                const QVariantMap one = chord.toMap();
                const int at = one.value("at").toInt();
                if (at < start || at >= start + qMax(qsizetype(1), part.size()))
                    continue;
                onLine.append(QVariantMap {
                    {"at", at - start},
                    {"name", chords::shown(one.value("name").toString(), m_originalKey, m_chordKey, notation)},
                });
            }
            elementLines.append(QVariantMap {{"text", part}, {"chords", onLine}});
            start += int(part.size()) + 1;
        }
        // As the words are trimmed: no empty lines before or after them.
        const auto empty = [](const QVariant &line) {
            const QVariantMap map = line.toMap();
            return map.value("text").toString().trimmed().isEmpty() && map.value("chords").toList().isEmpty();
        };
        while (!elementLines.isEmpty() && empty(elementLines.first()))
            elementLines.removeFirst();
        while (!elementLines.isEmpty() && empty(elementLines.last()))
            elementLines.removeLast();
        lines += elementLines;
    }
    return lines;
}

QString Show::slideText(bool next, int source, const QString &name, int transform) const
{
    const QVariantMap slide = next ? nextSlide() : currentSlide();
    QString text;
    if (source == Words) {
        text = slide.value("plainText").toString();
    } else if (source == ElementNamed) {
        // Of every element of that name that shows, whichever case the name is in
        QStringList texts;
        const QVariantList elements = slide.value("elements").toList();
        for (const QVariant &entry : elements) {
            const QVariantMap element = entry.toMap();
            if (!element.value("visible").toBool() || element.value("name").toString().compare(name, Qt::CaseInsensitive) != 0)
                continue;
            const QString words = element.value("displayText").value<RichText>().plainText().trimmed();
            if (!words.isEmpty())
                texts << words;
        }
        text = texts.join(u'\n');
    }
    return proconvert::linkTransformed(text, transform);
}
