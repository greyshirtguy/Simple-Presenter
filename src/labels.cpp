#include "labels.h"

#include "labelfile.h"

QString Labels::open(const QString &workspace)
{
    m_workspace = workspace;
    return reload();
}

QString Labels::reload()
{
    QString error;
    m_presets.clear();
    if (!m_workspace.isEmpty()) {
        const QList<labelfile::Label> labels = labelfile::presets(m_workspace, &error);
        for (const labelfile::Label &label : labels)
            m_presets.append(QVariantMap {{"text", label.text}, {"color", label.color}});
    }
    emit changed();
    return error;
}

QString Labels::add(const QString &text, const QString &color)
{
    const QString error = labelfile::add(m_workspace, {text.trimmed(), color});
    reload();
    return error;
}

QString Labels::change(int index, const QString &text, const QString &color)
{
    const QString error = labelfile::change(m_workspace, index, {text.trimmed(), color});
    reload();
    return error;
}

QString Labels::remove(int index)
{
    const QString error = labelfile::remove(m_workspace, index);
    reload();
    return error;
}
