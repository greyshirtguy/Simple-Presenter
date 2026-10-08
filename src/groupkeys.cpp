#include "groupkeys.h"

#include "groups.pb.h"
#include "keymapping.pb.h"
#include "proconvert.h"
#include "workspacefiles.h"

#include <QColor>
#include <QFile>

namespace {

const QString what = QStringLiteral("the groups");

// A hotkey as a capital letter or a digit, and back. ProPresenter numbers the keys of
// a keyboard: the letters first, then the digits from 0. Any other key is not one a
// group's hotkey is taken to be here, and is "".
QString keyOf(const rv::data::HotKey &hotKey)
{
    const int code = int(hotKey.code());
    if (code >= rv::data::KEY_CODE_ANSI_A && code <= rv::data::KEY_CODE_ANSI_Z)
        return QString(QChar('A' + code - rv::data::KEY_CODE_ANSI_A));
    if (code >= rv::data::KEY_CODE_ANSI_0 && code <= rv::data::KEY_CODE_ANSI_9)
        return QString(QChar('0' + code - rv::data::KEY_CODE_ANSI_0));
    return {};
}

rv::data::KeyCode codeOf(const QString &key)
{
    const ushort c = key.size() == 1 ? key.at(0).toUpper().unicode() : 0;
    if (c >= 'A' && c <= 'Z')
        return rv::data::KeyCode(rv::data::KEY_CODE_ANSI_A + c - 'A');
    if (c >= '0' && c <= '9')
        return rv::data::KeyCode(rv::data::KEY_CODE_ANSI_0 + c - '0');
    return rv::data::KEY_CODE_UNKNOWN;
}

QString lower(const std::string &name)
{
    return QString::fromStdString(name).trimmed().toLower();
}

// The key of a mapping that is of a plain letter or digit to a group, else "".
QString mappedKey(const rv::data::KeyMapping &mapping)
{
    if (!mapping.has_group_identifier() || !mapping.has_keyboard() || mapping.keyboard().key_equivalent_modifier_flags_size() > 0)
        return {};
    const QString key = QString::fromStdString(mapping.keyboard().key_equivalent()).toUpper();
    return codeOf(key) != rv::data::KEY_CODE_UNKNOWN ? key : QString();
}

} // namespace

QString GroupKeys::path() const
{
    return m_workspace.isEmpty() ? QString() : m_workspace + QStringLiteral("/Configuration/Groups");
}

QString GroupKeys::open(const QString &workspace)
{
    m_workspace = workspace;
    return reload();
}

QString GroupKeys::reload()
{
    m_exists = QFile::exists(path());
    m_keys.clear();
    m_mappedKeys.clear();
    QString error;

    rv::data::ProGroupsDocument groups;
    if (m_exists && (error = workspace::readMessage(path(), &groups, what)).isEmpty()) {
        for (const rv::data::Group &group : groups.groups()) {
            const QString key = keyOf(group.hotkey());
            if (!key.isEmpty() && !lower(group.name()).isEmpty())
                m_keys.append(QVariantMap {{"name", lower(group.name())}, {"label", QString::fromStdString(group.name()).trimmed()},
                                           {"key", key}});
        }
    }

    // The mappings for every kind of machine, then those for one kind
    const QString mappingsPath = m_workspace + QStringLiteral("/Configuration/KeyMappings");
    rv::data::KeyMappingDocument mappings;
    if (!m_workspace.isEmpty() && QFile::exists(mappingsPath)) {
        const QString mappingsError = workspace::readMessage(mappingsPath, &mappings, QStringLiteral("the key mappings"));
        if (error.isEmpty())
            error = mappingsError;
        if (mappingsError.isEmpty()) {
            for (const auto *list : {&mappings.keymappings(), &mappings.macos_keymappings(), &mappings.windows_keymappings()}) {
                for (const rv::data::KeyMapping &mapping : *list) {
                    const QString key = mappedKey(mapping);
                    const QString name = lower(mapping.group_identifier().parameter_name());
                    if (!key.isEmpty() && !name.isEmpty())
                        m_mappedKeys.append(QVariantMap {
                            {"name", name},
                            {"label", QString::fromStdString(mapping.group_identifier().parameter_name()).trimmed()},
                            {"key", key},
                        });
                }
            }
        }
    }
    emit changed();
    return error;
}

QString GroupKeys::setKeys(const QVariantList &groups)
{
    if (m_workspace.isEmpty())
        return {};
    const bool making = !QFile::exists(path());
    rv::data::ProGroupsDocument document;
    if (!making) {
        const QString error = workspace::readMessage(path(), &document, what);
        if (!error.isEmpty())
            return error;
    }

    for (const QVariant &entry : groups) {
        const QVariantMap wanted = entry.toMap();
        const QString name = wanted.value("name").toString().trimmed();
        const QString key = wanted.value("key").toString();
        if (name.isEmpty())
            continue;
        rv::data::Group *found = nullptr;
        for (rv::data::Group &group : *document.mutable_groups()) {
            if (!found && lower(group.name()) == name.toLower())
                found = &group;
        }
        if (!found) {
            // A group the list does not have joins it if it has a hotkey to be kept
            // there, or if the list is being made, which is then of every group.
            if (key.isEmpty() && !making)
                continue;
            found = document.add_groups();
            found->mutable_uuid()->set_string(workspace::newUuid());
            found->set_name(name.toStdString());
            proconvert::setColor(found->mutable_color(), QColor(wanted.value("color").toString()));
        }
        found->mutable_hotkey()->set_code(codeOf(key));
    }

    const QString error = workspace::writeMessage(path(), document, what);
    if (error.isEmpty())
        reload();
    return error;
}
