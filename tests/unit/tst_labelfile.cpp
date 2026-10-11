// Slide labels (src/labelfile.h): a slide's own, in its cue, and the labels a workspace
// keeps ready, tried on a cue and on files made as ProPresenter writes them.

#include "labelfile.h"

#include "labels.pb.h"

#include <google/protobuf/unknown_field_set.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace labelfile;

namespace {

rv::data::ProLabelsDocument fileOf(const QString &workspace)
{
    rv::data::ProLabelsDocument document;
    QFile file(workspace + "/Configuration/Labels");
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = file.readAll();
        document.ParseFromArray(bytes.constData(), int(bytes.size()));
    }
    return document;
}

// A list as ProPresenter might have left it: two of words alone, one with a colour, one
// whose colour is the wholly see-through white that ProPresenter writes for none, and
// something after the labels that this code knows nothing of.
void writeProPresenters(const QString &workspace)
{
    rv::data::ProLabelsDocument document;
    document.add_labels()->set_text("Blank Slide");
    document.add_labels()->set_text("Instrumental");
    rv::data::Action::Label *coloured = document.add_labels();
    coloured->set_text("Offering");
    coloured->mutable_color()->set_green(0.5f);
    coloured->mutable_color()->set_alpha(1);
    rv::data::Action::Label *clear = document.add_labels();
    clear->set_text("Repeat");
    clear->mutable_color()->set_red(1);
    clear->mutable_color()->set_green(1);
    clear->mutable_color()->set_blue(1);
    document.GetReflection()->MutableUnknownFields(&document)->AddVarint(900, 42);
    std::string bytes;
    document.SerializeToString(&bytes);
    QDir().mkpath(workspace + "/Configuration");
    QFile file(workspace + "/Configuration/Labels");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes.data(), qint64(bytes.size()));
}

// A cue as ProPresenter writes one: a slide, and media under it.
rv::data::Cue cue()
{
    rv::data::Cue made;
    made.mutable_uuid()->set_string("CUE");
    made.set_name("its own name");
    rv::data::Action *slide = made.add_actions();
    slide->mutable_uuid()->set_string("SLIDE");
    slide->set_type(rv::data::Action::ACTION_TYPE_PRESENTATION_SLIDE);
    slide->mutable_slide()->mutable_presentation()->mutable_base_slide()->mutable_uuid()->set_string("BASE");
    rv::data::Action *media = made.add_actions();
    media->mutable_uuid()->set_string("MEDIA");
    media->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
    media->mutable_label()->set_text("a video's own label");
    return made;
}

QString said(const QList<Label> &labels)
{
    QStringList out;
    for (const Label &label : labels)
        out << label.text + (label.color.isEmpty() ? QString() : "/" + label.color);
    return out.join(" | ");
}

} // namespace

class TestLabelFile : public QObject
{
    Q_OBJECT

private slots:
    void aSlideIsGivenALabelOnTheActionThatShowsIt()
    {
        rv::data::Cue made = cue();
        QCOMPARE(of(made).text, "");
        QVERIFY(set(&made, {"Chorus tag", "#ff8000"}));
        QCOMPARE(of(made), (Label {"Chorus tag", "#ff8000"}));
        const rv::data::Action::Label &written = made.actions(0).label();
        QCOMPARE(written.text(), std::string("Chorus tag"));
        QVERIFY(written.color().red() == 1 && qAbs(written.color().green() - 128 / 255.0f) < 0.001f && written.color().blue() == 0 && written.color().alpha() == 1);
        // The cue's own name is another thing, and the media's label is the media's.
        QCOMPARE(made.name(), std::string("its own name"));
        QCOMPARE(made.actions(1).label().text(), std::string("a video's own label"));
        QCOMPARE(made.actions_size(), 2);
    }

    void aLabelWithNoColourHasNoneWritten()
    {
        rv::data::Cue made = cue();
        QVERIFY(set(&made, {"Spoken", "#102030"}));
        QVERIFY(set(&made, {"Spoken", ""}));
        QVERIFY(made.actions(0).has_label() && !made.actions(0).label().has_color());
        QCOMPARE(of(made), (Label {"Spoken", ""}));
        // What is not a colour is none.
        QVERIFY(set(&made, {"Spoken", "red"}));
        QVERIFY(!made.actions(0).label().has_color());
    }

    void takingALabelOffTakesItOutOfTheCue()
    {
        rv::data::Cue plain = cue();
        const std::string before = plain.SerializeAsString();
        rv::data::Cue made = plain;
        QVERIFY(set(&made, {"Chorus tag", "#ff8000"}));
        QVERIFY(set(&made, {"", ""}));
        QVERIFY(!made.actions(0).has_label());
        // The cue is byte for byte the one that never had a label.
        QCOMPARE(made.SerializeAsString(), before);
        // A colour alone is a label too, as ProPresenter has them.
        QVERIFY(set(&made, {"", "#00ff00"}));
        QVERIFY(made.actions(0).has_label());
        QCOMPARE(of(made), (Label {"", "#00ff00"}));
    }

    void aCueWithNoSlideHasNothingToLabel()
    {
        rv::data::Cue made;
        made.add_actions()->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
        const std::string before = made.SerializeAsString();
        QVERIFY(!set(&made, {"Lost", ""}));
        QCOMPARE(made.SerializeAsString(), before);
        QCOMPARE(of(made).text, "");
    }

    void theLabelsKeptReadyAreReadAsProPresenterWroteThem()
    {
        QTemporaryDir folder;
        QString error;
        // A workspace with no file has none, and that is not an error.
        QCOMPARE(presets(folder.path(), &error).size(), 0);
        QCOMPARE(error, "");
        writeProPresenters(folder.path());
        // (See-through white is no colour.)
        QCOMPARE(said(presets(folder.path(), &error)), "Blank Slide | Instrumental | Offering/#008000 | Repeat");
        QCOMPARE(error, "");
    }

    void addingChangingAndRemovingKeepTheRestOfTheFile()
    {
        QTemporaryDir folder;
        writeProPresenters(folder.path());
        QString error;
        QCOMPARE(add(folder.path(), {"Welcome", "#1e88e5"}), "");
        QCOMPARE(said(presets(folder.path(), &error)), "Blank Slide | Instrumental | Offering/#008000 | Repeat | Welcome/#1e88e5");
        // One the list has already is not there twice; one of no words is not kept.
        QCOMPARE(add(folder.path(), {"Welcome", "#1e88e5"}), "");
        QCOMPARE(presets(folder.path(), &error).size(), 5);
        QVERIFY(!add(folder.path(), {"  ", "#1e88e5"}).isEmpty());
        QCOMPARE(change(folder.path(), 1, {"Instrumental Break", ""}), "");
        QCOMPARE(remove(folder.path(), 0), "");
        QCOMPARE(said(presets(folder.path(), &error)), "Instrumental Break | Offering/#008000 | Repeat | Welcome/#1e88e5");
        // The one that had ProPresenter's see-through white has it still, as written.
        const rv::data::ProLabelsDocument now = fileOf(folder.path());
        QVERIFY(now.labels(2).has_color() && now.labels(2).color().red() == 1 && now.labels(2).color().alpha() == 0);
        // And what this knows nothing of is still in the file.
        const google::protobuf::UnknownFieldSet &unknown = now.GetReflection()->GetUnknownFields(now);
        QCOMPARE(unknown.field_count(), 1);
        QCOMPARE(unknown.field(0).number(), 900);
        // A place the list does not have changes nothing.
        QVERIFY(!change(folder.path(), 9, {"Lost", ""}).isEmpty());
        QVERIFY(!remove(folder.path(), -1).isEmpty());
        QCOMPARE(presets(folder.path(), &error).size(), 4);
    }

    void theFirstLabelKeptMakesTheFile()
    {
        QTemporaryDir folder;
        QVERIFY(!QFile::exists(folder.path() + "/Configuration/Labels"));
        QCOMPARE(add(folder.path(), {"Welcome", ""}), "");
        QString error;
        QCOMPARE(said(presets(folder.path(), &error)), "Welcome");
        QVERIFY(!fileOf(folder.path()).labels(0).has_color());
    }
};

QTEST_APPLESS_MAIN(TestLabelFile)
#include "tst_labelfile.moc"
