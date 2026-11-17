#include "presentationedit.h"

#include "arrangementedit.h"
#include "ccli.h"
#include "chords.h"
#include "extendedapijson.h"
#include "groupedit.h"
#include "labelfile.h"
#include "proconvert.h"
#include "slideedit.h"
#include "workspacefiles.h"

#include <QHash>

namespace presentationedit {

namespace {

using workspace::newUuid;

const QString slideGone = QStringLiteral("The presentation has no such slide");

// The group the workspace's own list has by that name, as a slide's menu would choose
// it: its colour and its id in the list. One the list has not is its name alone, with
// `color` if one was given.
groupedit::Choice choiceOf(const QString &name, const QString &color, const Context &context)
{
    groupedit::Choice choice {name.trimmed(), color, QString()};
    for (const QVariant &each : context.groups) {
        const QVariantMap listed = each.toMap();
        if (listed.value(QStringLiteral("name")).toString().trimmed().compare(choice.name, Qt::CaseInsensitive) != 0)
            continue;
        choice.name = listed.value(QStringLiteral("name")).toString();
        choice.id = listed.value(QStringLiteral("id")).toString();
        if (choice.color.isEmpty())
            choice.color = listed.value(QStringLiteral("color")).toString();
        break;
    }
    return choice;
}

// A colour as groupedit and labelfile take one: "#rrggbb", or "" for none. False if
// the text is no colour.
bool plainColor(const QVariant &value, QString *color)
{
    if (value.toString().trimmed().isEmpty()) {
        color->clear();
        return true;
    }
    QColor read;
    if (!extendedapijson::colorOf(value, &read))
        return false;
    *color = read.name(QColor::HexRgb);
    return true;
}

// Where new cues go: their place in the list of cues, and the group that lists them
// with their place in that (ProDocument has the same for the window's own changes)
struct Place
{
    int cue = 0;
    int group = -1;
    int inGroup = 0;
    // The cue new slides take their size from, or -1
    int anchor = -1;
};

int indexOfCue(const rv::data::Presentation &presentation, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (int i = 0; i < presentation.cues_size(); ++i) {
        if (presentation.cues(i).uuid().string() == wanted)
            return i;
    }
    return -1;
}

bool placeFor(rv::data::Presentation *presentation, const Where &where, Place *place)
{
    const QString beside = where.after.isEmpty() ? where.before : where.after;
    const bool after = !where.after.isEmpty();
    if (!beside.isEmpty()) {
        place->anchor = indexOfCue(*presentation, beside);
        if (place->anchor < 0)
            return false;
        place->cue = place->anchor + (after ? 1 : 0);
        for (int g = 0; g < presentation->cue_groups_size() && place->group < 0; ++g) {
            const auto &group = presentation->cue_groups(g);
            for (int i = 0; i < group.cue_identifiers_size(); ++i) {
                if (group.cue_identifiers(i).string() == beside.toStdString()) {
                    place->group = g;
                    place->inGroup = i + (after ? 1 : 0);
                    break;
                }
            }
        }
        return true;
    }
    if (!where.group.isEmpty()) {
        place->group = groupIndex(*presentation, where.group);
        if (place->group < 0)
            return false;
        const auto &group = presentation->cue_groups(place->group);
        place->inGroup = group.cue_identifiers_size();
        // After the group's last cue in the list of cues, or failing that at its end
        place->cue = presentation->cues_size();
        if (group.cue_identifiers_size() > 0) {
            const int last = indexOfCue(*presentation, QString::fromStdString(group.cue_identifiers(group.cue_identifiers_size() - 1).string()));
            if (last >= 0) {
                place->cue = last + 1;
                place->anchor = last;
            }
        }
        return true;
    }
    // At the end: of the last group, if there are groups. A presentation with nothing
    // in it at all is given a group for them, since ProPresenter shows a presentation
    // by its groups.
    if (presentation->cue_groups_size() == 0 && presentation->cues_size() == 0) {
        auto *group = presentation->add_cue_groups();
        group->mutable_group()->mutable_uuid()->set_string(newUuid());
        group->mutable_group()->mutable_hotkey();
    }
    if (presentation->cue_groups_size() > 0) {
        place->group = presentation->cue_groups_size() - 1;
        place->inGroup = presentation->cue_groups(place->group).cue_identifiers_size();
    }
    place->cue = presentation->cues_size();
    place->anchor = presentation->cues_size() - 1;
    return true;
}

// The cue at the end of the list is walked back up to its place, in the list of cues
// and in the group's list of them, and the place moves on past it
void settle(rv::data::Presentation *presentation, Place *place)
{
    const std::string id = presentation->cues(presentation->cues_size() - 1).uuid().string();
    for (int i = presentation->cues_size() - 1; i > place->cue; --i)
        presentation->mutable_cues()->SwapElements(i, i - 1);
    ++place->cue;
    if (place->group >= 0) {
        auto *group = presentation->mutable_cue_groups(place->group);
        group->add_cue_identifiers()->set_string(id);
        for (int i = group->cue_identifiers_size() - 1; i > place->inGroup; --i)
            group->mutable_cue_identifiers()->SwapElements(i, i - 1);
        ++place->inGroup;
    }
}

const rv::data::PresentationSlide *constSlideOf(const rv::data::Cue &cue)
{
    for (const rv::data::Action &action : cue.actions()) {
        if (action.isenabled() && action.has_slide() && action.slide().has_presentation())
            return &action.slide().presentation();
    }
    return nullptr;
}

// The size for a new slide: that of the cue at `anchor`, or failing that of the first
// slide there is, or failing that the context's
QSizeF sizeFor(const rv::data::Presentation &presentation, int anchor, const Context &context)
{
    const auto sizeOf = [](const rv::data::Cue &cue) {
        const rv::data::PresentationSlide *slide = constSlideOf(cue);
        if (slide && slide->base_slide().size().width() > 0 && slide->base_slide().size().height() > 0)
            return QSizeF(slide->base_slide().size().width(), slide->base_slide().size().height());
        return QSizeF();
    };
    if (anchor >= 0 && anchor < presentation.cues_size() && sizeOf(presentation.cues(anchor)).isValid())
        return sizeOf(presentation.cues(anchor));
    for (const rv::data::Cue &cue : presentation.cues()) {
        if (sizeOf(cue).isValid())
            return sizeOf(cue);
    }
    return context.size;
}

// The text box, with words in it, of the slide nearest a place in the list of cues: the
// nearest before it, or failing that the nearest after. What a slide made of words
// alone has a box like, so that it looks as the others do. Null if no slide has one.
const rv::data::Slide::Element *wordsBoxNear(const rv::data::Presentation &presentation, int place)
{
    const auto boxOf = [&presentation](int index) -> const rv::data::Slide::Element * {
        const rv::data::PresentationSlide *slide = constSlideOf(presentation.cues(index));
        if (!slide)
            return nullptr;
        const int box = slideedit::wordsBox(slide->base_slide());
        if (box < 0)
            return nullptr;
        const rv::data::Slide::Element &element = slide->base_slide().elements(box);
        return proconvert::readText(element.element().text()).plainText().trimmed().isEmpty() ? nullptr : &element;
    };
    for (int i = qMin(place, presentation.cues_size()) - 1; i >= 0; --i) {
        if (const auto *box = boxOf(i))
            return box;
    }
    for (int i = qMax(0, place); i < presentation.cues_size(); ++i) {
        if (const auto *box = boxOf(i))
            return box;
    }
    return nullptr;
}

// What a slide is described by that this makes of it: its words or its elements, its
// label, its notes, whether it is enabled, its background
QString fill(rv::data::Presentation *presentation, rv::data::Cue *cue, const QVariantMap &wanted, int place)
{
    rv::data::PresentationSlide *shown = slideOf(cue);
    rv::data::Slide *slide = shown->mutable_base_slide();
    if (wanted.contains(QStringLiteral("elements"))) {
        if (wanted.value(QStringLiteral("elements")).typeId() != QMetaType::QVariantList)
            return QStringLiteral("A slide's `elements` is a list of elements");
        for (const QVariant &each : wanted.value(QStringLiteral("elements")).toList()) {
            const extendedapijson::ElementChanges element = extendedapijson::elementChanges(each.toMap(), {});
            if (!element.error.isEmpty())
                return element.error;
            QString id;
            const QString error = slideedit::add(slide, element, &id);
            if (!error.isEmpty())
                return error;
        }
    } else if (!wanted.value(QStringLiteral("text")).toString().isEmpty()) {
        // (A copy of the box, the list of cues being about to grow under the pointer)
        const rv::data::Slide::Element *near = wordsBoxNear(*presentation, place);
        if (near) {
            const rv::data::Slide::Element like = *near;
            slideedit::setWords(slide, wanted.value(QStringLiteral("text")).toString(), &like);
        } else {
            slideedit::setWords(slide, wanted.value(QStringLiteral("text")).toString(), nullptr);
        }
    }
    QString color;
    if (!plainColor(wanted.value(QStringLiteral("label_color")), &color))
        return QStringLiteral("`label_color` is not a colour");
    if (!color.isEmpty())
        labelfile::set(cue, {wanted.value(QStringLiteral("label")).toString(), color});
    const QString notes = wanted.value(QStringLiteral("notes")).toString();
    if (!notes.isEmpty())
        proconvert::writeNotes(shown, RichText::plain(notes, proconvert::notesFormat(), Qt::AlignLeft));
    if (wanted.contains(QStringLiteral("enabled")) && !wanted.value(QStringLiteral("enabled")).toBool())
        cue->set_isenabled(false);
    if (wanted.contains(QStringLiteral("background")) && !slideedit::setBackground(slide, wanted.value(QStringLiteral("background")).toString()))
        return QStringLiteral("`background` is not a colour");
    return {};
}

}

// ---- finding

QList<QStringList> groupCues(const rv::data::Presentation &presentation)
{
    QHash<std::string, bool> known;
    for (const rv::data::Cue &cue : presentation.cues())
        known.insert(cue.uuid().string(), true);
    QList<QStringList> made;
    for (const auto &group : presentation.cue_groups()) {
        QStringList ids;
        for (const rv::data::UUID &id : group.cue_identifiers()) {
            // (A group may name a cue that is no more, which shows nothing)
            if (known.contains(id.string()))
                ids.append(QString::fromStdString(id.string()));
        }
        made.append(ids);
    }
    return made;
}

QStringList cueOrder(const rv::data::Presentation &presentation)
{
    QStringList order;
    if (presentation.cue_groups_size() == 0) {
        for (const rv::data::Cue &cue : presentation.cues())
            order.append(QString::fromStdString(cue.uuid().string()));
        return order;
    }
    for (const QStringList &ids : groupCues(presentation))
        order += ids;
    return order;
}

QString cueId(const rv::data::Presentation &presentation, const QString &which)
{
    if (indexOfCue(presentation, which) >= 0)
        return which;
    bool isNumber = false;
    const int number = which.toInt(&isNumber);
    const QStringList order = cueOrder(presentation);
    return isNumber && number >= 0 && number < order.size() ? order.at(number) : QString();
}

rv::data::Cue *cue(rv::data::Presentation *presentation, const QString &id)
{
    const int index = indexOfCue(*presentation, id);
    return index < 0 ? nullptr : presentation->mutable_cues(index);
}

rv::data::PresentationSlide *slideOf(rv::data::Cue *cue)
{
    for (rv::data::Action &action : *cue->mutable_actions()) {
        if (action.isenabled() && action.has_slide() && action.slide().has_presentation())
            return action.mutable_slide()->mutable_presentation();
    }
    return nullptr;
}

int groupIndex(const rv::data::Presentation &presentation, const QString &which)
{
    const std::string wanted = which.toStdString();
    for (int i = 0; i < presentation.cue_groups_size(); ++i) {
        if (presentation.cue_groups(i).group().uuid().string() == wanted)
            return i;
    }
    for (int i = 0; i < presentation.cue_groups_size(); ++i) {
        if (!which.trimmed().isEmpty()
            && QString::fromStdString(presentation.cue_groups(i).group().name()).trimmed().compare(which.trimmed(), Qt::CaseInsensitive) == 0)
            return i;
    }
    bool isNumber = false;
    const int number = which.toInt(&isNumber);
    return isNumber && number >= 0 && number < presentation.cue_groups_size() ? number : -1;
}

int arrangementIndex(const rv::data::Presentation &presentation, const QString &which)
{
    const std::string wanted = which.toStdString();
    for (int i = 0; i < presentation.arrangements_size(); ++i) {
        if (presentation.arrangements(i).uuid().string() == wanted)
            return i;
    }
    for (int i = 0; i < presentation.arrangements_size(); ++i) {
        if (!which.trimmed().isEmpty()
            && QString::fromStdString(presentation.arrangements(i).name()).trimmed().compare(which.trimmed(), Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

// ---- slides

QVariantMap described(const QVariant &slide)
{
    if (slide.typeId() == QMetaType::QString)
        return {{QStringLiteral("text"), slide.toString()}};
    return slide.toMap();
}

QString addSlides(rv::data::Presentation *presentation, const QVariantList &slides, const Where &where, const Context &context,
                  QStringList *made)
{
    const rv::data::Presentation before = *presentation;
    Place place;
    if (!placeFor(presentation, where, &place)) {
        *presentation = before;
        return where.group.isEmpty() ? slideGone : QStringLiteral("The presentation has no such group");
    }
    QStringList ids;
    for (const QVariant &each : slides) {
        if (each.typeId() != QMetaType::QString && each.typeId() != QMetaType::QVariantMap) {
            *presentation = before;
            return QStringLiteral("Each of `slides` is a slide's words, or an object describing the slide");
        }
        const QVariantMap wanted = described(each);
        rv::data::Cue *cue = proconvert::addBlankCue(presentation, wanted.value(QStringLiteral("label")).toString().toStdString(),
                                                     sizeFor(*presentation, place.anchor, context));
        const QString error = fill(presentation, cue, wanted, place.cue);
        if (!error.isEmpty()) {
            *presentation = before;
            return error;
        }
        ids.append(QString::fromStdString(cue->uuid().string()));
        settle(presentation, &place);
        // (The next takes its size and its look from this one)
        place.anchor = place.cue - 1;
    }
    *made = ids;
    return {};
}

QString changeSlide(rv::data::Presentation *presentation, const QString &cueId, const QVariantMap &changes, const Context &context)
{
    const rv::data::Presentation before = *presentation;
    const auto failed = [&](const QString &why) {
        *presentation = before;
        return why;
    };
    rv::data::Cue *target = cue(presentation, cueId);
    if (!target)
        return slideGone;
    rv::data::PresentationSlide *shown = slideOf(target);

    if (changes.contains(QStringLiteral("label")) || changes.contains(QStringLiteral("label_color"))) {
        const labelfile::Label had = labelfile::of(*target);
        labelfile::Label wanted = had;
        if (changes.contains(QStringLiteral("label")))
            wanted.text = changes.value(QStringLiteral("label")).toString();
        if (changes.contains(QStringLiteral("label_color")) && !plainColor(changes.value(QStringLiteral("label_color")), &wanted.color))
            return failed(QStringLiteral("`label_color` is not a colour"));
        if (!(wanted == had) && !labelfile::set(target, wanted))
            return failed(QStringLiteral("That is not a slide that can have a label"));
    }
    if (changes.contains(QStringLiteral("enabled")) && target->isenabled() != changes.value(QStringLiteral("enabled")).toBool())
        target->set_isenabled(changes.value(QStringLiteral("enabled")).toBool());

    const bool wantsSlide = changes.contains(QStringLiteral("notes")) || changes.contains(QStringLiteral("background"))
                            || changes.contains(QStringLiteral("text"));
    if (wantsSlide && !shown)
        return failed(QStringLiteral("That cue shows no slide to change"));
    if (changes.contains(QStringLiteral("notes"))) {
        // As the editor keeps notes: typed over those there are, in the look they
        // have; and where there are none, nothing typed is nothing to keep
        const QString notes = changes.value(QStringLiteral("notes")).toString();
        if (shown->has_notes()) {
            const RichText had = proconvert::readNotes(*shown);
            if (had.plainText() != notes) {
                const TextRun look = had.paragraphs.isEmpty() ? proconvert::notesFormat() : had.firstRun();
                proconvert::writeNotes(shown, RichText::plain(notes, look, Qt::AlignLeft));
            }
        } else if (!notes.isEmpty()) {
            proconvert::writeNotes(shown, RichText::plain(notes, proconvert::notesFormat(), Qt::AlignLeft));
        }
    }
    if (changes.contains(QStringLiteral("background"))
        && !slideedit::setBackground(shown->mutable_base_slide(), changes.value(QStringLiteral("background")).toString()))
        return failed(QStringLiteral("`background` is not a colour"));
    if (changes.contains(QStringLiteral("text"))) {
        if (changes.value(QStringLiteral("text")).typeId() != QMetaType::QString)
            return failed(QStringLiteral("`text` is the slide's words, as text"));
        const rv::data::Slide::Element *near = wordsBoxNear(*presentation, indexOfCue(*presentation, cueId));
        if (near) {
            const rv::data::Slide::Element like = *near;
            slideedit::setWords(shown->mutable_base_slide(), changes.value(QStringLiteral("text")).toString(), &like);
        } else {
            slideedit::setWords(shown->mutable_base_slide(), changes.value(QStringLiteral("text")).toString(), nullptr);
        }
    }
    if (changes.contains(QStringLiteral("group"))) {
        // A name alone, or { name, color }
        const QVariant asked = changes.value(QStringLiteral("group"));
        const QVariantMap group = asked.typeId() == QMetaType::QString ? QVariantMap {{QStringLiteral("name"), asked}} : asked.toMap();
        QString color;
        if (!plainColor(group.value(QStringLiteral("color")), &color))
            return failed(QStringLiteral("A group's `color` is not a colour"));
        const groupedit::Outcome outcome = groupedit::setGroupFrom(presentation, cueId.toStdString(),
                                                                   choiceOf(group.value(QStringLiteral("name")).toString(), color, context),
                                                                   [] { return newUuid(); });
        if (outcome == groupedit::Outcome::NoSuchSlide)
            return failed(QStringLiteral("That slide is in no group of the presentation"));
    }
    return {};
}

QString moveCue(rv::data::Presentation *presentation, const QString &cueId, const Where &where)
{
    const rv::data::Presentation before = *presentation;
    const int from = indexOfCue(*presentation, cueId);
    if (from < 0)
        return slideGone;
    const QString beside = where.after.isEmpty() ? where.before : where.after;
    if (beside == cueId)
        return {};
    // Out of the groups that list it, and to the end of the list of cues; then to its
    // place as a new one would go
    for (auto &group : *presentation->mutable_cue_groups()) {
        for (int i = group.cue_identifiers_size() - 1; i >= 0; --i) {
            if (group.cue_identifiers(i).string() == cueId.toStdString())
                group.mutable_cue_identifiers()->DeleteSubrange(i, 1);
        }
    }
    for (int i = from; i < presentation->cues_size() - 1; ++i)
        presentation->mutable_cues()->SwapElements(i, i + 1);
    // (Found with the cue set aside: the last of the list is not a place to go beside)
    rv::data::Cue moved = presentation->cues(presentation->cues_size() - 1);
    presentation->mutable_cues()->RemoveLast();
    Place place;
    if (!placeFor(presentation, where, &place)) {
        *presentation = before;
        return where.group.isEmpty() ? slideGone : QStringLiteral("The presentation has no such group");
    }
    *presentation->add_cues() = moved;
    settle(presentation, &place);
    return {};
}

QString removeCue(rv::data::Presentation *presentation, const QString &cueId)
{
    const int index = indexOfCue(*presentation, cueId);
    if (index < 0)
        return slideGone;
    presentation->mutable_cues()->DeleteSubrange(index, 1);
    for (auto &group : *presentation->mutable_cue_groups()) {
        for (int i = group.cue_identifiers_size() - 1; i >= 0; --i) {
            if (group.cue_identifiers(i).string() == cueId.toStdString())
                group.mutable_cue_identifiers()->DeleteSubrange(i, 1);
        }
    }
    return {};
}

// ---- groups

int addGroup(rv::data::Presentation *presentation, const QString &name, const QString &color, const Context &context)
{
    auto *group = presentation->add_cue_groups();
    group->mutable_group()->mutable_uuid()->set_string(newUuid());
    group->mutable_group()->mutable_hotkey();
    if (!name.trimmed().isEmpty())
        groupedit::dress(group->mutable_group(), choiceOf(name, color, context));
    return presentation->cue_groups_size() - 1;
}

QString changeGroup(rv::data::Presentation *presentation, int index, const QVariantMap &changes, const Context &context)
{
    if (index < 0 || index >= presentation->cue_groups_size())
        return QStringLiteral("The presentation has no such group");
    rv::data::Group *group = presentation->mutable_cue_groups(index)->mutable_group();
    const QString hadName = QString::fromStdString(group->name());
    const QString hadColor = group->has_color() && group->color().alpha() > 0 ? proconvert::toColor(group->color()).name() : QString();
    if (changes.contains(QStringLiteral("name"))) {
        // Another name: the workspace's group of that name, if it has one, as a slide's
        // menu would choose it, in the colour given or failing that the list's
        QString color;
        if (changes.contains(QStringLiteral("color")) && !plainColor(changes.value(QStringLiteral("color")), &color))
            return QStringLiteral("A group's `color` is not a colour");
        const QString name = changes.value(QStringLiteral("name")).toString().trimmed();
        if (name != hadName.trimmed() || (changes.contains(QStringLiteral("color")) && color != hadColor))
            groupedit::dress(group, choiceOf(name, color, context));
    } else if (changes.contains(QStringLiteral("color"))) {
        QString color;
        if (!plainColor(changes.value(QStringLiteral("color")), &color))
            return QStringLiteral("A group's `color` is not a colour");
        if (color != hadColor) {
            if (color.isEmpty())
                group->clear_color();
            else
                proconvert::setColor(group->mutable_color(), QColor(color));
        }
    }
    return {};
}

QString moveGroup(rv::data::Presentation *presentation, int index, int target, bool after)
{
    const int count = presentation->cue_groups_size();
    if (index < 0 || index >= count || target < 0 || target >= count)
        return QStringLiteral("The presentation has no such group");
    if (index == target)
        return {};
    // Where it ends up, counted with it taken out first
    int to = target + (after ? 1 : 0);
    if (index < to)
        --to;
    int from = index;
    for (; from < to; ++from)
        presentation->mutable_cue_groups()->SwapElements(from, from + 1);
    for (; from > to; --from)
        presentation->mutable_cue_groups()->SwapElements(from, from - 1);
    return {};
}

void removeGroup(rv::data::Presentation *presentation, int index)
{
    if (index < 0 || index >= presentation->cue_groups_size())
        return;
    const std::string id = presentation->cue_groups(index).group().uuid().string();
    QStringList cues;
    for (const rv::data::UUID &cueId : presentation->cue_groups(index).cue_identifiers())
        cues.append(QString::fromStdString(cueId.string()));
    presentation->mutable_cue_groups()->DeleteSubrange(index, 1);
    for (const QString &cueId : cues)
        removeCue(presentation, cueId);
    for (auto &arrangement : *presentation->mutable_arrangements()) {
        for (int i = arrangement.group_identifiers_size() - 1; i >= 0; --i) {
            if (arrangement.group_identifiers(i).string() == id)
                arrangement.mutable_group_identifiers()->DeleteSubrange(i, 1);
        }
    }
}

// ---- arrangements

QString setArrangement(rv::data::Presentation *presentation, const QString &which, const QVariantMap &changes, QString *id)
{
    const rv::data::Presentation before = *presentation;
    const auto failed = [&](const QString &why) {
        *presentation = before;
        return why;
    };
    // The groups it is to go through, found before anything is made
    QList<std::string> groups;
    const bool hasGroups = changes.contains(QStringLiteral("groups"));
    if (hasGroups) {
        if (changes.value(QStringLiteral("groups")).typeId() != QMetaType::QVariantList
            && changes.value(QStringLiteral("groups")).typeId() != QMetaType::QStringList)
            return QStringLiteral("An arrangement's `groups` is a list of groups, by id or name");
        for (const QVariant &each : changes.value(QStringLiteral("groups")).toList()) {
            const int group = groupIndex(*presentation, each.toString());
            if (group < 0)
                return QStringLiteral("The presentation has no group “%1”").arg(each.toString());
            groups.append(presentation->cue_groups(group).group().uuid().string());
        }
    }
    int index = arrangementIndex(*presentation, which);
    if (index < 0) {
        // A new one, called what it was asked for by
        if (which.trimmed().isEmpty())
            return QStringLiteral("An arrangement needs a name");
        const std::string made = newUuid();
        arrangementedit::add(presentation, which.trimmed(), made);
        index = presentation->arrangements_size() - 1;
    }
    rv::data::Presentation::Arrangement *arrangement = presentation->mutable_arrangements(index);
    *id = QString::fromStdString(arrangement->uuid().string());
    if (hasGroups) {
        bool same = arrangement->group_identifiers_size() == groups.size();
        for (int i = 0; same && i < groups.size(); ++i)
            same = arrangement->group_identifiers(i).string() == groups.at(i);
        if (!same) {
            arrangement->clear_group_identifiers();
            for (const std::string &group : groups)
                arrangement->add_group_identifiers()->set_string(group);
        }
    }
    if (changes.contains(QStringLiteral("name"))) {
        const arrangementedit::Outcome outcome = arrangementedit::rename(presentation, id->toStdString(), changes.value(QStringLiteral("name")).toString());
        if (outcome == arrangementedit::Outcome::NoName)
            return failed(QStringLiteral("An arrangement needs a name"));
        if (outcome == arrangementedit::Outcome::NameTaken)
            return failed(QStringLiteral("The presentation has an arrangement of that name already"));
    }
    return {};
}

QString selectArrangement(rv::data::Presentation *presentation, const QString &which)
{
    if (which.trimmed().isEmpty()) {
        if (presentation->has_selected_arrangement())
            presentation->clear_selected_arrangement();
        return {};
    }
    const int index = arrangementIndex(*presentation, which);
    if (index < 0)
        return QStringLiteral("The presentation has no arrangement “%1”").arg(which);
    if (presentation->selected_arrangement().string() != presentation->arrangements(index).uuid().string())
        *presentation->mutable_selected_arrangement() = presentation->arrangements(index).uuid();
    return {};
}

// ---- a presentation made

QString build(rv::data::Presentation *presentation, const QVariantMap &wanted, const Context &context, QStringList *made,
              QVariantList *descriptions)
{
    QStringList ids;
    QVariantList all;
    const auto addTo = [&](int group, const QVariantList &slides) -> QString {
        QStringList added;
        const QString error = addSlides(presentation, slides, {QString(), QString(),
                                        QString::fromStdString(presentation->cue_groups(group).group().uuid().string())}, context, &added);
        if (!error.isEmpty())
            return error;
        ids += added;
        for (const QVariant &slide : slides)
            all.append(described(slide));
        return {};
    };

    if (wanted.contains(QStringLiteral("groups"))) {
        if (wanted.value(QStringLiteral("groups")).typeId() != QMetaType::QVariantList)
            return QStringLiteral("`groups` is a list of groups, each {name, slides}");
        for (const QVariant &each : wanted.value(QStringLiteral("groups")).toList()) {
            const QVariantMap group = each.toMap();
            if (each.typeId() != QMetaType::QVariantMap || group.value(QStringLiteral("slides")).typeId() != QMetaType::QVariantList)
                return QStringLiteral("Each of `groups` is {name, slides}, `slides` a list of slides");
            QString color;
            if (!plainColor(group.value(QStringLiteral("color")), &color))
                return QStringLiteral("A group's `color` is not a colour");
            const QString error = addTo(addGroup(presentation, group.value(QStringLiteral("name")).toString(), color, context),
                                        group.value(QStringLiteral("slides")).toList());
            if (!error.isEmpty())
                return error;
        }
    } else {
        // In a group with an id and no name, which is how ProPresenter keeps slides
        // that are in no group; with nothing asked for, one slide with nothing on it,
        // as a new presentation here has always had
        QVariantList slides = wanted.value(QStringLiteral("slides")).toList();
        if (wanted.contains(QStringLiteral("slides")) && wanted.value(QStringLiteral("slides")).typeId() != QMetaType::QVariantList)
            return QStringLiteral("`slides` is a list of slides");
        if (slides.isEmpty())
            slides.append(QVariantMap());
        const QString error = addTo(addGroup(presentation, QString(), QString(), context), slides);
        if (!error.isEmpty())
            return error;
    }

    if (wanted.contains(QStringLiteral("arrangements"))) {
        if (wanted.value(QStringLiteral("arrangements")).typeId() != QMetaType::QVariantList)
            return QStringLiteral("`arrangements` is a list, each {name, groups}");
        for (const QVariant &each : wanted.value(QStringLiteral("arrangements")).toList()) {
            const QVariantMap arrangement = each.toMap();
            QString id;
            QVariantMap changes;
            if (arrangement.contains(QStringLiteral("groups")))
                changes.insert(QStringLiteral("groups"), arrangement.value(QStringLiteral("groups")));
            const QString error = setArrangement(presentation, arrangement.value(QStringLiteral("name")).toString(), changes, &id);
            if (!error.isEmpty())
                return error;
        }
    }
    if (wanted.contains(QStringLiteral("arrangement"))) {
        const QString error = selectArrangement(presentation, wanted.value(QStringLiteral("arrangement")).toString());
        if (!error.isEmpty())
            return error;
    }
    if (wanted.contains(QStringLiteral("destination"))) {
        const QString destination = wanted.value(QStringLiteral("destination")).toString();
        if (destination == QLatin1String("announcements"))
            presentation->set_content_destination(rv::data::Action::CONTENT_DESTINATION_ANNOUNCEMENTS);
        else if (destination != QLatin1String("presentation") && !destination.isEmpty())
            return QStringLiteral("`destination` is \"presentation\" or \"announcements\"");
    }
    if (wanted.contains(QStringLiteral("copyright"))) {
        if (wanted.value(QStringLiteral("copyright")).typeId() != QMetaType::QVariantMap)
            return QStringLiteral("`copyright` is an object: {title, author, artist, publisher, year, song_number, display}");
        ccli::change(presentation->mutable_ccli(), extendedapijson::named(wanted.value(QStringLiteral("copyright")).toMap()));
    }
    if (wanted.contains(QStringLiteral("key"))) {
        const QString key = wanted.value(QStringLiteral("key")).toString();
        if (chords::keyNumber(key) < 0)
            return QStringLiteral("“%1” is not a key: one is written as \"G\", \"F#m\", \"Bb\"").arg(key);
        using Scale = rv::data::MusicKeyScale;
        for (Scale *target : {presentation->mutable_music()->mutable_original(), presentation->mutable_music()->mutable_user()}) {
            target->set_music_key(Scale::MusicKey(chords::keyNumber(key)));
            target->set_music_scale(chords::keyIsMinor(key) ? Scale::MUSIC_SCALE_MINOR : Scale::MUSIC_SCALE_MAJOR);
        }
    }
    *made = ids;
    *descriptions = all;
    return {};
}

}
