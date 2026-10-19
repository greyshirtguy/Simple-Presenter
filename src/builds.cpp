#include "builds.h"

#include "transitionkinds.h"

#include <QColor>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <set>

namespace builds {

namespace {

using Build = rv::data::Slide::Element::Build;
using Variable = rv::data::Effect::EffectVariable;
using Direction = rv::data::Effect::EffectVariable::EffectDirection;

// A new id, written as ProPresenter writes them
std::string newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

QColor toColor(const rv::data::Color &color)
{
    return QColor::fromRgbF(color.red(), color.green(), color.blue(), color.alpha());
}

void setColor(rv::data::Color *to, const QColor &color)
{
    to->set_red(float(color.redF()));
    to->set_green(float(color.greenF()));
    to->set_blue(float(color.blueF()));
    to->set_alpha(float(color.alphaF()));
}

// A direction as the file has it (a bit: 1 the top left, 2 the top, 256 the bottom
// right) as a number from 0 to 8, and back; -1 for none
int directionIndex(int bit)
{
    for (int i = 0; i < 9; ++i) {
        if (bit == (1 << i))
            return i;
    }
    return -1;
}

Direction::EffectDirection directionBit(int index)
{
    return index >= 0 && index < 9 ? Direction::EffectDirection(1 << index) : Direction::EFFECT_DIRECTION_NONE;
}

QString startName(Build::Start start)
{
    switch (start) {
    case Build::START_WITH_PREVIOUS:
        return QStringLiteral("withPrevious");
    case Build::START_AFTER_PREVIOUS:
        return QStringLiteral("afterPrevious");
    case Build::START_WITH_SLIDE:
        return QStringLiteral("withSlide");
    default:
        return QStringLiteral("click");
    }
}

bool startOf(const QString &name, Build::Start *start)
{
    if (name == QLatin1String("click"))
        *start = Build::START_ON_CLICK;
    else if (name == QLatin1String("withPrevious"))
        *start = Build::START_WITH_PREVIOUS;
    else if (name == QLatin1String("afterPrevious"))
        *start = Build::START_AFTER_PREVIOUS;
    else if (name == QLatin1String("withSlide"))
        *start = Build::START_WITH_SLIDE;
    else
        return false;
    return true;
}

// A build of the slide, where it is: the element it belongs to, and whether it is that
// element's Build Out
struct Found
{
    int element = -1;
    bool out = false;
};

// Every build of the slide in the order they run: the order's first, then those it does
// not name, element by element, in before out (a GUESS at ProPresenter, see the header).
QList<Found> ordered(const rv::data::Slide &slide)
{
    QList<Found> all;
    for (int i = 0; i < slide.elements_size(); ++i) {
        if (slide.elements(i).has_build_in())
            all.append({i, false});
        if (slide.elements(i).has_build_out())
            all.append({i, true});
    }
    const auto idOf = [&slide](const Found &found) {
        const rv::data::Slide::Element &element = slide.elements(found.element);
        return (found.out ? element.build_out() : element.build_in()).uuid().string();
    };
    QList<Found> result;
    for (const rv::data::UUID &named : slide.element_build_order()) {
        for (int i = 0; i < all.size(); ++i) {
            if (idOf(all.at(i)) == named.string()) {
                result.append(all.takeAt(i));
                break;
            }
        }
    }
    result.append(all);
    return result;
}

const Build &buildOf(const rv::data::Slide &slide, const Found &found)
{
    const rv::data::Slide::Element &element = slide.elements(found.element);
    return found.out ? element.build_out() : element.build_in();
}

// The build with that id, and where it is; element -1 if there is none
Found findBuild(const rv::data::Slide &slide, const QString &buildId)
{
    const std::string id = buildId.toStdString();
    for (int i = 0; i < slide.elements_size(); ++i) {
        if (slide.elements(i).has_build_in() && slide.elements(i).build_in().uuid().string() == id)
            return {i, false};
        if (slide.elements(i).has_build_out() && slide.elements(i).build_out().uuid().string() == id)
            return {i, true};
    }
    return {};
}

// How long a build's transition takes: nothing for a Cut or for none
double lengthOf(const Build &build)
{
    return build.has_transition() ? std::max(0.0, build.transition().duration()) : 0;
}

QVariantMap describeBuild(const rv::data::Slide &slide, const Found &found)
{
    const Build &build = buildOf(slide, found);
    return {
        {"id", QString::fromStdString(build.uuid().string())},
        {"element", QString::fromStdString(slide.elements(found.element).element().uuid().string())},
        {"out", found.out},
        {"start", startName(build.start())},
        {"delay", build.delaytime()},
        {"transition", describeTransition(build.transition())},
    };
}

// Sets a variable of an effect from the value QML has for it; false for a value of the
// wrong kind.
bool setVariable(Variable *variable, const QVariant &value)
{
    bool number = false;
    const double wanted = value.toDouble(&number);
    switch (variable->Type_case()) {
    case Variable::kFloat:
        if (!number)
            return false;
        variable->mutable_float_()->set_value(float(wanted));
        return true;
    case Variable::kDouble:
        if (!number)
            return false;
        variable->mutable_double_()->set_value(wanted);
        return true;
    case Variable::kInt:
        if (!number)
            return false;
        variable->mutable_int_()->set_value(int(std::lround(wanted)));
        return true;
    case Variable::kColor: {
        // (As a colour, or as QML writes one: "#aarrggbb".)
        const QColor color = value.typeId() == QMetaType::QString ? QColor::fromString(value.toString()) : value.value<QColor>();
        if (!color.isValid())
            return false;
        setColor(variable->mutable_color()->mutable_color(), color);
        return true;
    }
    case Variable::kDirection:
        if (!number || directionIndex(1 << int(std::lround(wanted))) < 0)
            return false;
        variable->mutable_direction()->set_direction(directionBit(int(std::lround(wanted))));
        return true;
    default:
        return true;
    }
}

// An effect for a transition as ProPresenter makes one, its variables at their first
// values
rv::data::Effect madeEffect(const transitionkinds::Transition &kind)
{
    rv::data::Effect effect;
    effect.mutable_uuid()->set_string(newId());
    effect.set_name(kind.name.toStdString());
    effect.set_render_id(kind.renderId.toStdString());
    effect.set_behavior_description(kind.description.toStdString());
    effect.set_category(kind.category.toStdString());
    for (const transitionkinds::Variable &wanted : kind.variables) {
        Variable *variable = effect.add_variables();
        variable->set_name(wanted.name.toStdString());
        variable->set_description(wanted.description.toStdString());
        switch (wanted.kind) {
        case transitionkinds::Variable::Float:
            variable->mutable_float_()->set_value(float(wanted.initial));
            variable->mutable_float_()->set_default_value(float(wanted.initial));
            variable->mutable_float_()->set_min(float(wanted.least));
            variable->mutable_float_()->set_max(float(wanted.most));
            break;
        case transitionkinds::Variable::Color:
            for (rv::data::Color *color : {variable->mutable_color()->mutable_color(), variable->mutable_color()->mutable_default_color()}) {
                color->set_red(wanted.initialColor[0]);
                color->set_green(wanted.initialColor[1]);
                color->set_blue(wanted.initialColor[2]);
                color->set_alpha(wanted.initialColor[3]);
            }
            break;
        case transitionkinds::Variable::Direction:
            variable->mutable_direction()->set_direction(directionBit(int(wanted.initial)));
            variable->mutable_direction()->set_default_direction(directionBit(int(wanted.initial)));
            variable->mutable_direction()->set_available_directions(uint32_t(wanted.allowed));
            break;
        case transitionkinds::Variable::Grouping:
        case transitionkinds::Variable::Motion:
            // (A GUESS at how ProPresenter writes these, which no file here has: a number.)
            variable->mutable_int_()->set_value(int(wanted.initial));
            variable->mutable_int_()->set_default_value(int(wanted.initial));
            break;
        }
    }
    return effect;
}

} // namespace

QVariantMap describeTransition(const rv::data::Transition &transition)
{
    if (!transition.has_effect())
        return {};
    const rv::data::Effect &effect = transition.effect();
    QVariantMap values;
    for (const Variable &variable : effect.variables()) {
        const QString name = QString::fromStdString(variable.name());
        switch (variable.Type_case()) {
        case Variable::kFloat:
            values.insert(name, double(variable.float_().value()));
            break;
        case Variable::kDouble:
            values.insert(name, variable.double_().value());
            break;
        case Variable::kInt:
            values.insert(name, variable.int_().value());
            break;
        case Variable::kColor:
            values.insert(name, toColor(variable.color().color()));
            break;
        case Variable::kDirection:
            values.insert(name, directionIndex(int(variable.direction().direction())));
            break;
        default:
            break;
        }
    }
    return {
        {"renderId", QString::fromStdString(effect.render_id())},
        {"name", QString::fromStdString(effect.name())},
        {"category", QString::fromStdString(effect.category())},
        {"duration", transition.duration()},
        {"values", values},
    };
}

QString writeTransition(rv::data::Transition *into, const QVariantMap &transition)
{
    const QString renderId = transition.value("renderId").toString();
    const bool same = !transition.value("reset").toBool() && into->has_effect()
                   && QString::fromStdString(into->effect().render_id()).compare(renderId, Qt::CaseInsensitive) == 0;
    const transitionkinds::Transition *kind = transitionkinds::find(renderId);
    if (!same && !kind)
        return QStringLiteral("That is not a transition ProPresenter has");
    // (A transition made afresh with no length said is as long as ProPresenter makes it.)
    bool number = true;
    const double duration = transition.contains("duration") ? transition.value("duration").toDouble(&number)
                          : same ? into->duration() : kind->duration;
    if (!number || !std::isfinite(duration) || duration < 0)
        return QStringLiteral("A transition's length is a number of seconds, not less than none");

    rv::data::Effect effect = same ? into->effect() : madeEffect(*kind);
    const QVariantMap values = transition.value("values").toMap();
    for (Variable &variable : *effect.mutable_variables()) {
        const QString name = QString::fromStdString(variable.name());
        if (values.contains(name) && !setVariable(&variable, values.value(name)))
            return QStringLiteral("That is not a value %1 can have").arg(name);
    }
    if (duration != into->duration())
        into->set_duration(duration);
    *into->mutable_effect() = std::move(effect);
    return {};
}

QVariantList describe(const rv::data::Slide &slide)
{
    QVariantList list;
    for (const Found &found : ordered(slide))
        list.append(describeBuild(slide, found));
    return list;
}

Plan plan(const rv::data::Slide &slide)
{
    Plan result;
    // The step the build before was in, which With Previous and After Previous join,
    // and how many On Click steps there have been (a With Slide build after one goes
    // back to the slide's own step, and the next On Click is a step of its own still)
    int step = 0;
    int clicks = 0;
    // What a build With Previous after it is timed from (see the header): the moment
    // the build before was set going, which for one With Slide, On Click or After
    // Previous is before its delay, and for one With Previous is when it began
    double previousSetOff = 0;
    double previousEnd = 0;
    for (const Found &found : ordered(slide)) {
        const Build &build = buildOf(slide, found);
        const double delay = std::max(0.0, build.delaytime());
        Planned planned;
        planned.id = QString::fromStdString(build.uuid().string());
        planned.element = QString::fromStdString(slide.elements(found.element).element().uuid().string());
        planned.out = found.out;
        planned.duration = lengthOf(build);
        double setOff = 0;
        switch (build.start()) {
        case Build::START_WITH_SLIDE:
            planned.step = 0;
            setOff = 0;
            planned.at = delay;
            break;
        case Build::START_WITH_PREVIOUS:
            planned.step = step;
            planned.at = previousSetOff + delay;
            setOff = planned.at;
            break;
        case Build::START_AFTER_PREVIOUS:
            planned.step = step;
            setOff = previousEnd;
            planned.at = setOff + delay;
            break;
        default:
            planned.step = ++clicks;
            setOff = 0;
            planned.at = delay;
            break;
        }
        step = planned.step;
        previousSetOff = setOff;
        previousEnd = planned.at + planned.duration;
        result.builds.append(planned);
    }
    result.steps = clicks;
    return result;
}

QVariantMap describePlan(const rv::data::Slide &slide, int *steps)
{
    const Plan planned = plan(slide);
    if (steps)
        *steps = planned.steps;
    QVariantMap byElement;
    for (const Planned &build : planned.builds) {
        QVariantMap entry = byElement.value(build.element).toMap();
        const QString half = build.out ? QStringLiteral("out") : QStringLiteral("in");
        const Found found = findBuild(slide, build.id);
        entry.insert(half + "Step", build.step);
        entry.insert(half + "At", build.at);
        entry.insert(half + "Duration", build.duration);
        entry.insert(half + "Transition", describeTransition(buildOf(slide, found).transition()));
        byElement.insert(build.element, entry);
    }
    return byElement;
}

QString add(rv::data::Slide *slide, const QString &element, bool out, const QVariantMap &transition, QString *made)
{
    const std::string wanted = element.toStdString();
    rv::data::Slide::Element *target = nullptr;
    for (rv::data::Slide::Element &candidate : *slide->mutable_elements()) {
        if (candidate.element().uuid().string() == wanted) {
            target = &candidate;
            break;
        }
    }
    if (!target)
        return QStringLiteral("The slide no longer has that element");
    if (out ? target->has_build_out() : target->has_build_in())
        return QStringLiteral("That element has a build of that kind already");
    Build build;
    build.mutable_uuid()->set_string(newId());
    build.mutable_elementuuid()->set_string(wanted);
    build.set_start(Build::START_ON_CLICK);
    const QString error = writeTransition(build.mutable_transition(), transition);
    if (!error.isEmpty())
        return error;
    if (made)
        *made = QString::fromStdString(build.uuid().string());
    slide->add_element_build_order()->set_string(build.uuid().string());
    *(out ? target->mutable_build_out() : target->mutable_build_in()) = std::move(build);
    return {};
}

QString remove(rv::data::Slide *slide, const QString &buildId)
{
    const Found found = findBuild(*slide, buildId);
    if (found.element < 0)
        return QStringLiteral("The slide no longer has that build");
    rv::data::Slide::Element *element = slide->mutable_elements(found.element);
    if (found.out)
        element->clear_build_out();
    else
        element->clear_build_in();
    auto *order = slide->mutable_element_build_order();
    for (int i = order->size() - 1; i >= 0; --i) {
        if (order->Get(i).string() == buildId.toStdString())
            order->DeleteSubrange(i, 1);
    }
    return {};
}

QString change(rv::data::Slide *slide, const QString &buildId, const QVariantMap &changes)
{
    const Found found = findBuild(*slide, buildId);
    if (found.element < 0)
        return QStringLiteral("The slide no longer has that build");
    Build build = buildOf(*slide, found);
    if (changes.contains("start")) {
        Build::Start start;
        if (!startOf(changes.value("start").toString(), &start))
            return QStringLiteral("That is not a way for a build to start");
        build.set_start(start);
    }
    if (changes.contains("delay")) {
        bool number = false;
        const double delay = changes.value("delay").toDouble(&number);
        if (!number || !std::isfinite(delay) || delay < 0)
            return QStringLiteral("A delay is a number of seconds, not less than none");
        build.set_delaytime(delay);
    }
    if (changes.contains("transition")) {
        const QString error = writeTransition(build.mutable_transition(), changes.value("transition").toMap());
        if (!error.isEmpty())
            return error;
    }
    rv::data::Slide::Element *element = slide->mutable_elements(found.element);
    *(found.out ? element->mutable_build_out() : element->mutable_build_in()) = std::move(build);
    return {};
}

QString move(rv::data::Slide *slide, const QString &buildId, int index)
{
    if (findBuild(*slide, buildId).element < 0)
        return QStringLiteral("The slide no longer has that build");
    QList<std::string> order;
    for (const Found &found : ordered(*slide))
        order.append(buildOf(*slide, found).uuid().string());
    const int from = int(order.indexOf(buildId.toStdString()));
    const int to = std::clamp(index, 0, int(order.size()) - 1);
    order.move(from, to);
    // (The ids the order named that are no builds of the slide's go: there is nothing
    // left for them to put in order.)
    slide->clear_element_build_order();
    for (const std::string &id : order)
        slide->add_element_build_order()->set_string(id);
    return {};
}

void takeThemeBuilds(rv::data::Slide *slide, const rv::data::Slide &theme, const QHash<QString, QString> &tookLook)
{
    const auto themed = [&theme](const QString &id) -> const rv::data::Slide::Element * {
        for (const rv::data::Slide::Element &element : theme.elements()) {
            if (QString::fromStdString(element.element().uuid().string()) == id)
                return &element;
        }
        return nullptr;
    };
    for (rv::data::Slide::Element &element : *slide->mutable_elements()) {
        const QString id = QString::fromStdString(element.element().uuid().string());
        const auto took = tookLook.constFind(id);
        if (took == tookLook.constEnd())
            continue;
        const rv::data::Slide::Element *from = themed(took.value());
        element.clear_build_in();
        element.clear_build_out();
        if (!from)
            continue;
        if (from->has_build_in()) {
            *element.mutable_build_in() = from->build_in();
            element.mutable_build_in()->mutable_elementuuid()->set_string(id.toStdString());
        }
        if (from->has_build_out()) {
            *element.mutable_build_out() = from->build_out();
            element.mutable_build_out()->mutable_elementuuid()->set_string(id.toStdString());
        }
    }
    // The order: the theme's, then the slide's, of the builds the slide has now
    std::set<std::string> there;
    for (const rv::data::Slide::Element &element : slide->elements()) {
        if (element.has_build_in())
            there.insert(element.build_in().uuid().string());
        if (element.has_build_out())
            there.insert(element.build_out().uuid().string());
    }
    QList<std::string> order;
    for (const auto *list : {&theme.element_build_order(), &slide->element_build_order()}) {
        for (const rv::data::UUID &id : *list) {
            if (there.count(id.string()) > 0 && !order.contains(id.string()))
                order.append(id.string());
        }
    }
    slide->clear_element_build_order();
    for (const std::string &id : order)
        slide->add_element_build_order()->set_string(id);
}

QString setSlideTransition(rv::data::PresentationSlide *slide, const QVariantMap &transition)
{
    if (transition.isEmpty()) {
        slide->clear_transition();
        return {};
    }
    rv::data::Transition made = slide->transition();
    const QString error = writeTransition(&made, transition);
    if (error.isEmpty())
        *slide->mutable_transition() = std::move(made);
    return error;
}

} // namespace builds
