// ProPresenter's builds (src/builds.h) and its transitions as its files know them
// (src/transitionkinds.h), tried on a slide laid out as ProPresenter wrote one in the
// maintainer's Names Library (a lower third: a name wiped in with the slide, its two
// lines with it a moment later, all wiped out again after five seconds), with fields
// this app knows nothing of, which must come back as they went.

#include "builds.h"
#include "transitionkinds.h"

#include <google/protobuf/unknown_field_set.h>

#include <QColor>
#include <QTest>

namespace {

using Build = rv::data::Slide::Element::Build;

const QString wipe = QStringLiteral("7084A71F-C12C-4EEA-8D90-A2D30FB0B256");
const QString dissolve = QStringLiteral("EC52A828-AD85-4602-B70C-1DEE7C904DB6");
const QString push = QStringLiteral("B400F82E-CF9D-4700-89FD-F3889DAA06C5");
const QString colorBurn = QStringLiteral("2B855B65-6ABC-4F65-8198-A19A5EF97821");

// A Wipe as ProPresenter writes one, coming from `direction` (the file's bit)
rv::data::Transition wipeFrom(int direction, const char *effectId)
{
    rv::data::Transition transition;
    transition.set_duration(0.60000002384185791);
    rv::data::Effect *effect = transition.mutable_effect();
    effect->mutable_uuid()->set_string(effectId);
    effect->set_name("Wipe");
    effect->set_render_id(wipe.toStdString());
    effect->set_behavior_description("Wipe");
    effect->set_category("Wipes");
    rv::data::Effect::EffectVariable *variable = effect->add_variables();
    variable->set_name("Direction");
    variable->set_description("Direction of Wipe");
    variable->mutable_direction()->set_direction(rv::data::Effect::EffectVariable::EffectDirection::EffectDirection(direction));
    variable->mutable_direction()->set_default_direction(rv::data::Effect::EffectVariable::EffectDirection::EFFECT_DIRECTION_TOP);
    variable->mutable_direction()->set_available_directions(495);
    return transition;
}

void setBuild(Build *build, const char *id, const char *element, Build::Start start, double delay, rv::data::Transition transition)
{
    build->mutable_uuid()->set_string(id);
    build->mutable_elementuuid()->set_string(element);
    build->set_start(start);
    if (delay > 0)
        build->set_delaytime(delay);
    *build->mutable_transition() = std::move(transition);
}

rv::data::Slide::Element *addElement(rv::data::Slide *slide, const char *id, const char *name)
{
    rv::data::Slide::Element *element = slide->add_elements();
    element->mutable_element()->mutable_uuid()->set_string(id);
    element->mutable_element()->set_name(name);
    element->set_info(3);
    return element;
}

// The lower third, as ProPresenter wrote it (ids shortened)
rv::data::PresentationSlide namesSlide()
{
    rv::data::PresentationSlide presentationSlide;
    rv::data::Slide *slide = presentationSlide.mutable_base_slide();
    slide->mutable_uuid()->set_string("SLIDE");
    rv::data::Slide::Element *title = addElement(slide, "E7F4", "Title");
    setBuild(title->mutable_build_in(), "987F", "E7F4", Build::START_WITH_PREVIOUS, 0.2, wipeFrom(128, "5887"));
    setBuild(title->mutable_build_out(), "246B", "E7F4", Build::START_AFTER_PREVIOUS, 5, wipeFrom(2, "B912"));
    rv::data::Slide::Element *role = addElement(slide, "FA95", "Role");
    setBuild(role->mutable_build_in(), "9C03", "FA95", Build::START_WITH_PREVIOUS, 0, wipeFrom(128, "D37E"));
    setBuild(role->mutable_build_out(), "96D2", "FA95", Build::START_WITH_PREVIOUS, 0, wipeFrom(2, "5611"));
    rv::data::Slide::Element *name = addElement(slide, "B51F", "Name");
    setBuild(name->mutable_build_in(), "43CE", "B51F", Build::START_WITH_SLIDE, 0, wipeFrom(8, "02D3"));
    setBuild(name->mutable_build_out(), "E88D", "B51F", Build::START_WITH_PREVIOUS, 5.2, wipeFrom(32, "7A71"));
    addElement(slide, "PLAIN", "Background bar");
    for (const char *id : {"43CE", "987F", "9C03", "246B", "E88D", "96D2"})
        slide->add_element_build_order()->set_string(id);
    // Something of the slide's, and of a build's, that this app knows nothing of
    slide->GetReflection()->MutableUnknownFields(slide)->AddVarint(900, 7);
    Build *build = title->mutable_build_in();
    build->GetReflection()->MutableUnknownFields(build)->AddVarint(901, 8);
    return presentationSlide;
}

std::string bytesOf(const google::protobuf::MessageLite &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

QStringList ids(const QVariantList &builds)
{
    QStringList result;
    for (const QVariant &build : builds)
        result.append(build.toMap().value("id").toString());
    return result;
}

const builds::Planned *planned(const builds::Plan &plan, const QString &id)
{
    for (const builds::Planned &build : plan.builds) {
        if (build.id == id)
            return &build;
    }
    return nullptr;
}

} // namespace

class BuildsTest : public QObject
{
    Q_OBJECT

private slots:
    // The builds read in the order the file gives them, as the Build tab lists them
    void readsWhatProPresenterWrote()
    {
        const rv::data::PresentationSlide slide = namesSlide();
        const QVariantList list = builds::describe(slide.base_slide());
        QCOMPARE(ids(list), QStringList({"43CE", "987F", "9C03", "246B", "E88D", "96D2"}));
        const QVariantMap first = list.at(0).toMap();
        QCOMPARE(first.value("element").toString(), QStringLiteral("B51F"));
        QCOMPARE(first.value("out").toBool(), false);
        QCOMPARE(first.value("start").toString(), QStringLiteral("withSlide"));
        const QVariantMap transition = first.value("transition").toMap();
        QCOMPARE(transition.value("renderId").toString(), wipe);
        QCOMPARE(transition.value("name").toString(), QStringLiteral("Wipe"));
        // From the left: the third of nine, numbered from the top left
        QCOMPARE(transition.value("values").toMap().value("Direction").toInt(), 3);
        QCOMPARE(list.at(3).toMap().value("start").toString(), QStringLiteral("afterPrevious"));
        QCOMPARE(list.at(3).toMap().value("delay").toDouble(), 5.0);
        QCOMPARE(list.at(3).toMap().value("out").toBool(), true);
    }

    // When each runs: the name with the slide, the two lines a fifth of a second on, the
    // title out five seconds after their wipes end; the name With Previous with 5.2 s,
    // which is timed from when the title was set going, before its delay, and so goes
    // 0.2 s after it, not 5.2 s; and the role With Previous with none, which goes with
    // the name (the maintainer's correction: With Previous had been timed from when the
    // build before began, its delay and all)
    void plansLikeAPowerPointTimeline()
    {
        const builds::Plan plan = builds::plan(namesSlide().base_slide());
        QCOMPARE(plan.steps, 0);
        QCOMPARE(plan.builds.size(), 6);
        QCOMPARE(planned(plan, "43CE")->at, 0.0);
        QCOMPARE(planned(plan, "987F")->at, 0.2);
        QCOMPARE(planned(plan, "9C03")->at, 0.2);
        QVERIFY(qAbs(planned(plan, "246B")->at - (0.2 + 0.6 + 5)) < 1e-6);
        QVERIFY(qAbs(planned(plan, "E88D")->at - (0.2 + 0.6 + 5.2)) < 1e-6);
        QVERIFY(qAbs(planned(plan, "96D2")->at - planned(plan, "E88D")->at) < 1e-9);
        for (const builds::Planned &build : plan.builds)
            QCOMPARE(build.step, 0);
        // As the slide's map has it, by element
        int steps = -1;
        const QVariantMap byElement = builds::describePlan(namesSlide().base_slide(), &steps);
        QCOMPARE(steps, 0);
        QCOMPARE(byElement.size(), 3);
        QVERIFY(!byElement.contains("PLAIN"));
        const QVariantMap title = byElement.value("E7F4").toMap();
        QCOMPARE(title.value("inAt").toDouble(), 0.2);
        QVERIFY(qAbs(title.value("outAt").toDouble() - 5.8) < 1e-6);
        QVERIFY(qAbs(byElement.value("B51F").toMap().value("outAt").toDouble() - 6.0) < 1e-6);
        QVERIFY(qAbs(title.value("inDuration").toDouble() - 0.6) < 1e-6);
        QCOMPARE(title.value("inTransition").toMap().value("renderId").toString(), wipe);
    }

    // On Click builds: each starts a step of its own, and what follows it is timed from it
    void onClickMakesSteps()
    {
        rv::data::Slide slide;
        Build *a = addElement(&slide, "A", "A")->mutable_build_in();
        setBuild(a, "a", "A", Build::START_ON_CLICK, 0, wipeFrom(2, "e1"));
        Build *b = addElement(&slide, "B", "B")->mutable_build_in();
        setBuild(b, "b", "B", Build::START_WITH_PREVIOUS, 0.5, wipeFrom(2, "e2"));
        Build *c = addElement(&slide, "C", "C")->mutable_build_in();
        setBuild(c, "c", "C", Build::START_ON_CLICK, 1, wipeFrom(2, "e3"));
        Build *d = slide.mutable_elements(0)->mutable_build_out();
        setBuild(d, "d", "A", Build::START_AFTER_PREVIOUS, 2, wipeFrom(2, "e4"));
        for (const char *id : {"a", "b", "c", "d"})
            slide.add_element_build_order()->set_string(id);
        const builds::Plan plan = builds::plan(slide);
        QCOMPARE(plan.steps, 2);
        QCOMPARE(planned(plan, "a")->step, 1);
        QCOMPARE(planned(plan, "a")->at, 0.0);
        QCOMPARE(planned(plan, "b")->step, 1);
        QCOMPARE(planned(plan, "b")->at, 0.5);
        QCOMPARE(planned(plan, "c")->step, 2);
        QCOMPARE(planned(plan, "c")->at, 1.0);
        QCOMPARE(planned(plan, "d")->step, 2);
        QVERIFY(qAbs(planned(plan, "d")->at - (1 + 0.6 + 2)) < 1e-6);
    }

    // A build With Slide after an On Click one is in the slide's own step, and the On
    // Click after it is a step of its own, not the first one again
    void withSlideBetweenClicks()
    {
        rv::data::Slide slide;
        setBuild(addElement(&slide, "A", "A")->mutable_build_in(), "a", "A", Build::START_ON_CLICK, 0, wipeFrom(2, "e1"));
        setBuild(addElement(&slide, "B", "B")->mutable_build_in(), "b", "B", Build::START_WITH_SLIDE, 0.5, wipeFrom(2, "e2"));
        setBuild(addElement(&slide, "C", "C")->mutable_build_in(), "c", "C", Build::START_ON_CLICK, 0, wipeFrom(2, "e3"));
        setBuild(slide.mutable_elements(2)->mutable_build_out(), "d", "C", Build::START_WITH_PREVIOUS, 1, wipeFrom(2, "e4"));
        for (const char *id : {"a", "b", "c", "d"})
            slide.add_element_build_order()->set_string(id);
        const builds::Plan plan = builds::plan(slide);
        QCOMPARE(plan.steps, 2);
        QCOMPARE(planned(plan, "a")->step, 1);
        QCOMPARE(planned(plan, "b")->step, 0);
        QCOMPARE(planned(plan, "b")->at, 0.5);
        QCOMPARE(planned(plan, "c")->step, 2);
        QCOMPARE(planned(plan, "d")->step, 2);
        QCOMPARE(planned(plan, "d")->at, 1.0);
    }

    // A build the order does not name runs after those it names, element by element
    void unnamedBuildsComeLast()
    {
        rv::data::PresentationSlide presentationSlide = namesSlide();
        rv::data::Slide *slide = presentationSlide.mutable_base_slide();
        slide->mutable_element_build_order()->DeleteSubrange(0, 2);
        QCOMPARE(ids(builds::describe(*slide)), QStringList({"9C03", "246B", "E88D", "96D2", "987F", "43CE"}));
    }

    // Adding a build: On Click, no delay, the transition as ProPresenter makes it, at the
    // end of the order; and only one of each kind for an element
    void adds()
    {
        rv::data::PresentationSlide presentationSlide = namesSlide();
        rv::data::Slide *slide = presentationSlide.mutable_base_slide();
        QString made;
        QCOMPARE(builds::add(slide, "PLAIN", false, {{"renderId", dissolve}}, &made), QString());
        QVERIFY(!made.isEmpty());
        const rv::data::Slide::Element &bar = slide->elements(3);
        QVERIFY(bar.has_build_in());
        QVERIFY(!bar.has_build_out());
        QCOMPARE(bar.build_in().elementuuid().string(), std::string("PLAIN"));
        QCOMPARE(bar.build_in().start(), Build::START_ON_CLICK);
        QCOMPARE(bar.build_in().transition().effect().name(), std::string("Dissolve"));
        QCOMPARE(bar.build_in().transition().effect().category(), std::string("Dissolves"));
        QVERIFY(qAbs(bar.build_in().transition().duration() - transitionkinds::find(dissolve)->duration) < 1e-9);
        QCOMPARE(slide->element_build_order_size(), 7);
        QCOMPARE(QString::fromStdString(slide->element_build_order(6).string()), made);
        QCOMPARE(builds::plan(*slide).steps, 1);
        // Not twice
        const std::string before = bytesOf(presentationSlide);
        QVERIFY(!builds::add(slide, "PLAIN", false, {{"renderId", dissolve}}).isEmpty());
        QVERIFY(!builds::add(slide, "GONE", true, {{"renderId", dissolve}}).isEmpty());
        QVERIFY(!builds::add(slide, "PLAIN", true, {{"renderId", "NOT-A-TRANSITION"}}).isEmpty());
        QCOMPARE(bytesOf(presentationSlide), before);
    }

    // Taking a build off takes it and its place in the order, and nothing else
    void removes()
    {
        rv::data::PresentationSlide presentationSlide = namesSlide();
        QCOMPARE(builds::remove(presentationSlide.mutable_base_slide(), "246B"), QString());
        rv::data::PresentationSlide expected = namesSlide();
        expected.mutable_base_slide()->mutable_elements(0)->clear_build_out();
        expected.mutable_base_slide()->mutable_element_build_order()->DeleteSubrange(3, 1);
        QCOMPARE(bytesOf(presentationSlide), bytesOf(expected));
        QVERIFY(!builds::remove(presentationSlide.mutable_base_slide(), "246B").isEmpty());
    }

    // A build's start, delay and transition changed, and nothing else of it
    void changes()
    {
        rv::data::PresentationSlide presentationSlide = namesSlide();
        rv::data::Slide *slide = presentationSlide.mutable_base_slide();
        QCOMPARE(builds::change(slide, "987F", {{"start", "click"}, {"delay", 1.5},
                                                {"transition", QVariantMap {{"renderId", wipe}, {"values", QVariantMap {{"Direction", 5}}}}}}),
                 QString());
        rv::data::PresentationSlide expected = namesSlide();
        Build *build = expected.mutable_base_slide()->mutable_elements(0)->mutable_build_in();
        build->set_start(Build::START_ON_CLICK);
        build->set_delaytime(1.5);
        build->mutable_transition()->mutable_effect()->mutable_variables(0)->mutable_direction()->set_direction(
            rv::data::Effect::EffectVariable::EffectDirection::EFFECT_DIRECTION_RIGHT);
        QCOMPARE(bytesOf(presentationSlide), bytesOf(expected));
        // Another transition altogether: made as ProPresenter makes it, with its own variables
        QCOMPARE(builds::change(slide, "987F", {{"transition", QVariantMap {{"renderId", colorBurn}, {"duration", 1.0},
                                                                             {"values", QVariantMap {{"Burn Color", QColor(Qt::red)}}}}}}),
                 QString());
        const rv::data::Effect &effect = slide->elements(0).build_in().transition().effect();
        QCOMPARE(effect.name(), std::string("Color Burn"));
        QCOMPARE(effect.render_id(), colorBurn.toStdString());
        QCOMPARE(effect.variables_size(), 1);
        QCOMPARE(effect.variables(0).name(), std::string("Burn Color"));
        QCOMPARE(effect.variables(0).color().color().red(), 1.0f);
        QCOMPARE(slide->elements(0).build_in().transition().duration(), 1.0);
        // Put back as ProPresenter has it to begin with: its colour and its length
        QCOMPARE(builds::change(slide, "987F", {{"transition", QVariantMap {{"renderId", colorBurn}, {"reset", true}}}}), QString());
        QCOMPARE(slide->elements(0).build_in().transition().effect().variables(0).color().color().red(), 0.5f);
        QVERIFY(qAbs(slide->elements(0).build_in().transition().duration() - transitionkinds::find(colorBurn)->duration) < 1e-9);
        // What cannot be, changes nothing
        const std::string before = bytesOf(presentationSlide);
        QVERIFY(!builds::change(slide, "987F", {{"start", "sometime"}}).isEmpty());
        QVERIFY(!builds::change(slide, "987F", {{"delay", -1}}).isEmpty());
        QVERIFY(!builds::change(slide, "987F", {{"transition", QVariantMap {{"renderId", wipe}, {"duration", -2}}}}).isEmpty());
        QVERIFY(!builds::change(slide, "GONE", {{"delay", 1}}).isEmpty());
        QCOMPARE(bytesOf(presentationSlide), before);
    }

    // The order changed, as the Build tab's list is dragged
    void moves()
    {
        rv::data::PresentationSlide presentationSlide = namesSlide();
        rv::data::Slide *slide = presentationSlide.mutable_base_slide();
        QCOMPARE(builds::move(slide, "96D2", 0), QString());
        QCOMPARE(ids(builds::describe(*slide)), QStringList({"96D2", "43CE", "987F", "9C03", "246B", "E88D"}));
        QCOMPARE(builds::move(slide, "96D2", 99), QString());
        QCOMPARE(ids(builds::describe(*slide)), QStringList({"43CE", "987F", "9C03", "246B", "E88D", "96D2"}));
        // Back where it was, the slide is as it came, unknown fields and all
        QCOMPARE(bytesOf(presentationSlide), bytesOf(namesSlide()));
    }

    // The slide's own transition, set and taken off again: the slide as it came
    void slideTransition()
    {
        rv::data::PresentationSlide slide = namesSlide();
        QCOMPARE(builds::setSlideTransition(&slide, {{"renderId", colorBurn}, {"duration", 0.6}}), QString());
        QVERIFY(slide.has_transition());
        QCOMPARE(slide.transition().effect().name(), std::string("Color Burn"));
        QCOMPARE(builds::describeTransition(slide.transition()).value("values").toMap().value("Burn Color").value<QColor>(),
                 QColor::fromRgbF(0.5, 0.5, 0.5, 1));
        QCOMPARE(builds::setSlideTransition(&slide, {}), QString());
        QCOMPARE(bytesOf(slide), bytesOf(namesSlide()));
        QVERIFY(!builds::setSlideTransition(&slide, {{"renderId", "NOT-A-TRANSITION"}}).isEmpty());
        QCOMPARE(bytesOf(slide), bytesOf(namesSlide()));
    }

    // A slide dressed in a theme for a screen takes the theme's builds: a text box that
    // took a theme box's look takes its builds in place of its own (or none, from a box
    // with none), what the theme brought whole keeps the theme's, the slide's own others
    // keep theirs; the theme's order first, then what is left of the slide's
    void takesAThemesBuilds()
    {
        rv::data::Slide theme;
        setBuild(addElement(&theme, "T1", "Name")->mutable_build_in(), "tb1", "T1", Build::START_WITH_SLIDE, 0, wipeFrom(8, "e1"));
        addElement(&theme, "T3", "Role");
        setBuild(addElement(&theme, "T2", "Bar")->mutable_build_out(), "tb2", "T2", Build::START_AFTER_PREVIOUS, 5, wipeFrom(2, "e2"));
        for (const char *id : {"tb1", "tb2"})
            theme.add_element_build_order()->set_string(id);
        // As themefile::dress leaves it: the slide's two text boxes in the theme's
        // looks, the theme's bar whole, and a picture of the slide's own
        rv::data::Slide slide;
        setBuild(addElement(&slide, "S1", "Name")->mutable_build_in(), "sb1", "S1", Build::START_ON_CLICK, 0, wipeFrom(2, "e3"));
        setBuild(addElement(&slide, "S4", "Role")->mutable_build_out(), "sb4", "S4", Build::START_ON_CLICK, 0, wipeFrom(2, "e4"));
        *slide.add_elements() = theme.elements(2);
        setBuild(addElement(&slide, "S3", "Picture")->mutable_build_in(), "sb3", "S3", Build::START_WITH_PREVIOUS, 1, wipeFrom(2, "e5"));
        for (const char *id : {"sb1", "sb4", "sb3"})
            slide.add_element_build_order()->set_string(id);

        builds::takeThemeBuilds(&slide, theme, {{"S1", "T1"}, {"S4", "T3"}});
        QCOMPARE(slide.elements(0).build_in().uuid().string(), std::string("tb1"));
        QCOMPARE(slide.elements(0).build_in().elementuuid().string(), std::string("S1"));
        QCOMPARE(slide.elements(0).build_in().start(), Build::START_WITH_SLIDE);
        QVERIFY(!slide.elements(0).has_build_out());
        QVERIFY(!slide.elements(1).has_build_in());
        QVERIFY(!slide.elements(1).has_build_out());
        QCOMPARE(slide.elements(2).build_out().uuid().string(), std::string("tb2"));
        QCOMPARE(slide.elements(3).build_in().uuid().string(), std::string("sb3"));
        QCOMPARE(ids(builds::describe(slide)), QStringList({"tb1", "tb2", "sb3"}));
        QCOMPARE(slide.element_build_order_size(), 3);
        // And the plan of it: the name with the slide, the bar five seconds after it, and
        // the picture With Previous a second after the bar was set going
        const builds::Plan plan = builds::plan(slide);
        QCOMPARE(plan.steps, 0);
        QCOMPARE(planned(plan, "tb1")->element, QStringLiteral("S1"));
        QVERIFY(qAbs(planned(plan, "tb2")->at - 5.6) < 1e-6);
        QVERIFY(qAbs(planned(plan, "sb3")->at - 1.6) < 1e-6);
    }

    // ProPresenter's 43, each once, by name; Cut among them; the text animations apart
    void catalogue()
    {
        const QList<transitionkinds::Transition> &all = transitionkinds::all();
        QCOMPARE(all.size(), 43);
        QSet<QString> seen;
        int text = 0;
        for (int i = 0; i < all.size(); ++i) {
            seen.insert(all.at(i).renderId);
            if (i > 0)
                QVERIFY(all.at(i - 1).name.compare(all.at(i).name, Qt::CaseInsensitive) <= 0);
            text += all.at(i).category == QLatin1String("TextAnimation") ? 1 : 0;
        }
        QCOMPARE(seen.size(), 43);
        QVERIFY(text >= 4);
        QVERIFY(transitionkinds::find(wipe) != nullptr);
        QCOMPARE(transitionkinds::find(wipe)->variables.at(0).allowed, 495);
        QVERIFY(transitionkinds::find(push) != nullptr);
        QVERIFY(transitionkinds::find("AB29D07B-E9E2-4E0A-93BD-AD3EA58120FA") != nullptr);
    }
};

QTEST_GUILESS_MAIN(BuildsTest)
#include "tst_builds.moc"
