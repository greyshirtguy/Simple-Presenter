#include "macros.h"

#include "actions.h"
#include "proconvert.h"
#include "workspacefiles.h"

#include <QColor>

#include <QFile>
#include <QSet>

namespace {

using Macro = rv::data::MacrosDocument::Macro;
using Collection = rv::data::MacrosDocument::MacroCollection;

const QString what = QStringLiteral("the macros");
const QString macroGone = QStringLiteral("There is no longer such a macro");
const QString collectionGone = QStringLiteral("There is no longer such a collection");

int macroIndex(const rv::data::MacrosDocument &document, const std::string &id)
{
    for (int i = 0; i < document.macros_size(); ++i) {
        if (document.macros(i).uuid().string() == id)
            return i;
    }
    return -1;
}

Collection *findCollection(rv::data::MacrosDocument *document, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (Collection &collection : *document->mutable_macro_collections()) {
        if (collection.uuid().string() == wanted)
            return &collection;
    }
    return nullptr;
}

// Takes a macro out of whichever collections list it.
void unlist(rv::data::MacrosDocument *document, const std::string &id)
{
    for (Collection &collection : *document->mutable_macro_collections()) {
        for (int i = collection.items_size() - 1; i >= 0; --i) {
            if (collection.items(i).macro_id().string() == id)
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

QSet<QString> macroNames(const rv::data::MacrosDocument &document)
{
    QSet<QString> names;
    for (const Macro &macro : document.macros())
        names.insert(QString::fromStdString(macro.name()));
    return names;
}

// Puts every macro that no collection lists into one: the first collection if there
// is one, and otherwise a new one, named as ProPresenter names its first.
void collectStrays(rv::data::MacrosDocument *document)
{
    QSet<std::string> listed;
    for (const Collection &collection : document->macro_collections()) {
        for (const Collection::Item &item : collection.items())
            listed.insert(item.macro_id().string());
    }
    for (const Macro &macro : document->macros()) {
        if (listed.contains(macro.uuid().string()))
            continue;
        if (document->macro_collections_size() == 0) {
            Collection *made = document->add_macro_collections();
            made->mutable_uuid()->set_string(workspace::newUuid());
            made->set_name("Default Collection");
        }
        document->mutable_macro_collections(0)->add_items()->mutable_macro_id()->set_string(macro.uuid().string());
    }
}

QVariantMap describe(const Macro &macro)
{
    QVariantList listed;
    for (const rv::data::Action &action : macro.actions()) {
        if (action.isenabled())
            listed.append(actions::describe(action));
    }
    // A colour that is all nothing, alpha and all, is no colour given.
    const rv::data::Color &color = macro.color();
    const bool coloured = macro.has_color() && color.alpha() > 0;
    // The pictures that are a digit or a letter are the ones drawn here as themselves.
    const int image = int(macro.image_type());
    QString letter;
    if (image >= Macro::ImageTypeOne && image <= Macro::ImageTypeNine)
        letter = QString::number(image - Macro::ImageTypeOne + 1);
    else if (image == Macro::ImageTypeZero)
        letter = QStringLiteral("0");
    else if (image >= Macro::ImageTypeLetterA && image <= Macro::ImageTypeLetterZ)
        letter = QString(QChar('A' + image - Macro::ImageTypeLetterA));
    return {
        {"id", QString::fromStdString(macro.uuid().string())},
        {"name", QString::fromStdString(macro.name())},
        {"color", coloured ? proconvert::toColor(color).name() : QString()},
        {"letter", letter},
        {"actions", listed},
    };
}

} // namespace

QString Macros::path() const
{
    return m_workspace.isEmpty() ? QString() : m_workspace + QStringLiteral("/Configuration/Macros");
}

QString Macros::open(const QString &workspace)
{
    m_workspace = workspace;
    return reload();
}

QString Macros::reload()
{
    rv::data::MacrosDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        document.Clear();
    show(document);
    return error;
}

// A workspace with no macros file has no macros, which is not an error.
QString Macros::read(rv::data::MacrosDocument *document) const
{
    if (!QFile::exists(path())) {
        document->Clear();
        return {};
    }
    return workspace::readMessage(path(), document, what);
}

QString Macros::write(rv::data::MacrosDocument *document)
{
    collectStrays(document);
    const QString error = workspace::writeMessage(path(), *document, what);
    if (error.isEmpty())
        show(*document);
    return error;
}

void Macros::show(const rv::data::MacrosDocument &document)
{
    m_collections.clear();
    QSet<std::string> listed;
    for (const Collection &collection : document.macro_collections()) {
        QVariantList macros;
        for (const Collection::Item &item : collection.items()) {
            const int index = macroIndex(document, item.macro_id().string());
            if (index >= 0) {
                macros.append(describe(document.macros(index)));
                listed.insert(item.macro_id().string());
            }
        }
        m_collections.append(QVariantMap {
            {"id", QString::fromStdString(collection.uuid().string())},
            {"name", QString::fromStdString(collection.name())},
            {"macros", macros},
        });
    }
    // Macros that no collection lists are a collection here, with no id, until the
    // file is next written.
    QVariantList strays;
    for (const Macro &macro : document.macros()) {
        if (!listed.contains(macro.uuid().string()))
            strays.append(describe(macro));
    }
    if (!strays.isEmpty())
        m_collections.prepend(QVariantMap {{"id", QString()}, {"name", QStringLiteral("Macros")}, {"macros", strays}});
    emit changed();
}

QVariantMap Macros::find(const QString &id, const QString &name) const
{
    QVariantMap byName;
    for (const QVariant &entry : m_collections) {
        const QVariantMap collection = entry.toMap();
        const QVariantList macros = collection.value("macros").toList();
        for (const QVariant &candidate : macros) {
            QVariantMap macro = candidate.toMap();
            const bool byId = !id.isEmpty() && macro.value("id").toString() == id;
            const bool named = byName.isEmpty() && !name.isEmpty() && macro.value("name").toString() == name;
            if (!byId && !named)
                continue;
            macro.insert("collection", collection.value("id"));
            macro.insert("collectionName", collection.value("name"));
            if (byId)
                return macro;
            byName = macro;
        }
    }
    return byName;
}

QVariantMap Macros::add(const QString &collection)
{
    rv::data::MacrosDocument document;
    QString error = read(&document);
    if (!error.isEmpty())
        return {{"id", QString()}, {"error", error}};
    collectStrays(&document);
    if (document.macro_collections_size() == 0) {
        Collection *made = document.add_macro_collections();
        made->mutable_uuid()->set_string(workspace::newUuid());
        made->set_name("Default Collection");
    }
    Collection *target = collection.isEmpty() ? document.mutable_macro_collections(0) : findCollection(&document, collection);
    if (!target)
        return {{"id", QString()}, {"error", collectionGone}};

    const std::string id = workspace::newUuid();
    Macro *macro = document.add_macros();
    macro->mutable_uuid()->set_string(id);
    macro->set_name(freeName(QStringLiteral("Macro"), macroNames(document)).toStdString());
    macro->mutable_color();
    target->add_items()->mutable_macro_id()->set_string(id);
    error = write(&document);
    return {{"id", error.isEmpty() ? QString::fromStdString(id) : QString()}, {"error", error}};
}

QVariantMap Macros::duplicate(const QString &id)
{
    rv::data::MacrosDocument document;
    QString error = read(&document);
    if (!error.isEmpty())
        return {{"id", QString()}, {"error", error}};
    collectStrays(&document);
    const int index = macroIndex(document, id.toStdString());
    if (index < 0)
        return {{"id", QString()}, {"error", macroGone}};

    // The same macro under another name, with ids of its own, listed after the original.
    Macro copy = document.macros(index);
    const std::string copyId = workspace::newUuid();
    copy.mutable_uuid()->set_string(copyId);
    copy.set_name(freeName(QString::fromStdString(copy.name()), macroNames(document)).toStdString());
    for (rv::data::Action &action : *copy.mutable_actions())
        action.mutable_uuid()->set_string(workspace::newUuid());
    *document.add_macros() = copy;
    for (Collection &collection : *document.mutable_macro_collections()) {
        for (int i = 0; i < collection.items_size(); ++i) {
            if (collection.items(i).macro_id().string() != id.toStdString())
                continue;
            collection.add_items()->mutable_macro_id()->set_string(copyId);
            for (int at = collection.items_size() - 1; at > i + 1; --at)
                collection.mutable_items()->SwapElements(at, at - 1);
            error = write(&document);
            return {{"id", error.isEmpty() ? QString::fromStdString(copyId) : QString()}, {"error", error}};
        }
    }
    return {{"id", QString()}, {"error", macroGone}};
}

QString Macros::rename(const QString &id, const QString &name)
{
    return changeMacro(id, [&name](Macro *macro) {
        macro->set_name(name.toStdString());
        return QString();
    });
}

QString Macros::setColor(const QString &id, const QString &color)
{
    return changeMacro(id, [&color](Macro *macro) {
        macro->mutable_color()->Clear();
        if (!color.isEmpty())
            proconvert::setColor(macro->mutable_color(), QColor(color));
        return QString();
    });
}

QString Macros::remove(const QString &id)
{
    rv::data::MacrosDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    const int index = macroIndex(document, id.toStdString());
    if (index < 0)
        return macroGone;
    document.mutable_macros()->DeleteSubrange(index, 1);
    unlist(&document, id.toStdString());
    return write(&document);
}

QString Macros::move(const QString &id, const QString &collection)
{
    rv::data::MacrosDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    collectStrays(&document);
    if (macroIndex(document, id.toStdString()) < 0)
        return macroGone;
    Collection *target = findCollection(&document, collection);
    if (!target)
        return collectionGone;
    unlist(&document, id.toStdString());
    target->add_items()->mutable_macro_id()->set_string(id.toStdString());
    return write(&document);
}

QVariantMap Macros::addCollection()
{
    rv::data::MacrosDocument document;
    QString error = read(&document);
    if (!error.isEmpty())
        return {{"id", QString()}, {"error", error}};
    collectStrays(&document);
    QSet<QString> names;
    for (const Collection &collection : document.macro_collections())
        names.insert(QString::fromStdString(collection.name()));
    const std::string id = workspace::newUuid();
    Collection *made = document.add_macro_collections();
    made->mutable_uuid()->set_string(id);
    made->set_name(freeName(QStringLiteral("New Collection"), names).toStdString());
    error = write(&document);
    return {{"id", error.isEmpty() ? QString::fromStdString(id) : QString()}, {"error", error}};
}

QString Macros::renameCollection(const QString &id, const QString &name)
{
    rv::data::MacrosDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    Collection *collection = findCollection(&document, id);
    if (!collection)
        return collectionGone;
    collection->set_name(name.toStdString());
    return write(&document);
}

QString Macros::removeCollection(const QString &id)
{
    rv::data::MacrosDocument document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    collectStrays(&document);
    for (int i = 0; i < document.macro_collections_size(); ++i) {
        if (document.macro_collections(i).uuid().string() != id.toStdString())
            continue;
        for (const Collection::Item &item : document.macro_collections(i).items()) {
            const int index = macroIndex(document, item.macro_id().string());
            if (index >= 0)
                document.mutable_macros()->DeleteSubrange(index, 1);
        }
        document.mutable_macro_collections()->DeleteSubrange(i, 1);
        return write(&document);
    }
    return collectionGone;
}

QString Macros::addAction(const QString &id, const QVariantMap &action)
{
    return changeMacro(id, [&action](Macro *macro) {
        rv::data::Action made;
        const QString error = actions::build(action, &made);
        if (error.isEmpty())
            *macro->add_actions() = made;
        return error;
    });
}

QString Macros::changeAction(const QString &id, const QString &actionId, const QVariantMap &action)
{
    return changeMacro(id, [&](Macro *macro) {
        for (rv::data::Action &candidate : *macro->mutable_actions()) {
            if (QString::fromStdString(candidate.uuid().string()) == actionId)
                return actions::build(action, &candidate);
        }
        return QStringLiteral("That macro no longer has that action");
    });
}

QString Macros::removeAction(const QString &id, const QString &actionId)
{
    return changeMacro(id, [&](Macro *macro) {
        for (int i = 0; i < macro->actions_size(); ++i) {
            if (QString::fromStdString(macro->actions(i).uuid().string()) == actionId) {
                macro->mutable_actions()->DeleteSubrange(i, 1);
                return QString();
            }
        }
        return QStringLiteral("That macro no longer has that action");
    });
}

QString Macros::changeMacro(const QString &id, const std::function<QString(Macro *)> &change)
{
    rv::data::MacrosDocument document;
    QString error = read(&document);
    if (!error.isEmpty())
        return error;
    const int index = macroIndex(document, id.toStdString());
    if (index < 0)
        return macroGone;
    error = change(document.mutable_macros(index));
    return error.isEmpty() ? write(&document) : error;
}
