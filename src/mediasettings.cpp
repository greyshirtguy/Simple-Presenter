#include "mediasettings.h"

#include "mediaeffects.h"

#include <QColor>
#include <QUuid>

#include <cmath>

namespace mediasettings {

namespace {

using Drawing = rv::data::Media::DrawingProperties;
using Transport = rv::data::Media::TransportProperties;

// The drawing settings of media that is drawn (a picture, a video, a live video), or
// null for what is not (sound).
const Drawing *drawingOf(const rv::data::Media &media)
{
    if (media.has_video())
        return &media.video().drawing();
    if (media.has_image())
        return &media.image().drawing();
    if (media.has_live_video())
        return &media.live_video().drawing();
    return nullptr;
}

Drawing *mutableDrawingOf(rv::data::Media *media)
{
    if (media->has_video())
        return media->mutable_video()->mutable_drawing();
    if (media->has_image())
        return media->mutable_image()->mutable_drawing();
    if (media->has_live_video())
        return media->mutable_live_video()->mutable_drawing();
    return nullptr;
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

// A new id, written as ProPresenter writes them (as workspace::newUuid does)
QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
}

QVariantMap describeVariable(const rv::data::Effect::EffectVariable &variable)
{
    QVariantMap map {{"name", QString::fromStdString(variable.name())}};
    switch (variable.Type_case()) {
    case rv::data::Effect::EffectVariable::kInt:
        map.insert("kind", QStringLiteral("int"));
        map.insert("value", variable.int_().value());
        map.insert("least", variable.int_().min());
        map.insert("most", variable.int_().max());
        map.insert("initial", variable.int_().default_value());
        break;
    case rv::data::Effect::EffectVariable::kFloat:
        map.insert("kind", QStringLiteral("float"));
        map.insert("value", double(variable.float_().value()));
        map.insert("least", double(variable.float_().min()));
        map.insert("most", double(variable.float_().max()));
        map.insert("initial", double(variable.float_().default_value()));
        break;
    case rv::data::Effect::EffectVariable::kDouble:
        map.insert("kind", QStringLiteral("double"));
        map.insert("value", variable.double_().value());
        map.insert("least", variable.double_().min());
        map.insert("most", variable.double_().max());
        map.insert("initial", variable.double_().default_value());
        break;
    case rv::data::Effect::EffectVariable::kColor:
        map.insert("kind", QStringLiteral("color"));
        map.insert("value", toColor(variable.color().color()));
        map.insert("initial", toColor(variable.color().default_color()));
        break;
    default:
        // (A direction, which none of ProPresenter's effects for media has)
        map.insert("kind", QStringLiteral("other"));
        break;
    }
    return map;
}

QVariantMap describeEffect(const rv::data::Effect &effect)
{
    const QString renderId = QString::fromStdString(effect.render_id());
    const mediaeffects::Effect *known = mediaeffects::find(renderId);
    QVariantList variables;
    for (const rv::data::Effect::EffectVariable &variable : effect.variables()) {
        QVariantMap described = describeVariable(variable);
        // Which of the shader's numbers, or of its colours, this one is: by its place
        // among the catalogue's of its kind, found by name
        int slot = -1;
        if (known) {
            int numbers = 0;
            int colors = 0;
            for (const mediaeffects::Variable &wanted : known->variables) {
                const bool colour = wanted.kind == mediaeffects::Variable::Color;
                if (wanted.name == described.value("name").toString()) {
                    slot = colour ? colors : numbers;
                    break;
                }
                (colour ? colors : numbers) += 1;
            }
        }
        described.insert("slot", slot);
        variables.append(described);
    }

    // What the shader is given: the catalogue's numbers and colours in its order, each
    // from the variable of that name if the file has one.
    QVariantList numbers;
    QVariantList colors;
    if (known) {
        for (const mediaeffects::Variable &wanted : known->variables) {
            const auto found = std::find_if(variables.cbegin(), variables.cend(), [&wanted](const QVariant &variable) {
                return variable.toMap().value("name").toString() == wanted.name;
            });
            const QVariantMap had = found == variables.cend() ? QVariantMap() : found->toMap();
            if (wanted.kind == mediaeffects::Variable::Color)
                colors.append(had.value("kind").toString() == QLatin1String("color") ? had.value("value") : QVariant(wanted.initialColor));
            else
                numbers.append(had.contains("value") && had.value("kind").toString() != QLatin1String("color")
                                   ? had.value("value").toDouble() : wanted.initial);
        }
    }
    const QString name = QString::fromStdString(effect.name());
    return {
        {"id", QString::fromStdString(effect.uuid().string())},
        {"on", effect.enabled()},
        {"name", name.isEmpty() && known ? known->name : name},
        {"renderId", renderId},
        {"known", known != nullptr},
        {"shader", known ? known->shader : QString()},
        {"animated", known && known->animated},
        {"variables", variables},
        {"numbers", numbers},
        {"colors", colors},
    };
}

rv::data::Effect makeEffect(const mediaeffects::Effect &known, const QString &id)
{
    rv::data::Effect made;
    made.mutable_uuid()->set_string(id.toStdString());
    made.set_enabled(true);
    made.set_name(known.name.toStdString());
    made.set_render_id(known.renderId.toStdString());
    made.set_behavior_description(known.description.toStdString());
    for (const mediaeffects::Variable &wanted : known.variables) {
        rv::data::Effect::EffectVariable *variable = made.add_variables();
        variable->set_name(wanted.name.toStdString());
        variable->set_description(wanted.description.toStdString());
        switch (wanted.kind) {
        case mediaeffects::Variable::Int: {
            rv::data::Effect::EffectVariable::EffectInt *number = variable->mutable_int_();
            number->set_value(int(std::lround(wanted.initial)));
            number->set_default_value(int(std::lround(wanted.initial)));
            number->set_min(int(std::lround(wanted.least)));
            number->set_max(int(std::lround(wanted.most)));
            break;
        }
        case mediaeffects::Variable::Float: {
            rv::data::Effect::EffectVariable::EffectFloat *number = variable->mutable_float_();
            number->set_value(float(wanted.initial));
            number->set_default_value(float(wanted.initial));
            number->set_min(float(wanted.least));
            number->set_max(float(wanted.most));
            break;
        }
        case mediaeffects::Variable::Color:
            setColor(variable->mutable_color()->mutable_color(), wanted.initialColor);
            setColor(variable->mutable_color()->mutable_default_color(), wanted.initialColor);
            break;
        }
    }
    return made;
}

// Sets a variable's value from the inspector's, keeping it within its least and most
// as the file has them. False for a value of the wrong kind.
bool setVariable(rv::data::Effect::EffectVariable *variable, const QVariant &value)
{
    bool number = false;
    const double wanted = value.toDouble(&number);
    switch (variable->Type_case()) {
    case rv::data::Effect::EffectVariable::kInt: {
        if (!number)
            return false;
        rv::data::Effect::EffectVariable::EffectInt *to = variable->mutable_int_();
        int rounded = int(std::lround(wanted));
        if (to->max() > to->min())
            rounded = qBound(to->min(), rounded, to->max());
        to->set_value(rounded);
        return true;
    }
    case rv::data::Effect::EffectVariable::kFloat: {
        if (!number)
            return false;
        rv::data::Effect::EffectVariable::EffectFloat *to = variable->mutable_float_();
        float set = float(wanted);
        if (to->max() > to->min())
            set = qBound(to->min(), set, to->max());
        to->set_value(set);
        return true;
    }
    case rv::data::Effect::EffectVariable::kDouble: {
        if (!number)
            return false;
        rv::data::Effect::EffectVariable::EffectDouble *to = variable->mutable_double_();
        double set = wanted;
        if (to->max() > to->min())
            set = qBound(to->min(), set, to->max());
        to->set_value(set);
        return true;
    }
    case rv::data::Effect::EffectVariable::kColor: {
        const QColor color = value.value<QColor>();
        if (!color.isValid())
            return false;
        setColor(variable->mutable_color()->mutable_color(), color);
        return true;
    }
    default:
        // Nothing here sets the others; they are kept as they are.
        return true;
    }
}

// The effects as the list asks for them, made from what the media has and the
// catalogue; or an error.
QString effectsFor(const google::protobuf::RepeatedPtrField<rv::data::Effect> &had, const QVariantList &wanted,
                   std::vector<rv::data::Effect> *result)
{
    for (const QVariant &entry : wanted) {
        const QVariantMap map = entry.toMap();
        const QString id = map.value("id").toString();
        if (id.isEmpty())
            return QStringLiteral("An effect has no id");
        const auto found = std::find_if(had.cbegin(), had.cend(), [&id](const rv::data::Effect &effect) {
            return QString::fromStdString(effect.uuid().string()) == id;
        });
        rv::data::Effect effect;
        if (found != had.cend()) {
            effect = *found;
        } else {
            const mediaeffects::Effect *known = mediaeffects::find(map.value("renderId").toString());
            if (!known)
                return QStringLiteral("That is not an effect this app knows");
            effect = makeEffect(*known, id);
        }
        if (map.contains("on"))
            effect.set_enabled(map.value("on").toBool());
        for (const QVariant &asked : map.value("variables").toList()) {
            const QVariantMap variable = asked.toMap();
            if (!variable.contains("value"))
                continue;
            for (rv::data::Effect::EffectVariable &mine : *effect.mutable_variables()) {
                if (QString::fromStdString(mine.name()) == variable.value("name").toString()) {
                    if (!setVariable(&mine, variable.value("value")))
                        return QStringLiteral("That is not a value %1 can have").arg(QString::fromStdString(mine.name()));
                    break;
                }
            }
        }
        result->push_back(effect);
    }
    return {};
}

bool isTime(const QVariant &value)
{
    bool number = false;
    const double seconds = value.toDouble(&number);
    return number && std::isfinite(seconds) && seconds >= 0;
}

} // namespace

QVariantMap describe(const rv::data::Media &media)
{
    static const Drawing none;
    const Drawing &drawing = drawingOf(media) ? *drawingOf(media) : none;
    const int scale = drawing.scale_behavior() == rv::data::Media::SCALE_BEHAVIOR_FILL      ? 1
                    : drawing.scale_behavior() == rv::data::Media::SCALE_BEHAVIOR_STRETCH ? 2 : 0;
    const int rotation = int(drawing.native_rotation());
    const int turned = rotation == 90 || rotation == 180 || rotation == 270 ? rotation : 0;
    const rv::data::Graphics::EdgeInsets &crop = drawing.crop_insets();

    double inPoint = 0;
    double outPoint = 0;
    double endPoint = 0;
    double thumbnail = -1;
    int playback = 0;
    if (media.has_video()) {
        const Transport &transport = media.video().transport();
        inPoint = transport.in_point();
        outPoint = transport.out_point();
        endPoint = transport.end_point();
        thumbnail = media.video().video().thumbnail_position();
        playback = qBound(0, int(transport.playback_behavior()), 3);
    }
    const bool trimmedOut = outPoint > 0 && outPoint > inPoint && (endPoint <= 0 || outPoint < endPoint - 0.0005);

    QVariantList effects;
    bool effectDrawn = false;
    for (const rv::data::Effect &effect : drawing.effects()) {
        const QVariantMap described = describeEffect(effect);
        effectDrawn = effectDrawn || (described.value("on").toBool() && !described.value("shader").toString().isEmpty());
        effects.append(described);
    }
    const bool cropped = drawing.crop_enable() && (crop.left() > 0 || crop.right() > 0 || crop.top() > 0 || crop.bottom() > 0);

    return {
        {"scale", scale},
        {"blurred", drawing.is_blurred()},
        {"rotation", turned},
        {"flipH", drawing.flipped_horizontally()},
        {"flipV", drawing.flipped_vertically()},
        {"cropOn", drawing.crop_enable()},
        {"cropLeft", crop.left()},
        {"cropRight", crop.right()},
        {"cropTop", crop.top()},
        {"cropBottom", crop.bottom()},
        {"naturalWidth", drawing.natural_size().width()},
        {"naturalHeight", drawing.natural_size().height()},
        {"inPoint", inPoint},
        {"outPoint", outPoint},
        {"endPoint", endPoint},
        {"trimmedOut", trimmedOut},
        {"thumbnail", thumbnail},
        {"playback", playback},
        {"effects", effects},
        {"drawn", cropped || turned != 0 || drawing.flipped_horizontally() || drawing.flipped_vertically()
                      || drawing.is_blurred() || effectDrawn},
    };
}

QString apply(rv::data::Media *media, const QVariantMap &changes)
{
    Drawing *drawing = mutableDrawingOf(media);
    if (!drawing)
        return QStringLiteral("Only a picture or a video has these settings");
    const bool video = media->has_video();

    // Everything is looked at before anything is changed.
    if (changes.contains("scale") && (changes.value("scale").toInt() < 0 || changes.value("scale").toInt() > 2))
        return QStringLiteral("That is not a way of fitting media");
    if (changes.contains("rotation")) {
        const int rotation = changes.value("rotation").toInt();
        if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270)
            return QStringLiteral("Media turns by a quarter at a time");
    }
    for (const char *key : {"cropLeft", "cropRight", "cropTop", "cropBottom"}) {
        if (changes.contains(key) && !isTime(changes.value(key)))
            return QStringLiteral("A crop is a number of pixels, not less than none");
    }
    for (const char *key : {"inPoint", "outPoint", "endPoint", "playback"}) {
        if (changes.contains(key) && !video)
            return QStringLiteral("Only a video has a part that plays");
    }
    for (const char *key : {"inPoint", "outPoint", "endPoint"}) {
        if (changes.contains(key) && !isTime(changes.value(key)))
            return QStringLiteral("A time is a number of seconds, not less than none");
    }
    if (changes.contains("thumbnail") && !video)
        return QStringLiteral("Only a video has a frame to choose for its picture");
    if (changes.contains("playback") && (changes.value("playback").toInt() < 0 || changes.value("playback").toInt() > 3))
        return QStringLiteral("That is not a way of playing");
    std::vector<rv::data::Effect> effects;
    if (changes.contains("effects")) {
        const QString error = effectsFor(drawing->effects(), changes.value("effects").toList(), &effects);
        if (!error.isEmpty())
            return error;
    }

    if (changes.contains("scale"))
        drawing->set_scale_behavior(rv::data::Media::ScaleBehavior(changes.value("scale").toInt()));
    if (changes.contains("blurred"))
        drawing->set_is_blurred(changes.value("blurred").toBool());
    if (changes.contains("rotation"))
        drawing->set_native_rotation(Drawing::NativeRotationType(changes.value("rotation").toInt()));
    if (changes.contains("flipH"))
        drawing->set_flipped_horizontally(changes.value("flipH").toBool());
    if (changes.contains("flipV"))
        drawing->set_flipped_vertically(changes.value("flipV").toBool());
    if (changes.contains("cropOn"))
        drawing->set_crop_enable(changes.value("cropOn").toBool());
    if (changes.contains("cropLeft"))
        drawing->mutable_crop_insets()->set_left(changes.value("cropLeft").toDouble());
    if (changes.contains("cropRight"))
        drawing->mutable_crop_insets()->set_right(changes.value("cropRight").toDouble());
    if (changes.contains("cropTop"))
        drawing->mutable_crop_insets()->set_top(changes.value("cropTop").toDouble());
    if (changes.contains("cropBottom"))
        drawing->mutable_crop_insets()->set_bottom(changes.value("cropBottom").toDouble());
    if (video) {
        Transport *transport = media->mutable_video()->mutable_transport();
        if (changes.contains("inPoint"))
            transport->set_in_point(changes.value("inPoint").toDouble());
        if (changes.contains("outPoint"))
            transport->set_out_point(changes.value("outPoint").toDouble());
        if (changes.contains("endPoint"))
            transport->set_end_point(changes.value("endPoint").toDouble());
        if (changes.contains("playback"))
            transport->set_playback_behavior(Transport::PlaybackBehavior(changes.value("playback").toInt()));
        if (changes.contains("thumbnail"))
            media->mutable_video()->mutable_video()->set_thumbnail_position(changes.value("thumbnail").toDouble());
    }
    if (changes.contains("effects")) {
        drawing->clear_effects();
        for (rv::data::Effect &effect : effects)
            *drawing->add_effects() = std::move(effect);
    }
    return {};
}

bool isSet(const rv::data::Media &media, bool countFitting)
{
    if (const Drawing *drawing = drawingOf(media)) {
        const auto scale = drawing->scale_behavior();
        if ((countFitting && (scale == rv::data::Media::SCALE_BEHAVIOR_FILL || scale == rv::data::Media::SCALE_BEHAVIOR_STRETCH))
            || drawing->is_blurred() || drawing->native_rotation() != Drawing::NATIVE_ROTATION_TYPE_ROTATE_STANDARD
            || drawing->flipped_horizontally() || drawing->flipped_vertically() || drawing->crop_enable() || drawing->effects_size() > 0)
            return true;
    }
    if (media.has_video()) {
        // (As describe() tells a trim: an out point before the end, after the in point)
        const Transport &transport = media.video().transport();
        const double out = transport.out_point();
        const bool trimmedOut = out > 0 && out > transport.in_point() && (transport.end_point() <= 0 || out < transport.end_point() - 0.0005);
        if (transport.in_point() > 0 || trimmedOut || media.video().video().thumbnail_position() > 0)
            return true;
    }
    return false;
}

QVariantMap newEffect(const QString &renderId)
{
    const mediaeffects::Effect *known = mediaeffects::find(renderId);
    return known ? describeEffect(makeEffect(*known, newId())) : QVariantMap();
}

void carryOver(const rv::data::Media &from, rv::data::Media *to)
{
    const Drawing *had = drawingOf(from);
    Drawing *drawing = mutableDrawingOf(to);
    if (had && drawing) {
        // (A size set by dragging the old media about was a size for that media.)
        drawing->set_scale_behavior(had->scale_behavior() == rv::data::Media::SCALE_BEHAVIOR_CUSTOM
                                        ? rv::data::Media::SCALE_BEHAVIOR_FIT : had->scale_behavior());
        drawing->set_is_blurred(had->is_blurred());
        drawing->set_native_rotation(had->native_rotation());
        drawing->set_flipped_horizontally(had->flipped_horizontally());
        drawing->set_flipped_vertically(had->flipped_vertically());
        *drawing->mutable_effects() = had->effects();
    }
    if (from.has_video() && to->has_video()) {
        const Transport &transport = from.video().transport();
        Transport *into = to->mutable_video()->mutable_transport();
        into->set_playback_behavior(transport.playback_behavior());
        into->set_times_to_loop(transport.times_to_loop());
        into->set_loop_time(transport.loop_time());
        into->set_retrigger(transport.retrigger());
        if (from.video().has_audio())
            *to->mutable_video()->mutable_audio() = from.video().audio();
    }
}

} // namespace mediasettings
