#include "groupedit.h"

namespace groupedit {

namespace {

// Makes a group the chosen one: its name, its colour, and which of the workspace's
// groups it is. Its own id and its hotkey are not touched.
void dress(rv::data::Group *group, const Choice &choice)
{
    group->set_name(choice.name.toStdString());
    bool isColor = choice.color.size() == 7 && choice.color.startsWith(u'#');
    int parts[3] = {0, 0, 0};
    for (int i = 0; isColor && i < 3; ++i)
        parts[i] = choice.color.mid(1 + 2 * i, 2).toInt(&isColor, 16);
    if (isColor) {
        rv::data::Color *color = group->mutable_color();
        color->set_red(float(parts[0] / 255.0));
        color->set_green(float(parts[1] / 255.0));
        color->set_blue(float(parts[2] / 255.0));
        color->set_alpha(1);
    } else {
        group->clear_color();
    }
    if (choice.id.isEmpty())
        group->clear_application_group_identifier();
    else
        group->mutable_application_group_identifier()->set_string(choice.id.toStdString());
    // (The name of the list's group is not written beside its id by ProPresenter, and
    // one left from before would be the wrong one.)
    group->clear_application_group_name();
}

} // namespace

Outcome setGroupFrom(rv::data::Presentation *presentation, const std::string &cueId, const Choice &choice,
                     const std::function<std::string()> &newId)
{
    for (int index = 0; index < presentation->cue_groups_size(); ++index) {
        rv::data::Presentation::CueGroup *entry = presentation->mutable_cue_groups(index);
        int place = -1;
        for (int i = 0; i < entry->cue_identifiers_size() && place < 0; ++i) {
            if (entry->cue_identifiers(i).string() == cueId)
                place = i;
        }
        if (place < 0)
            continue;
        if (QString::fromStdString(entry->group().name()).trimmed() == choice.name.trimmed())
            return Outcome::Unchanged;
        if (place == 0) {
            dress(entry->mutable_group(), choice);
            return Outcome::Whole;
        }

        // Split: the slides from this one on are taken out of their group and put in
        // a new one, which goes straight after it.
        const std::string oldId = entry->group().uuid().string();
        rv::data::Presentation::CueGroup tail;
        const std::string tailId = newId();
        tail.mutable_group()->mutable_uuid()->set_string(tailId);
        tail.mutable_group()->mutable_hotkey();
        dress(tail.mutable_group(), choice);
        for (int i = place; i < entry->cue_identifiers_size(); ++i)
            *tail.add_cue_identifiers() = entry->cue_identifiers(i);
        entry->mutable_cue_identifiers()->DeleteSubrange(place, entry->cue_identifiers_size() - place);
        // (`entry` is not to be used past here: adding to the list may move it.)
        *presentation->add_cue_groups() = tail;
        for (int i = presentation->cue_groups_size() - 1; i > index + 1; --i)
            presentation->mutable_cue_groups()->SwapElements(i, i - 1);

        // And after the old group wherever an arrangement has it (see the GUESS above).
        for (rv::data::Presentation::Arrangement &arrangement : *presentation->mutable_arrangements()) {
            for (int i = arrangement.group_identifiers_size() - 1; i >= 0; --i) {
                if (arrangement.group_identifiers(i).string() != oldId)
                    continue;
                arrangement.add_group_identifiers()->set_string(tailId);
                for (int j = arrangement.group_identifiers_size() - 1; j > i + 1; --j)
                    arrangement.mutable_group_identifiers()->SwapElements(j, j - 1);
            }
        }
        return Outcome::Split;
    }
    return Outcome::NoSuchSlide;
}

} // namespace groupedit
