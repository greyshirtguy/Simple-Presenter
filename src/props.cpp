#include "props.h"

#include "proconvert.h"
#include "workspacefiles.h"

#include <QFile>
#include <QSet>

namespace {

using Collection = rv::data::PropDocument::PropCollection;

const QString what = QStringLiteral("the props");
const QString propGone = QStringLiteral("There is no longer such a prop");
const QString collectionGone = QStringLiteral("There is no longer such a collection");

// The slide a prop's cue shows, as an index into its actions, or -1.
int slideAction(const rv::data::Cue &cue)
{
    for (int i = 0; i < cue.actions_size(); ++i) {
        if (cue.actions(i).has_slide() && cue.actions(i).slide().has_prop())
            return i;
    }
    return -1;
}

int cueIndex(const rv::data::PropDocument &document, const std::string &id)
{
    for (int i = 0; i < document.cues_size(); ++i) {
        if (document.cues(i).uuid().string() == id)
            return i;
    }
    return -1;
}

Collection *findCollection(rv::data::PropDocument *document, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (Collection &collection : *document->mutable_prop_collections()) {
        if (collection.uuid().string() == wanted)
            return &collection;
    }
    return nullptr;
}

// Takes a prop out of whichever collections list it.
void unlist(rv::data::PropDocument *document, const std::string &id)
{
    for (Collection &collection : *document->mutable_prop_collections()) {
        for (int i = collection.items_size() - 1; i >= 0; --i) {
            if (collection.items(i).prop_cue_uuid().string() == id)
                collection.mutable_items()->DeleteSubrange(i, 1);
        }
    }
}

// `base`, or `base` and the first number that makes it a name not among `taken`.
QString freeName(const QString &base, const QSet<QString> &taken)
{
    QString name = base;
    for (int n = 2; taken.contains(name); ++n)
        name = QStringLiteral("%1 %2").arg(base).arg(n);
    return name;
}

QSet<QString> propNames(const rv::data::PropDocument &document)
{
    QSet<QString> names;
    for (const rv::data::Cue &cue : document.cues())
        names.insert(QString::fromStdString(cue.name()));
    return names;
}

// Puts every prop that no collection lists into one, as ProPresenter did when it gave
// props collections: the first collection if there is one, and otherwise a new one.
void collectStrays(rv::data::PropDocument *document)
{
    QSet<std::string> listed;
    for (const Collection &collection : document->prop_collections()) {
        for (const Collection::Item &item : collection.items())
            listed.insert(item.prop_cue_uuid().string());
    }
    for (const rv::data::Cue &cue : document->cues()) {
        if (listed.contains(cue.uuid().string()))
            continue;
        if (document->prop_collections_size() == 0) {
            Collection *made = document->add_prop_collections();
            made->mutable_uuid()->set_string(workspace::newUuid());
            made->set_name("Default Collection");
        }
        document->mutable_prop_collections(0)->add_items()->mutable_prop_cue_uuid()->set_string(cue.uuid().string());
    }
}

} // namespace

QString Props::path() const
{
    return m_workspace.isEmpty() ? QString() : m_workspace + QStringLiteral("/Configuration/Props");
}

QString Props::open(const QString &workspace)
{
    m_workspace = workspace;
    return reload();
}

QString Props::reload()
{
    rv::data::PropDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        document.Clear();
    show(document);
    return error;
}

// A workspace with no props file has no props, which is not an error.
QString Props::read(rv::data::PropDocument *document) const
{
    if (!QFile::exists(path())) {
        document->Clear();
        return {};
    }
    return workspace::readMessage(path(), document, what);
}

QString Props::write(rv::data::PropDocument *document)
{
    collectStrays(document);
    const QString error = workspace::writeMessage(path(), *document, what);
    if (error.isEmpty())
        show(*document);
    return error;
}

// Takes the props as a file has them as what there is to show.
void Props::show(const rv::data::PropDocument &document)
{
    const auto describe = [](const rv::data::Cue &cue) {
        const int action = slideAction(cue);
        const QString name = QString::fromStdString(cue.name());
        return QVariantMap {
            {"id", QString::fromStdString(cue.uuid().string())},
            {"name", name},
            {"slide", action < 0 ? QVariantMap()
                                 : proconvert::toSlideMap(cue.actions(action).slide().prop().base_slide(), name)},
        };
    };

    m_collections.clear();
    QSet<std::string> listed;
    for (const Collection &collection : document.prop_collections()) {
        QVariantList props;
        for (const Collection::Item &item : collection.items()) {
            const int index = cueIndex(document, item.prop_cue_uuid().string());
            if (index >= 0 && slideAction(document.cues(index)) >= 0) {
                props.append(describe(document.cues(index)));
                listed.insert(item.prop_cue_uuid().string());
            }
        }
        m_collections.append(QVariantMap {
            {"id", QString::fromStdString(collection.uuid().string())},
            {"name", QString::fromStdString(collection.name())},
            {"single", collection.single_prop_enabled()},
            {"props", props},
        });
    }
    // Props that no collection lists: a file from before there were collections. They
    // are a collection here, with no id, until the file is next written.
    QVariantList strays;
    for (const rv::data::Cue &cue : document.cues()) {
        if (!listed.contains(cue.uuid().string()) && slideAction(cue) >= 0)
            strays.append(describe(cue));
    }
    if (!strays.isEmpty())
        m_collections.prepend(QVariantMap {{"id", QString()}, {"name", QStringLiteral("Props")}, {"single", false}, {"props", strays}});

    // A file that says nothing of how a prop comes and goes has it dissolve, briefly.
    m_transitionDuration = document.has_transition() ? document.transition().duration() : 0.5;
    emit changed();
}

QVariantMap Props::find(const QString &id) const
{
    for (const QVariant &entry : m_collections) {
        const QVariantMap collection = entry.toMap();
        const QVariantList props = collection.value("props").toList();
        for (const QVariant &candidate : props) {
            QVariantMap prop = candidate.toMap();
            if (prop.value("id").toString() == id) {
                prop.insert("collection", collection.value("id"));
                prop.insert("single", collection.value("single"));
                return prop;
            }
        }
    }
    return {};
}

QVariantMap Props::add(const QString &collection)
{
    rv::data::PropDocument document;
    QString error = read(&document);
    if (!error.isEmpty())
        return {{"id", QString()}, {"error", error}};
    collectStrays(&document);
    Collection *target = findCollection(&document, collection);
    if (!target && document.prop_collections_size() > 0)
        target = document.mutable_prop_collections(0);
    if (!target) {
        target = document.add_prop_collections();
        target->mutable_uuid()->set_string(workspace::newUuid());
        target->set_name("Default Collection");
    }

    // A cue as ProPresenter writes a prop's, with a slide that has nothing on it.
    const std::string id = workspace::newUuid();
    rv::data::Cue *cue = document.add_cues();
    cue->mutable_uuid()->set_string(id);
    cue->set_name(freeName(QStringLiteral("Prop"), propNames(document)).toStdString());
    cue->set_completion_action_type(rv::data::Cue::COMPLETION_ACTION_TYPE_LAST);
    cue->mutable_hot_key();
    cue->set_isenabled(true);
    rv::data::Action *action = cue->add_actions();
    action->mutable_uuid()->set_string(workspace::newUuid());
    action->set_isenabled(true);
    action->set_type(rv::data::Action::ACTION_TYPE_PROP_SLIDE);
    rv::data::Slide *slide = action->mutable_slide()->mutable_prop()->mutable_base_slide();
    slide->mutable_size()->set_width(1920);
    slide->mutable_size()->set_height(1080);
    slide->mutable_uuid()->set_string(workspace::newUuid());
    target->add_items()->mutable_prop_cue_uuid()->set_string(id);

    error = write(&document);
    return {{"id", error.isEmpty() ? QString::fromStdString(id) : QString()}, {"error", error}};
}

QVariantMap Props::duplicate(const QString &id)
{
    rv::data::PropDocument document;
    QString error = read(&document);
    if (!error.isEmpty())
        return {{"id", QString()}, {"error", error}};
    collectStrays(&document);
    const int index = cueIndex(document, id.toStdString());
    if (index < 0)
        return {{"id", QString()}, {"error", propGone}};

    // The same prop under another name, with ids of its own, listed after the original.
    rv::data::Cue copy = document.cues(index);
    const std::string copyId = workspace::newUuid();
    copy.mutable_uuid()->set_string(copyId);
    copy.set_name(freeName(QString::fromStdString(copy.name()), propNames(document)).toStdString());
    for (rv::data::Action &action : *copy.mutable_actions()) {
        action.mutable_uuid()->set_string(workspace::newUuid());
        if (action.has_slide() && action.slide().has_prop())
            action.mutable_slide()->mutable_prop()->mutable_base_slide()->mutable_uuid()->set_string(workspace::newUuid());
    }
    *document.add_cues() = copy;
    for (Collection &collection : *document.mutable_prop_collections()) {
        for (int i = 0; i < collection.items_size(); ++i) {
            if (collection.items(i).prop_cue_uuid().string() != id.toStdString())
                continue;
            collection.add_items()->mutable_prop_cue_uuid()->set_string(copyId);
            for (int at = collection.items_size() - 1; at > i + 1; --at)
                collection.mutable_items()->SwapElements(at, at - 1);
            error = write(&document);
            return {{"id", error.isEmpty() ? QString::fromStdString(copyId) : QString()}, {"error", error}};
        }
    }
    return {{"id", QString()}, {"error", propGone}};
}

QString Props::rename(const QString &id, const QString &name)
{
    rv::data::PropDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    const int index = cueIndex(document, id.toStdString());
    if (index < 0)
        return propGone;
    if (name.trimmed().isEmpty())
        return {};
    document.mutable_cues(index)->set_name(name.trimmed().toStdString());
    return write(&document);
}

QString Props::remove(const QString &id)
{
    rv::data::PropDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    const int index = cueIndex(document, id.toStdString());
    if (index < 0)
        return propGone;
    document.mutable_cues()->DeleteSubrange(index, 1);
    unlist(&document, id.toStdString());
    return write(&document);
}

QString Props::move(const QString &id, const QString &collection)
{
    rv::data::PropDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    collectStrays(&document);
    if (cueIndex(document, id.toStdString()) < 0)
        return propGone;
    Collection *target = findCollection(&document, collection);
    if (!target)
        return collectionGone;
    unlist(&document, id.toStdString());
    target->add_items()->mutable_prop_cue_uuid()->set_string(id.toStdString());
    return write(&document);
}

QString Props::place(const QString &id, const QString &before)
{
    rv::data::PropDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    collectStrays(&document);
    const std::string moved = id.toStdString();
    const std::string target = before.toStdString();
    for (Collection &collection : *document.mutable_prop_collections()) {
        int from = -1;
        for (int i = 0; i < collection.items_size(); ++i) {
            if (collection.items(i).prop_cue_uuid().string() == moved)
                from = i;
        }
        if (from < 0)
            continue;
        // Taken out, and put back before the other, or at the end.
        collection.mutable_items()->DeleteSubrange(from, 1);
        int to = collection.items_size();
        for (int i = 0; i < collection.items_size(); ++i) {
            if (!target.empty() && collection.items(i).prop_cue_uuid().string() == target)
                to = i;
        }
        collection.add_items()->mutable_prop_cue_uuid()->set_string(moved);
        for (int at = collection.items_size() - 1; at > to; --at)
            collection.mutable_items()->SwapElements(at, at - 1);
        return write(&document);
    }
    return propGone;
}

QVariantMap Props::addCollection()
{
    rv::data::PropDocument document;
    QString error = read(&document);
    if (!error.isEmpty())
        return {{"id", QString()}, {"error", error}};
    collectStrays(&document);
    QSet<QString> names;
    for (const Collection &collection : document.prop_collections())
        names.insert(QString::fromStdString(collection.name()));
    const std::string id = workspace::newUuid();
    Collection *made = document.add_prop_collections();
    made->mutable_uuid()->set_string(id);
    made->set_name(freeName(QStringLiteral("New Collection"), names).toStdString());
    error = write(&document);
    return {{"id", error.isEmpty() ? QString::fromStdString(id) : QString()}, {"error", error}};
}

QString Props::renameCollection(const QString &id, const QString &name)
{
    rv::data::PropDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    collectStrays(&document);
    Collection *collection = findCollection(&document, id);
    if (!collection)
        return collectionGone;
    if (name.trimmed().isEmpty())
        return {};
    collection->set_name(name.trimmed().toStdString());
    return write(&document);
}

QString Props::removeCollection(const QString &id)
{
    rv::data::PropDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    collectStrays(&document);
    for (int i = 0; i < document.prop_collections_size(); ++i) {
        if (document.prop_collections(i).uuid().string() != id.toStdString())
            continue;
        // Its props go with it.
        for (const Collection::Item &item : document.prop_collections(i).items()) {
            const int cue = cueIndex(document, item.prop_cue_uuid().string());
            if (cue >= 0)
                document.mutable_cues()->DeleteSubrange(cue, 1);
        }
        document.mutable_prop_collections()->DeleteSubrange(i, 1);
        return write(&document);
    }
    return collectionGone;
}

QString Props::setSingle(const QString &id, bool single)
{
    rv::data::PropDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    collectStrays(&document);
    Collection *collection = findCollection(&document, id);
    if (!collection)
        return collectionGone;
    collection->set_single_prop_enabled(single);
    return write(&document);
}
