#include "presentationeditor.h"

#include "proconvert.h"
#include "richtext.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

namespace {

// How many copies of a presentation from before editing sessions are kept.
const int backupsKept = 20;
// How many changes can be undone.
const int undoLimit = 200;

rv::data::Slide::Element *findElement(rv::data::Slide *slide, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (rv::data::Slide::Element &element : *slide->mutable_elements()) {
        if (element.element().uuid().string() == wanted)
            return &element;
    }
    return nullptr;
}

int indexOfElement(const rv::data::Slide &slide, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (int i = 0; i < slide.elements_size(); ++i) {
        if (slide.elements(i).element().uuid().string() == wanted)
            return i;
    }
    return -1;
}

// The first slide a cue shows, as an index into its actions, or -1.
int slideAction(const rv::data::Cue &cue)
{
    for (int i = 0; i < cue.actions_size(); ++i) {
        const rv::data::Action &action = cue.actions(i);
        if (action.isenabled() && action.has_slide() && action.slide().has_presentation())
            return i;
    }
    return -1;
}

const QString gone = QStringLiteral("That slide is no longer in the presentation.");
const QString elementGone = QStringLiteral("That element is no longer on the slide.");

} // namespace

PresentationEditor::PresentationEditor(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PresentationEditor::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant PresentationEditor::data(const QModelIndex &index, int role) const
{
    if (role != SlideRole || index.row() < 0 || index.row() >= m_slides.size())
        return {};
    return m_slides.at(index.row());
}

QHash<int, QByteArray> PresentationEditor::roleNames() const
{
    return {{SlideRole, "slide"}};
}

QString PresentationEditor::open(const QString &path, const QString &workspace)
{
    rv::data::Presentation presentation;
    QString error;
    if (!proconvert::readPresentation(path, &presentation, &error))
        return error;

    beginResetModel();
    m_presentation = presentation;
    m_path = path;
    m_workspace = workspace;
    m_name = QFileInfo(path).completeBaseName();
    m_undo.clear();
    m_redo.clear();
    m_changed = false;
    m_backupPath.clear();
    m_previewRow = -1;
    m_previewBefore.clear();

    // Every cue that shows a slide, group by group as the presentation stores them,
    // then any cue that no group has.
    m_rows.clear();
    QHash<std::string, int> cueIndexes;
    for (int i = 0; i < m_presentation.cues_size(); ++i)
        cueIndexes.insert(m_presentation.cues(i).uuid().string(), i);
    QSet<int> placed;
    const auto place = [this, &placed](int cue, const QString &group, const QString &color, bool start) {
        const rv::data::Cue &candidate = m_presentation.cues(cue);
        const int action = slideAction(candidate);
        if (placed.contains(cue) || !candidate.isenabled() || action < 0)
            return false;
        placed.insert(cue);
        m_rows.append({cue, action, group, color, start});
        return true;
    };
    for (const auto &group : m_presentation.cue_groups()) {
        const rv::data::Color &color = group.group().color();
        const QString colorName = group.group().has_color() && color.alpha() > 0 ? proconvert::toColor(color).name()
                                                                                  : QString();
        bool first = true;
        for (const rv::data::UUID &id : group.cue_identifiers()) {
            const auto cue = cueIndexes.constFind(id.string());
            if (cue != cueIndexes.constEnd() && place(*cue, QString::fromStdString(group.group().name()), colorName, first))
                first = false;
        }
    }
    for (int i = 0; i < m_presentation.cues_size(); ++i)
        place(i, QString(), QString(), false);

    m_slides.clear();
    for (int row = 0; row < m_rows.size(); ++row)
        m_slides.append(describe(row));
    endResetModel();

    emit documentChanged();
    emit historyChanged();
    return {};
}

void PresentationEditor::close()
{
    beginResetModel();
    m_presentation.Clear();
    m_path.clear();
    m_name.clear();
    m_rows.clear();
    m_slides.clear();
    m_undo.clear();
    m_redo.clear();
    m_changed = false;
    m_backupPath.clear();
    m_previewRow = -1;
    m_previewBefore.clear();
    endResetModel();
    emit documentChanged();
    emit historyChanged();
}

QVariantMap PresentationEditor::slideAt(int row) const
{
    return row >= 0 && row < m_slides.size() ? m_slides.at(row).toMap() : QVariantMap();
}

int PresentationEditor::rowOf(const QString &slideId) const
{
    const std::string wanted = slideId.toStdString();
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_presentation.cues(m_rows.at(row).cue).uuid().string() == wanted)
            return row;
    }
    return -1;
}

rv::data::Cue *PresentationEditor::cueAt(int row)
{
    return row >= 0 && row < m_rows.size() ? m_presentation.mutable_cues(m_rows.at(row).cue) : nullptr;
}

rv::data::Slide *PresentationEditor::slideIn(int row)
{
    rv::data::Cue *cue = cueAt(row);
    if (!cue)
        return nullptr;
    // A change to the cue undone or redone may have altered its actions.
    const int action = slideAction(*cue);
    return action < 0 ? nullptr
                      : cue->mutable_actions(action)->mutable_slide()->mutable_presentation()->mutable_base_slide();
}

QVariantMap PresentationEditor::describe(int row) const
{
    const Row &place = m_rows.at(row);
    const rv::data::Cue &cue = m_presentation.cues(place.cue);
    const int index = slideAction(cue);
    if (index < 0)
        return {};
    const rv::data::Action &action = cue.actions(index);
    QVariantMap slide = proconvert::toSlideMap(action.slide().presentation().base_slide(),
                                               QString::fromStdString(action.label().text()));
    slide.insert("id", QString::fromStdString(cue.uuid().string()));
    slide.insert("group", place.group);
    slide.insert("groupColor", place.groupColor);
    slide.insert("groupStart", place.groupStart);
    return slide;
}

void PresentationEditor::refresh(int row)
{
    if (row < 0 || row >= m_slides.size())
        return;
    m_slides[row] = describe(row);
    emit dataChanged(index(row), index(row), {SlideRole});
    emit slideChanged(row);
}

// The file as it was before this session's first change goes into a folder of the app's
// own, under the workspace's name and the file's place in it, with the time in its
// name. Only so many are kept for each presentation.
void PresentationEditor::backUp()
{
    if (m_changed || !m_backupPath.isEmpty())
        return;
    const QFileInfo file(m_path);
    QString place = file.dir().dirName();
    if (!m_workspace.isEmpty()) {
        const QString relative = QDir(m_workspace).relativeFilePath(file.absolutePath());
        if (!relative.startsWith(QLatin1String("..")))
            place = QFileInfo(m_workspace).fileName() + u'/' + relative;
    }
    const QDir directory(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                         + QStringLiteral("/Edit Backups/") + place);
    if (!directory.mkpath(QStringLiteral("."))) {
        qWarning("Cannot create %s", qPrintable(directory.path()));
        return;
    }
    const QString base = file.completeBaseName();
    const QString target = directory.filePath(base + QDateTime::currentDateTime().toString(QStringLiteral(" yyyy-MM-dd HH.mm.ss"))
                                              + QStringLiteral(".pro"));
    if (!QFile::exists(target) && !QFile::copy(m_path, target)) {
        qWarning("Cannot copy %s to %s", qPrintable(m_path), qPrintable(target));
        return;
    }
    m_backupPath = target;

    // Their names sort by time, so the oldest come first.
    const QStringList copies = directory.entryList({base + QStringLiteral(" ????-??-?? ??.??.??.pro")}, QDir::Files, QDir::Name);
    for (qsizetype i = 0; i < copies.size() - backupsKept; ++i)
        QFile::remove(directory.filePath(copies.at(i)));
}

// Saves the cue in a row as it now is, as one change from how it was `before`. If the
// file cannot be written, the cue goes back to how it was.
QString PresentationEditor::commit(int row, const std::string &before)
{
    rv::data::Cue *cue = cueAt(row);
    if (!cue)
        return gone;
    const std::string after = cue->SerializeAsString();
    if (after == before) {
        refresh(row);
        return {};
    }
    backUp();
    const QString error = proconvert::writePresentation(m_path, m_presentation);
    if (!error.isEmpty()) {
        cue->ParseFromString(before);
        refresh(row);
        return error;
    }
    m_changed = true;
    m_undo.append({row, before, after});
    if (m_undo.size() > undoLimit)
        m_undo.removeFirst();
    m_redo.clear();
    refresh(row);
    emit historyChanged();
    return {};
}

// Makes a change to the slide in a row and saves it. `apply` is given the slide and
// returns an error message, empty if it made the change.
template <typename Change>
QString PresentationEditor::change(int row, Change apply)
{
    // A preview left under way becomes a change of its own first.
    if (m_previewRow >= 0)
        commitPreview();
    rv::data::Cue *cue = cueAt(row);
    rv::data::Slide *slide = slideIn(row);
    if (!cue || !slide)
        return gone;
    const std::string before = cue->SerializeAsString();
    const QString error = apply(slide);
    if (!error.isEmpty()) {
        cue->ParseFromString(before);
        return error;
    }
    return commit(row, before);
}

template <typename Change>
void PresentationEditor::preview(int row, Change apply)
{
    if (m_previewRow >= 0 && m_previewRow != row)
        commitPreview();
    rv::data::Cue *cue = cueAt(row);
    rv::data::Slide *slide = slideIn(row);
    if (!cue || !slide)
        return;
    if (m_previewRow < 0) {
        m_previewRow = row;
        m_previewBefore = cue->SerializeAsString();
    }
    apply(slide);
    refresh(row);
}

QString PresentationEditor::commitPreview()
{
    if (m_previewRow < 0)
        return {};
    const int row = m_previewRow;
    const std::string before = m_previewBefore;
    m_previewRow = -1;
    m_previewBefore.clear();
    return commit(row, before);
}

void PresentationEditor::cancelPreview()
{
    if (m_previewRow < 0)
        return;
    const int row = m_previewRow;
    if (rv::data::Cue *cue = cueAt(row))
        cue->ParseFromString(m_previewBefore);
    m_previewRow = -1;
    m_previewBefore.clear();
    refresh(row);
}

QString PresentationEditor::setProperties(int row, const QString &element, const QVariantMap &changes)
{
    return change(row, [&](rv::data::Slide *slide) {
        return proconvert::applyChanges(slide, element, changes) ? QString() : elementGone;
    });
}

void PresentationEditor::previewProperties(int row, const QString &element, const QVariantMap &changes)
{
    preview(row, [&](rv::data::Slide *slide) { proconvert::applyChanges(slide, element, changes); });
}

QString PresentationEditor::setText(int row, const QString &element, const QVariant &richText)
{
    return change(row, [&](rv::data::Slide *slide) {
        rv::data::Slide::Element *target = findElement(slide, element);
        if (!target)
            return elementGone;
        proconvert::writeText(target->mutable_element()->mutable_text(), richText.value<RichText>());
        return QString();
    });
}

QString PresentationEditor::formatText(int row, const QString &element, int start, int end, const QVariantMap &format)
{
    return change(row, [&](rv::data::Slide *slide) {
        rv::data::Slide::Element *target = findElement(slide, element);
        if (!target)
            return elementGone;
        rv::data::Graphics::Text *text = target->mutable_element()->mutable_text();
        proconvert::writeText(text, proconvert::readText(*text).formatted(start, end, format));
        return QString();
    });
}

void PresentationEditor::previewFormat(int row, const QString &element, int start, int end, const QVariantMap &format)
{
    preview(row, [&](rv::data::Slide *slide) {
        if (rv::data::Slide::Element *target = findElement(slide, element)) {
            rv::data::Graphics::Text *text = target->mutable_element()->mutable_text();
            proconvert::writeText(text, proconvert::readText(*text).formatted(start, end, format));
        }
    });
}

QVariantMap PresentationEditor::addText(int row, const QString &like)
{
    QString id;
    const QString error = change(row, [&](rv::data::Slide *slide) {
        // Set like the element asked for, or failing that like the slide's first text.
        const rv::data::Slide::Element *model = findElement(slide, like);
        for (int i = 0; !model && i < slide->elements_size(); ++i) {
            if (!slide->elements(i).element().text().rtf_data().empty())
                model = &slide->elements(i);
        }
        const rv::data::Slide::Element added = proconvert::makeTextElement(*slide, model);
        id = QString::fromStdString(added.element().uuid().string());
        *slide->add_elements() = added;
        return QString();
    });
    return {{"id", error.isEmpty() ? id : QString()}, {"error", error}};
}

QVariantMap PresentationEditor::duplicate(int row, const QString &element)
{
    QString id;
    const QString error = change(row, [&](rv::data::Slide *slide) {
        const int index = indexOfElement(*slide, element);
        if (index < 0)
            return elementGone;
        rv::data::Slide::Element copy = slide->elements(index);
        rv::data::Graphics::Element *graphics = copy.mutable_element();
        graphics->mutable_uuid()->set_string(proconvert::newUuid());
        graphics->set_name(proconvert::uniqueElementName(*slide, QString::fromStdString(graphics->name())).toStdString());
        graphics->mutable_bounds()->mutable_origin()->set_x(graphics->bounds().origin().x() + 30);
        graphics->mutable_bounds()->mutable_origin()->set_y(graphics->bounds().origin().y() + 30);
        // Builds belong to the element they were made for.
        copy.clear_build_in();
        copy.clear_build_out();
        copy.clear_childbuilds();
        id = QString::fromStdString(graphics->uuid().string());
        *slide->add_elements() = copy;
        return QString();
    });
    return {{"id", error.isEmpty() ? id : QString()}, {"error", error}};
}

QString PresentationEditor::remove(int row, const QString &element)
{
    return change(row, [&](rv::data::Slide *slide) {
        const int index = indexOfElement(*slide, element);
        if (index < 0)
            return elementGone;
        slide->mutable_elements()->DeleteSubrange(index, 1);
        // It has no place in the order builds run in either.
        const std::string id = element.toStdString();
        for (int i = slide->element_build_order_size() - 1; i >= 0; --i) {
            if (slide->element_build_order(i).string() == id)
                slide->mutable_element_build_order()->DeleteSubrange(i, 1);
        }
        return QString();
    });
}

QString PresentationEditor::move(int row, const QString &element, int index)
{
    return change(row, [&](rv::data::Slide *slide) {
        int from = indexOfElement(*slide, element);
        if (from < 0)
            return elementGone;
        const int to = qBound(0, index, slide->elements_size() - 1);
        // Step by step, each a swap with its neighbour.
        for (; from < to; ++from)
            slide->mutable_elements()->SwapElements(from, from + 1);
        for (; from > to; --from)
            slide->mutable_elements()->SwapElements(from, from - 1);
        return QString();
    });
}

// Takes the last step off one list, puts its cue as it was at the other end of that
// step, saves, and puts the step on the other list.
QString PresentationEditor::restore(QList<Step> *from, QList<Step> *to, bool forwards)
{
    if (m_previewRow >= 0)
        commitPreview();
    if (from->isEmpty())
        return {};
    const Step step = from->last();
    rv::data::Cue *cue = cueAt(step.row);
    if (!cue)
        return gone;
    cue->ParseFromString(forwards ? step.after : step.before);
    const QString error = proconvert::writePresentation(m_path, m_presentation);
    if (!error.isEmpty()) {
        cue->ParseFromString(forwards ? step.before : step.after);
        return error;
    }
    from->removeLast();
    to->append(step);
    refresh(step.row);
    emit historyChanged();
    emit restored(step.row);
    return {};
}

QString PresentationEditor::undo()
{
    return restore(&m_undo, &m_redo, false);
}

QString PresentationEditor::redo()
{
    return restore(&m_redo, &m_undo, true);
}
