#include "arrangementedit.h"

#include <QSet>

namespace arrangementedit {

namespace {

rv::data::Presentation::Arrangement *find(rv::data::Presentation *presentation, const std::string &id, int *index = nullptr)
{
    for (int i = 0; i < presentation->arrangements_size(); ++i) {
        if (presentation->arrangements(i).uuid().string() == id) {
            if (index)
                *index = i;
            return presentation->mutable_arrangements(i);
        }
    }
    return nullptr;
}

// Moves the last of a repeated field to a place in it, the ones from there on each
// going one further along.
template <typename Field>
void lastTo(Field *field, int place)
{
    for (int i = field->size() - 1; i > place; --i)
        field->SwapElements(i, i - 1);
}

} // namespace

QString freeName(const rv::data::Presentation &presentation, const QString &wanted)
{
    QSet<QString> taken;
    for (const auto &arrangement : presentation.arrangements())
        taken.insert(QString::fromStdString(arrangement.name()));
    QString name = wanted;
    for (int number = 2; taken.contains(name); ++number)
        name = QStringLiteral("%1 %2").arg(wanted).arg(number);
    return name;
}

QString add(rv::data::Presentation *presentation, const QString &name, const std::string &newId)
{
    const QString given = freeName(*presentation, name);
    rv::data::Presentation::Arrangement *made = presentation->add_arrangements();
    made->mutable_uuid()->set_string(newId);
    made->set_name(given.toStdString());
    for (const auto &group : presentation->cue_groups())
        made->add_group_identifiers()->set_string(group.group().uuid().string());
    return given;
}

Outcome duplicate(rv::data::Presentation *presentation, const std::string &id, const std::string &newId, QString *made)
{
    int index = -1;
    const rv::data::Presentation::Arrangement *from = find(presentation, id, &index);
    if (!from)
        return Outcome::NoSuchArrangement;
    // (A copy of the whole of it, so that anything an arrangement may come to hold
    // that this does not know of is copied too; then its own id and name.)
    rv::data::Presentation::Arrangement copy = *from;
    *made = freeName(*presentation, QString::fromStdString(from->name()));
    copy.mutable_uuid()->set_string(newId);
    copy.set_name(made->toStdString());
    *presentation->add_arrangements() = copy;
    lastTo(presentation->mutable_arrangements(), index + 1);
    return Outcome::Done;
}

Outcome rename(rv::data::Presentation *presentation, const std::string &id, const QString &name)
{
    rv::data::Presentation::Arrangement *arrangement = find(presentation, id);
    if (!arrangement)
        return Outcome::NoSuchArrangement;
    const QString wanted = name.trimmed();
    if (wanted.isEmpty())
        return Outcome::NoName;
    if (QString::fromStdString(arrangement->name()) == wanted)
        return Outcome::Unchanged;
    for (const auto &other : presentation->arrangements()) {
        if (other.uuid().string() != id && QString::fromStdString(other.name()) == wanted)
            return Outcome::NameTaken;
    }
    arrangement->set_name(wanted.toStdString());
    return Outcome::Done;
}

Outcome remove(rv::data::Presentation *presentation, const std::string &id)
{
    int index = -1;
    if (!find(presentation, id, &index))
        return Outcome::NoSuchArrangement;
    presentation->mutable_arrangements()->DeleteSubrange(index, 1);
    if (presentation->has_selected_arrangement() && presentation->selected_arrangement().string() == id)
        presentation->clear_selected_arrangement();
    return Outcome::Done;
}

Outcome insertGroup(rv::data::Presentation *presentation, const std::string &id, const std::string &groupId, int landing)
{
    rv::data::Presentation::Arrangement *arrangement = find(presentation, id);
    if (!arrangement)
        return Outcome::NoSuchArrangement;
    bool known = false;
    for (const auto &group : presentation->cue_groups())
        known = known || group.group().uuid().string() == groupId;
    if (!known)
        return Outcome::NoSuchGroup;
    if (landing < 0 || landing > arrangement->group_identifiers_size())
        return Outcome::NoSuchPlace;
    arrangement->add_group_identifiers()->set_string(groupId);
    lastTo(arrangement->mutable_group_identifiers(), landing);
    return Outcome::Done;
}

Outcome moveGroup(rv::data::Presentation *presentation, const std::string &id, int from, int landing)
{
    rv::data::Presentation::Arrangement *arrangement = find(presentation, id);
    if (!arrangement)
        return Outcome::NoSuchArrangement;
    const int count = arrangement->group_identifiers_size();
    if (from < 0 || from >= count || landing < 0 || landing > count)
        return Outcome::NoSuchPlace;
    const int to = placed(count, from, landing);
    if (to < 0)
        return Outcome::Unchanged;
    // Step by step, each a swap with its neighbour.
    for (int at = from; at < to; ++at)
        arrangement->mutable_group_identifiers()->SwapElements(at, at + 1);
    for (int at = from; at > to; --at)
        arrangement->mutable_group_identifiers()->SwapElements(at, at - 1);
    return Outcome::Done;
}

Outcome removeGroup(rv::data::Presentation *presentation, const std::string &id, int place)
{
    rv::data::Presentation::Arrangement *arrangement = find(presentation, id);
    if (!arrangement)
        return Outcome::NoSuchArrangement;
    if (place < 0 || place >= arrangement->group_identifiers_size())
        return Outcome::NoSuchPlace;
    arrangement->mutable_group_identifiers()->DeleteSubrange(place, 1);
    return Outcome::Done;
}

int placed(int count, int from, int landing)
{
    if (count < 1 || from < 0 || from >= count || landing < 0 || landing > count)
        return -1;
    // The ones after where it was have each moved back one by the time it is put in
    // again, so a landing after it is one place less.
    const int to = landing > from ? landing - 1 : landing;
    return to == from ? -1 : to;
}

} // namespace arrangementedit
