#include "stagelayouts.h"

#include "proconvert.h"
#include "richtext.h"
#include "show.h"
#include "workspacefiles.h"

#include <QFile>
#include <QRectF>
#include <QSet>

namespace {

using Layout = rv::data::Stage::Layout;

const QString what = QStringLiteral("the stage layouts");
const QString layoutGone = QStringLiteral("There is no longer such a layout");

int layoutIndex(const rv::data::Stage::Document &document, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (int i = 0; i < document.layouts_size(); ++i) {
        if (document.layouts(i).uuid().string() == wanted)
            return i;
    }
    return -1;
}

QString freeName(const rv::data::Stage::Document &document, const QString &base)
{
    QSet<QString> taken;
    for (const Layout &layout : document.layouts())
        taken.insert(QString::fromStdString(layout.name()));
    QString name = base;
    for (int n = 2; taken.contains(name); ++n)
        name = QStringLiteral("%1 %2").arg(base).arg(n);
    return name;
}

// A text box of a new layout: where it is, how its text looks, and which slide's words
// it shows. Its own text is its name, which is what the editor shows in it while there
// are no such words to show, as ProPresenter's own layouts have it.
void addWords(rv::data::Slide *slide, const QString &name, const QRectF &box, qreal size, const QColor &colour, bool next)
{
    rv::data::Slide::Element element = proconvert::makeTextElement(*slide, nullptr);
    rv::data::Graphics::Text *text = element.mutable_element()->mutable_text();
    TextRun format = proconvert::readText(*text).firstRun();
    format.applyFormat({{"size", size}, {"bold", true}, {"color", colour}});
    proconvert::writeText(text, RichText::plain(name, format, Qt::AlignHCenter));
    const QString id = QString::fromStdString(element.element().uuid().string());
    *slide->add_elements() = element;
    proconvert::applyChanges(slide, id, {
        {"name", name}, {"x", box.x()}, {"y", box.y()}, {"width", box.width()}, {"height", box.height()},
        {"linkKind", QStringLiteral("slideText")}, {"linkSlideNext", next}, {"linkSlideSource", int(Show::Words)},
        // However many words there are, they are made to fit the box.
        {"textScale", int(rv::data::Graphics::Text::SCALE_BEHAVIOR_SCALE_FONT_DOWN)},
    });
}

} // namespace

QString StageLayouts::path() const
{
    return m_workspace.isEmpty() ? QString() : m_workspace + QStringLiteral("/Configuration/Stage");
}

QString StageLayouts::open(const QString &workspace)
{
    m_workspace = workspace;
    return reload();
}

QString StageLayouts::reload()
{
    rv::data::Stage::Document document;
    const QString error = read(&document);
    if (!error.isEmpty())
        document.Clear();
    show(document);
    return error;
}

// A workspace with no file of them has no layouts, which is not an error.
QString StageLayouts::read(rv::data::Stage::Document *document) const
{
    if (!QFile::exists(path())) {
        document->Clear();
        return {};
    }
    return workspace::readMessage(path(), document, what);
}

QString StageLayouts::write(const rv::data::Stage::Document &document)
{
    const QString error = workspace::writeMessage(path(), document, what);
    if (error.isEmpty())
        show(document);
    return error;
}

void StageLayouts::show(const rv::data::Stage::Document &document)
{
    m_layouts.clear();
    for (const Layout &layout : document.layouts()) {
        const QString name = QString::fromStdString(layout.name());
        m_layouts.append(QVariantMap {
            {"id", QString::fromStdString(layout.uuid().string())},
            {"name", name},
            {"slide", proconvert::toSlideMap(layout.slide(), name)},
        });
    }
    emit changed();
}

QVariantMap StageLayouts::find(const QString &id) const
{
    for (const QVariant &entry : m_layouts) {
        const QVariantMap layout = entry.toMap();
        if (layout.value("id").toString() == id)
            return layout;
    }
    return {};
}

QVariantMap StageLayouts::add()
{
    rv::data::Stage::Document document;
    QString error = read(&document);
    if (!error.isEmpty())
        return {{"id", QString()}, {"error", error}};

    const std::string id = workspace::newUuid();
    Layout *layout = document.add_layouts();
    layout->mutable_uuid()->set_string(id);
    layout->set_name(freeName(document, QStringLiteral("Layout")).toStdString());
    // Black, and the size of an ordinary screen: the stage display scales it to fit.
    rv::data::Slide *slide = layout->mutable_slide();
    slide->set_draws_background_color(true);
    proconvert::setColor(slide->mutable_background_color(), Qt::black);
    slide->mutable_size()->set_width(1920);
    slide->mutable_size()->set_height(1080);
    slide->mutable_uuid()->set_string(workspace::newUuid());
    addWords(slide, QStringLiteral("Current Slide"), QRectF(60, 40, 1800, 500), 130, QColor(0xff, 0xd4, 0x00), false);
    addWords(slide, QStringLiteral("Next Slide"), QRectF(60, 580, 1800, 460), 100, QColor(0x9a, 0x9d, 0xa3), true);

    error = write(document);
    return {{"id", error.isEmpty() ? QString::fromStdString(id) : QString()}, {"error", error}};
}

QVariantMap StageLayouts::duplicate(const QString &id)
{
    rv::data::Stage::Document document;
    QString error = read(&document);
    if (!error.isEmpty())
        return {{"id", QString()}, {"error", error}};
    const int index = layoutIndex(document, id);
    if (index < 0)
        return {{"id", QString()}, {"error", layoutGone}};

    Layout copy = document.layouts(index);
    const std::string copyId = workspace::newUuid();
    copy.mutable_uuid()->set_string(copyId);
    copy.set_name(freeName(document, QString::fromStdString(copy.name())).toStdString());
    copy.mutable_slide()->mutable_uuid()->set_string(workspace::newUuid());
    // After the one it is a copy of.
    *document.add_layouts() = copy;
    for (int at = document.layouts_size() - 1; at > index + 1; --at)
        document.mutable_layouts()->SwapElements(at, at - 1);

    error = write(document);
    return {{"id", error.isEmpty() ? QString::fromStdString(copyId) : QString()}, {"error", error}};
}

QString StageLayouts::rename(const QString &id, const QString &name)
{
    rv::data::Stage::Document document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    const int index = layoutIndex(document, id);
    if (index < 0)
        return layoutGone;
    if (name.trimmed().isEmpty())
        return {};
    document.mutable_layouts(index)->set_name(name.trimmed().toStdString());
    return write(document);
}

QString StageLayouts::remove(const QString &id)
{
    rv::data::Stage::Document document;
    const QString error = read(&document);
    if (!error.isEmpty())
        return error;
    const int index = layoutIndex(document, id);
    if (index < 0)
        return layoutGone;
    document.mutable_layouts()->DeleteSubrange(index, 1);
    return write(document);
}
