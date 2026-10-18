// What the Media Inspector sets of a piece of media (src/mediasettings.h) and the effects
// it can give it (src/mediaeffects.h), tried on media laid out as ProPresenter writes it,
// with fields this app knows nothing of, which must come back as they went.

#include "mediaeffects.h"
#include "mediasettings.h"

#include <google/protobuf/unknown_field_set.h>

#include <QColor>
#include <QTest>

namespace {

const QString vignette = QStringLiteral("42E83016-41DC-43AB-8061-9A93256DDA46");
const QString blur = QStringLiteral("D84CE4A3-D1C3-4F7C-AA75-4C645A3621F2");
const QString sepia = QStringLiteral("06157C8E-D971-4F06-9ECA-2663664E22A6");
const QString adjustColor = QStringLiteral("D4C199A9-64D7-4CCB-8C26-443E7A9C5C03");

// A variable of an effect, as ProPresenter writes one: its name, what it is for, and a
// number with its least and most (the default left out where it is 0, as protobuf does)
void addFloat(rv::data::Effect *effect, const char *name, const char *description, float value, float initial, float most)
{
    rv::data::Effect::EffectVariable *variable = effect->add_variables();
    variable->set_name(name);
    variable->set_description(description);
    variable->mutable_float_()->set_value(value);
    variable->mutable_float_()->set_default_value(initial);
    variable->mutable_float_()->set_max(most);
}

// A video as ProPresenter wrote one in the maintainer's workspace: stretched, cropped
// from the top and the bottom, looping, its picture taken at 3 s, a 30 s video not
// trimmed (the out point and the end point both at its length); with two effects that
// do not say they are on, and something in the media, in its drawing settings and in
// an effect that this app knows nothing of.
rv::data::Media prosVideo()
{
    rv::data::Media media;
    media.mutable_uuid()->set_string("E3FC911E-3F6B-494C-A7D1-8E9D2CDABB36");
    media.mutable_url()->set_absolute_string("file:///Users/dan/Documents/ProPresenter/Media/Assets/Liquid%20Metal.mov");
    media.mutable_metadata()->set_format("h264");
    rv::data::Media::VideoTypeProperties *video = media.mutable_video();
    rv::data::Media::DrawingProperties *drawing = video->mutable_drawing();
    drawing->set_scale_behavior(rv::data::Media::SCALE_BEHAVIOR_STRETCH);
    drawing->mutable_natural_size()->set_width(1920);
    drawing->mutable_natural_size()->set_height(1080);
    drawing->mutable_custom_image_bounds()->mutable_origin();
    drawing->set_crop_enable(true);
    drawing->mutable_crop_insets()->set_top(76.935483870968653);
    drawing->mutable_crop_insets()->set_bottom(209.30049088359252);
    drawing->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
    rv::data::Effect *first = drawing->add_effects();
    first->mutable_uuid()->set_string("F3CD5120-60F2-413E-A8BE-3A1E38F6DCC4");
    first->set_name("Vignette");
    first->set_render_id(vignette.toStdString());
    first->set_behavior_description("Reduction of an image's brightness or saturation at the periphery.");
    addFloat(first, "Radius", "Radius to darken", 50, 50, 100);
    first->GetReflection()->MutableUnknownFields(first)->AddVarint(900, 7);
    rv::data::Effect *second = drawing->add_effects();
    second->mutable_uuid()->set_string("2307BD6A-4A4F-4290-B9F9-38DB3DEF4877");
    second->set_name("Blur");
    second->set_render_id(blur.toStdString());
    second->set_behavior_description("Gaussian Blur of the image.");
    addFloat(second, "Intensity", "amount to blur by", 0.0393880233f, 0.5f, 1);
    drawing->GetReflection()->MutableUnknownFields(drawing)->AddVarint(901, 8);
    video->mutable_audio()->set_volume(1);
    rv::data::Media::TransportProperties *transport = video->mutable_transport();
    transport->set_play_rate(1);
    transport->set_out_point(30);
    transport->set_should_fade_in(true);
    transport->set_should_fade_out(true);
    transport->set_end_point(30);
    transport->set_playback_behavior(rv::data::Media::TransportProperties::PLAYBACK_BEHAVIOR_LOOP);
    transport->set_loop_time(1);
    transport->set_times_to_loop(1);
    video->mutable_video()->set_frame_rate(30);
    video->mutable_video()->set_thumbnail_position(3);
    video->mutable_video()->set_soft_loop_duration(0.5);
    media.GetReflection()->MutableUnknownFields(&media)->AddVarint(902, 9);
    return media;
}

// A picture as this app adds one (workspace::mediaElement): nothing set but its size.
rv::data::Media appsPicture()
{
    rv::data::Media media;
    media.mutable_uuid()->set_string("A");
    media.mutable_metadata()->set_format("PNG");
    media.mutable_image()->mutable_drawing()->mutable_natural_size()->set_width(800);
    media.mutable_image()->mutable_drawing()->mutable_natural_size()->set_height(600);
    media.mutable_image()->mutable_drawing()->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
    return media;
}

std::string bytesOf(const rv::data::Media &media)
{
    std::string bytes;
    media.SerializeToString(&bytes);
    return bytes;
}

QVariantMap effectNamed(const QVariantMap &settings, const QString &name)
{
    for (const QVariant &effect : settings.value("effects").toList()) {
        if (effect.toMap().value("name").toString() == name)
            return effect.toMap();
    }
    return {};
}

} // namespace

class MediaSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    // ProPresenter's video read as the inspector shows it
    void readsWhatProPresenterWrote()
    {
        const QVariantMap settings = mediasettings::describe(prosVideo());
        QCOMPARE(settings.value("scale").toInt(), 2);
        QCOMPARE(settings.value("blurred").toBool(), false);
        QCOMPARE(settings.value("rotation").toInt(), 0);
        QCOMPARE(settings.value("cropOn").toBool(), true);
        QCOMPARE(settings.value("cropTop").toDouble(), 76.935483870968653);
        QCOMPARE(settings.value("cropBottom").toDouble(), 209.30049088359252);
        QCOMPARE(settings.value("cropLeft").toDouble(), 0.0);
        QCOMPARE(settings.value("naturalWidth").toDouble(), 1920.0);
        // Out point and end point both at the length: not trimmed
        QCOMPARE(settings.value("outPoint").toDouble(), 30.0);
        QCOMPARE(settings.value("endPoint").toDouble(), 30.0);
        QCOMPARE(settings.value("trimmedOut").toBool(), false);
        QCOMPARE(settings.value("thumbnail").toDouble(), 3.0);
        QCOMPARE(settings.value("playback").toInt(), 1);
        // Cropped, so drawn otherwise than plain; its effects are off
        QCOMPARE(settings.value("drawn").toBool(), true);
        const QVariantList effects = settings.value("effects").toList();
        QCOMPARE(effects.size(), 2);
        const QVariantMap first = effects.at(0).toMap();
        QCOMPARE(first.value("name").toString(), QStringLiteral("Vignette"));
        QCOMPARE(first.value("on").toBool(), false);
        QCOMPARE(first.value("known").toBool(), true);
        QCOMPARE(first.value("shader").toString(), QStringLiteral("vignette"));
        QCOMPARE(first.value("numbers").toList(), QVariantList({50.0}));
        const QVariantMap variable = first.value("variables").toList().at(0).toMap();
        QCOMPARE(variable.value("name").toString(), QStringLiteral("Radius"));
        QCOMPARE(variable.value("kind").toString(), QStringLiteral("float"));
        QCOMPARE(variable.value("most").toDouble(), 100.0);
        QCOMPARE(effects.at(1).toMap().value("shader").toString(), QStringLiteral("blur"));
    }

    // Media this app made, with nothing set: drawn plain
    void plainMediaIsPlain()
    {
        const QVariantMap settings = mediasettings::describe(appsPicture());
        QCOMPARE(settings.value("scale").toInt(), 0);
        QCOMPARE(settings.value("drawn").toBool(), false);
        QCOMPARE(settings.value("trimmedOut").toBool(), false);
        QCOMPARE(settings.value("thumbnail").toDouble(), -1.0);
        QCOMPARE(settings.value("effects").toList().size(), 0);
        // An effect that is on, but not drawn here (it works on what is behind), does not
        // make it drawn otherwise either.
        rv::data::Media media = appsPicture();
        rv::data::Effect *behind = media.mutable_image()->mutable_drawing()->add_effects();
        behind->set_enabled(true);
        behind->set_render_id("F8907C4C-D768-41C1-8EBF-5B6AC07A4517");
        QCOMPARE(mediasettings::describe(media).value("drawn").toBool(), false);
    }

    // Whether media is set to anything: what decides whether a slide's map carries its
    // settings
    void setOrNot()
    {
        QVERIFY(!mediasettings::isSet(appsPicture()));
        QVERIFY(mediasettings::isSet(prosVideo()));
        rv::data::Media media = appsPicture();
        QCOMPARE(mediasettings::apply(&media, {{"flipV", true}}), QString());
        QVERIFY(mediasettings::isSet(media));
        QCOMPARE(mediasettings::apply(&media, {{"flipV", false}}), QString());
        QVERIFY(!mediasettings::isSet(media));
        // Filled: set, but not for a fill, which says how it is fitted by a key of its own
        QCOMPARE(mediasettings::apply(&media, {{"scale", 1}}), QString());
        QVERIFY(mediasettings::isSet(media));
        QVERIFY(!mediasettings::isSet(media, false));
        // An effect, even one that is off, is something set: the inspector shows it.
        QCOMPARE(mediasettings::apply(&media, {{"effects", QVariantList {mediasettings::newEffect(sepia)}}}), QString());
        QVERIFY(mediasettings::isSet(media, false));
        // A video with only its picture's moment chosen, or only trimmed, is set.
        rv::data::Media video = prosVideo();
        video.mutable_video()->mutable_drawing()->Clear();
        QVERIFY(mediasettings::isSet(video));
        video.mutable_video()->mutable_video()->set_thumbnail_position(-1);
        QVERIFY(!mediasettings::isSet(video));
        video.mutable_video()->mutable_transport()->set_out_point(12);
        QVERIFY(mediasettings::isSet(video));
    }

    // A change changes the fields it names and nothing else, byte for byte
    void changesOnlyWhatItNames()
    {
        rv::data::Media media = prosVideo();
        QCOMPARE(mediasettings::apply(&media, {{"flipH", true}, {"rotation", 90}, {"scale", 0}, {"blurred", true}}), QString());
        rv::data::Media expected = prosVideo();
        rv::data::Media::DrawingProperties *drawing = expected.mutable_video()->mutable_drawing();
        drawing->set_flipped_horizontally(true);
        drawing->set_native_rotation(rv::data::Media::DrawingProperties::NATIVE_ROTATION_TYPE_ROTATE_90);
        drawing->set_scale_behavior(rv::data::Media::SCALE_BEHAVIOR_FIT);
        drawing->set_is_blurred(true);
        QCOMPARE(bytesOf(media), bytesOf(expected));
        const QVariantMap settings = mediasettings::describe(media);
        QCOMPARE(settings.value("rotation").toInt(), 90);
        QCOMPARE(settings.value("blurred").toBool(), true);
    }

    // A value that cannot be changes nothing at all, not even the others asked for
    void refusesWhatCannotBe()
    {
        rv::data::Media media = prosVideo();
        const std::string before = bytesOf(media);
        QVERIFY(!mediasettings::apply(&media, {{"flipH", true}, {"rotation", 45}}).isEmpty());
        QVERIFY(!mediasettings::apply(&media, {{"flipV", true}, {"cropLeft", -1}}).isEmpty());
        QVERIFY(!mediasettings::apply(&media, {{"inPoint", -2}}).isEmpty());
        QVERIFY(!mediasettings::apply(&media, {{"effects", QVariantList {QVariantMap {{"id", "X"}, {"renderId", "NOT-AN-EFFECT"}}}}}).isEmpty());
        QCOMPARE(bytesOf(media), before);
        // A picture has no part that plays, nor a frame to take its picture from.
        rv::data::Media picture = appsPicture();
        QVERIFY(!mediasettings::apply(&picture, {{"inPoint", 1}}).isEmpty());
        QVERIFY(!mediasettings::apply(&picture, {{"thumbnail", 1}}).isEmpty());
        QCOMPARE(bytesOf(picture), bytesOf(appsPicture()));
        // Sound is not drawn.
        rv::data::Media sound;
        sound.mutable_audio();
        QVERIFY(!mediasettings::apply(&sound, {{"flipH", true}}).isEmpty());
    }

    // Trimming: an out point before the end is where it stops
    void trims()
    {
        rv::data::Media media = prosVideo();
        QCOMPARE(mediasettings::apply(&media, {{"inPoint", 2.5}, {"outPoint", 12}}), QString());
        QVariantMap settings = mediasettings::describe(media);
        QCOMPARE(settings.value("inPoint").toDouble(), 2.5);
        QCOMPARE(settings.value("trimmedOut").toBool(), true);
        // Back to the end: the out point at the length again
        QCOMPARE(mediasettings::apply(&media, {{"inPoint", 0}, {"outPoint", 30}}), QString());
        QCOMPARE(bytesOf(media), bytesOf(prosVideo()));
        // Media of this app's has no length in the file: an out point of its own is
        // before an end it does not know, and is a trim.
        rv::data::Media made;
        made.mutable_video()->mutable_transport()->set_play_rate(1);
        QCOMPARE(mediasettings::apply(&made, {{"outPoint", 5}}), QString());
        QCOMPARE(mediasettings::describe(made).value("trimmedOut").toBool(), true);
    }

    // Effects: kept by their ids with what the file has of them, added from the
    // catalogue, taken off; and added and taken off again, the media as it was
    void effectsAreKeptAddedAndTakenOff()
    {
        rv::data::Media media = prosVideo();
        const QVariantMap before = mediasettings::describe(media);
        QVariantMap kept = effectNamed(before, "Vignette");
        kept.insert("on", true);
        QVariantMap radius = kept.value("variables").toList().at(0).toMap();
        radius.insert("value", 70);
        kept.insert("variables", QVariantList {radius});
        const QVariantMap added = mediasettings::newEffect(sepia);
        QCOMPARE(added.value("name").toString(), QStringLiteral("Sepia"));
        QCOMPARE(added.value("on").toBool(), true);
        QCOMPARE(mediasettings::apply(&media, {{"effects", QVariantList {kept, added}}}), QString());

        const rv::data::Media::DrawingProperties &drawing = media.video().drawing();
        QCOMPARE(drawing.effects_size(), 2);
        const rv::data::Effect &first = drawing.effects(0);
        QCOMPARE(first.uuid().string(), std::string("F3CD5120-60F2-413E-A8BE-3A1E38F6DCC4"));
        QVERIFY(first.enabled());
        QCOMPARE(first.variables(0).float_().value(), 70.0f);
        QCOMPARE(first.variables(0).float_().max(), 100.0f);
        QCOMPARE(first.behavior_description(), std::string("Reduction of an image's brightness or saturation at the periphery."));
        QCOMPARE(first.GetReflection()->GetUnknownFields(first).field_count(), 1);
        const rv::data::Effect &second = drawing.effects(1);
        QCOMPARE(second.render_id(), sepia.toStdString());
        QCOMPARE(second.name(), std::string("Sepia"));
        QCOMPARE(second.behavior_description(), std::string("Simulates the Sepia tone."));
        QCOMPARE(QString::fromStdString(second.uuid().string()), added.value("id").toString());
        QVERIFY(second.enabled());
        QCOMPARE(mediasettings::describe(media).value("drawn").toBool(), true);

        // A value past the most it can be is the most.
        radius.insert("value", 500);
        kept.insert("variables", QVariantList {radius});
        QCOMPARE(mediasettings::apply(&media, {{"effects", QVariantList {kept, added}}}), QString());
        QCOMPARE(media.video().drawing().effects(0).variables(0).float_().value(), 100.0f);

        // A change of mind: the effects as they came, the media byte for byte as it was
        QCOMPARE(mediasettings::apply(&media, {{"effects", before.value("effects")}}), QString());
        QCOMPARE(bytesOf(media), bytesOf(prosVideo()));
    }

    // A new effect is as the catalogue has it, and what its shader is given follows the
    // catalogue's order, from the file's variables by name
    void newEffectsAndWhatTheShaderIsGiven()
    {
        const QVariantMap effect = mediasettings::newEffect(adjustColor);
        QCOMPARE(effect.value("name").toString(), QStringLiteral("Adjust Color"));
        QCOMPARE(effect.value("variables").toList().size(), 4);
        QCOMPARE(effect.value("numbers").toList(), QVariantList({0.0, 1.0, 0.0, 1.0}));
        QVERIFY(mediasettings::newEffect("NOT-AN-EFFECT").isEmpty());

        // An Adjust Color with only two of its variables, the other way round
        rv::data::Media media = appsPicture();
        rv::data::Effect *partial = media.mutable_image()->mutable_drawing()->add_effects();
        partial->set_enabled(true);
        partial->set_render_id(adjustColor.toStdString());
        addFloat(partial, "Contrast", "Adjust contrast", 1.5f, 1, 2);
        addFloat(partial, "Hue", "Adjust hue", 0.25f, 0, 3.14f);
        const QVariantMap described = mediasettings::describe(media).value("effects").toList().at(0).toMap();
        QCOMPARE(described.value("numbers").toList(), QVariantList({0.25, 1.0, 0.0, 1.5}));
        QCOMPARE(described.value("name").toString(), QStringLiteral("Adjust Color"));

        // Colours apart from numbers: Color Swap's two
        const QVariantMap swap = mediasettings::newEffect("7511AE15-3C2D-4840-BD66-DDC0C4A3DABE");
        QCOMPARE(swap.value("numbers").toList(), QVariantList({0.5}));
        QCOMPARE(swap.value("colors").toList().size(), 2);
        QCOMPARE(swap.value("colors").toList().at(0).value<QColor>(), QColor::fromRgbF(0.5, 0.5, 0.5, 1));
    }

    // The catalogue: ProPresenter's forty, each once, by name, all drawn here but the two
    // that work on what is behind
    void catalogue()
    {
        const QList<mediaeffects::Effect> &all = mediaeffects::all();
        QCOMPARE(all.size(), 40);
        QSet<QString> ids;
        int notDrawn = 0;
        for (int i = 0; i < all.size(); ++i) {
            ids.insert(all.at(i).renderId);
            if (i > 0)
                QVERIFY(all.at(i - 1).name.compare(all.at(i).name, Qt::CaseInsensitive) < 0);
            notDrawn += all.at(i).shader.isEmpty() ? 1 : 0;
            QVERIFY(all.at(i).variables.size() <= 6);
        }
        QCOMPARE(ids.size(), 40);
        QCOMPARE(notDrawn, 2);
        QVERIFY(mediaeffects::find(vignette.toLower()) != nullptr);
    }

    // Another file in its place keeps the settings that still make sense
    void carriesOver()
    {
        rv::data::Media from = prosVideo();
        QCOMPARE(mediasettings::apply(&from, {{"flipV", true}, {"rotation", 270}, {"inPoint", 4}, {"outPoint", 9}}), QString());
        rv::data::Media to;
        to.mutable_video()->mutable_drawing()->mutable_natural_size()->set_width(1280);
        to.mutable_video()->mutable_transport()->set_play_rate(1);
        mediasettings::carryOver(from, &to);
        const QVariantMap settings = mediasettings::describe(to);
        QCOMPARE(settings.value("scale").toInt(), 2);
        QCOMPARE(settings.value("flipV").toBool(), true);
        QCOMPARE(settings.value("rotation").toInt(), 270);
        QCOMPARE(settings.value("effects").toList().size(), 2);
        QCOMPARE(settings.value("playback").toInt(), 1);
        // Not its crop, its trim, its picture or its size
        QCOMPARE(settings.value("cropOn").toBool(), false);
        QCOMPARE(settings.value("inPoint").toDouble(), 0.0);
        QCOMPARE(settings.value("trimmedOut").toBool(), false);
        QCOMPARE(settings.value("thumbnail").toDouble(), 0.0);
        QCOMPARE(settings.value("naturalWidth").toDouble(), 1280.0);
    }
};

QTEST_GUILESS_MAIN(MediaSettingsTest)
#include "tst_mediasettings.moc"
