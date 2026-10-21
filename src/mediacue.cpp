#include "mediacue.h"

namespace mediacue {

using Cue = rv::data::Cue;
using Transport = rv::data::Media::TransportProperties;

Next read(const Cue &cue)
{
    switch (cue.completion_target_type()) {
    case Cue::COMPLETION_TARGET_TYPE_NEXT:
        return {QStringLiteral("next"), {}};
    case Cue::COMPLETION_TARGET_TYPE_RANDOM:
        return {QStringLiteral("random"), {}};
    case Cue::COMPLETION_TARGET_TYPE_FIRST:
        return {QStringLiteral("first"), {}};
    case Cue::COMPLETION_TARGET_TYPE_CUE:
        // (A cue to go to that names none is nothing to go to.)
        if (!cue.completion_target_uuid().string().empty())
            return {QStringLiteral("cue"), QString::fromStdString(cue.completion_target_uuid().string())};
        return {};
    default:
        return {};
    }
}

void describe(const Cue &cue, QVariantMap *row)
{
    row->insert("cueId", QString::fromStdString(cue.uuid().string()));
    // (Only for a row that is followed by anything: the map is every row's.)
    const Next next = read(cue);
    if (next.kind.isEmpty())
        return;
    row->insert("next", next.kind);
    if (!next.target.isEmpty())
        row->insert("nextCue", next.target);
}

bool set(Cue *cue, rv::data::Action *media, const QString &kind, const QString &target)
{
    Cue::CompletionTargetType wanted;
    if (kind.isEmpty())
        wanted = Cue::COMPLETION_TARGET_TYPE_NONE;
    else if (kind == QLatin1String("next"))
        wanted = Cue::COMPLETION_TARGET_TYPE_NEXT;
    else if (kind == QLatin1String("random"))
        wanted = Cue::COMPLETION_TARGET_TYPE_RANDOM;
    else if (kind == QLatin1String("first"))
        wanted = Cue::COMPLETION_TARGET_TYPE_FIRST;
    else if (kind == QLatin1String("cue") && !target.isEmpty())
        wanted = Cue::COMPLETION_TARGET_TYPE_CUE;
    else
        return false;
    if (cue->completion_target_type() != wanted)
        cue->set_completion_target_type(wanted);
    // The cue it names, only for a cue (and none left behind by one that was)
    if (wanted == Cue::COMPLETION_TARGET_TYPE_CUE) {
        if (cue->completion_target_uuid().string() != target.toStdString())
            cue->mutable_completion_target_uuid()->set_string(target.toStdString());
    } else if (cue->has_completion_target_uuid()) {
        cue->clear_completion_target_uuid();
    }
    // A row with anything to follow it stops at its end, or it would never get there.
    // (Its going round is its media's, which is where workspace::mediaBehaviour reads
    // it; a count or a time that went with it is left, unused, as setMediaPlayback
    // leaves them.)
    if (wanted != Cue::COMPLETION_TARGET_TYPE_NONE && media && media->media().element().has_video()
        && media->media().element().video().transport().playback_behavior() != Transport::PLAYBACK_BEHAVIOR_STOP)
        media->mutable_media()->mutable_element()->mutable_video()->mutable_transport()->set_playback_behavior(Transport::PLAYBACK_BEHAVIOR_STOP);
    return true;
}

} // namespace mediacue
