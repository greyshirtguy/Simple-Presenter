#include "videoframe.h"

#include <QElapsedTimer>
#include <QTransform>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/display.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace {

// How long a file may take before it is given up on: a file on a drive that has gone
// away must not hold a worker for ever.
const qint64 patience = 10000;
// How many packets to read looking for a frame before giving up.
const int packetLimit = 3000;
// How far into a video its picture is taken from, in seconds: far enough to be past a
// fade in from black, which is how a great many begin. A video too short for that to
// make sense gives its middle.
const double wantedAt = 5;
// How much later than that the picture may be from, when a keyframe is being looked for
// to take as it stands, or a picture that is not dark; and how far through the video at
// most, so that it is not a picture of the ending.
const double window = 10;
const double latestShare = 0.7;
// A picture this dim on the whole, black being 0 and white 1, says no more than a black
// one, and a later one is looked for: no more than this many keyframes are tried, each
// at least a second after the last.
const double dark = 0.1;
const int keyframesTried = 6;
// How long may be spent decoding up to the moment wanted when there is no keyframe to
// take, in milliseconds. A large video on a small processor does not get all the way in
// that, and gives the picture it had got to.
const qint64 decodingBudget = 2500;

int outOfPatience(void *started)
{
    return static_cast<QElapsedTimer *>(started)->elapsed() > patience;
}

// Everything libav hands out is released when this goes.
struct Video
{
    AVFormatContext *format = nullptr;
    AVCodecContext *decoder = nullptr;
    AVPacket *packet = av_packet_alloc();
    // The frame that is being kept, and where one comes out of the decoder before it is
    // known to be worth keeping
    AVFrame *frame = av_frame_alloc();
    AVFrame *scratch = av_frame_alloc();
    int stream = -1;

    ~Video()
    {
        av_frame_free(&scratch);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&decoder);
        avformat_close_input(&format);
    }
};

// Decodes the next frame, reading at most `packetLimit` packets. With `keyframesOnly`
// everything but keyframes is passed over, and the decoder is told after the first that
// nothing more is coming, so that it gives the frame up at once instead of waiting to
// see what follows it.
bool nextFrame(Video *video, bool keyframesOnly)
{
    for (int read = 0; read < packetLimit && av_read_frame(video->format, video->packet) >= 0; ++read) {
        const bool wanted = video->packet->stream_index == video->stream
                            && (!keyframesOnly || (video->packet->flags & AV_PKT_FLAG_KEY));
        const bool sent = wanted && avcodec_send_packet(video->decoder, video->packet) >= 0;
        av_packet_unref(video->packet);
        if (!sent)
            continue;
        if (keyframesOnly)
            avcodec_send_packet(video->decoder, nullptr);
        if (avcodec_receive_frame(video->decoder, video->frame) >= 0)
            return true;
        if (keyframesOnly)
            return false;
    }
    // The end of the file: whatever the decoder is still holding.
    avcodec_send_packet(video->decoder, nullptr);
    return avcodec_receive_frame(video->decoder, video->frame) >= 0;
}

// A frame as a picture of the given size. `method` is how swscale is to go about it.
QImage toImage(const AVFrame *frame, int width, int height, int method)
{
    const AVPixFmtDescriptor *description = av_pix_fmt_desc_get(AVPixelFormat(frame->format));
    const bool hasAlpha = description && (description->flags & AV_PIX_FMT_FLAG_ALPHA);
    QImage image(width, height, hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    if (image.isNull())
        return {};
    SwsContext *scaler = sws_getContext(frame->width, frame->height, AVPixelFormat(frame->format), width, height,
                                        AV_PIX_FMT_RGB32, method, nullptr, nullptr, nullptr);
    if (!scaler)
        return {};
    // High-definition video keeps its colours by a different rule from standard
    // definition's; a file that does not say which is taken by its size.
    const bool hd = frame->colorspace == AVCOL_SPC_BT709
                    || (frame->colorspace == AVCOL_SPC_UNSPECIFIED && frame->height >= 720);
    const int *coefficients = sws_getCoefficients(hd ? SWS_CS_ITU709 : SWS_CS_ITU601);
    sws_setColorspaceDetails(scaler, coefficients, frame->color_range == AVCOL_RANGE_JPEG,
                             sws_getCoefficients(SWS_CS_DEFAULT), 1, 0, 1 << 16, 1 << 16);
    uint8_t *lines[4] = {image.bits(), nullptr, nullptr, nullptr};
    int strides[4] = {int(image.bytesPerLine()), 0, 0, 0};
    sws_scale(scaler, frame->data, frame->linesize, 0, frame->height, lines, strides);
    sws_freeContext(scaler);
    return image;
}

// How bright a frame is on the whole, from 0 for black to 1 for white: the average over
// a few hundred points of it. A frame it cannot be told of counts as bright, and is
// taken as it is.
double brightnessOf(const AVFrame *frame)
{
    const QImage points = toImage(frame, 32, 18, SWS_POINT);
    if (points.isNull())
        return 1;
    int sum = 0;
    for (int y = 0; y < points.height(); ++y) {
        for (int x = 0; x < points.width(); ++x)
            sum += qGray(points.pixel(x, y));
    }
    return sum / (255.0 * points.width() * points.height());
}

// Reads on to the next keyframe of the video and says when it is to be shown, in the
// stream's own units, leaving it in video->packet for whoever asked to decode or to
// drop. False if none comes, or it does not say when.
bool nextKeyframe(Video *video, int64_t *at)
{
    for (int read = 0; read < packetLimit && av_read_frame(video->format, video->packet) >= 0; ++read) {
        if (video->packet->stream_index == video->stream && (video->packet->flags & AV_PKT_FLAG_KEY)) {
            *at = video->packet->pts != AV_NOPTS_VALUE ? video->packet->pts : video->packet->dts;
            return *at != AV_NOPTS_VALUE;
        }
        av_packet_unref(video->packet);
    }
    return false;
}

// Decodes the one keyframe that nextKeyframe() found, into video->scratch.
bool decodeKeyframe(Video *video)
{
    avcodec_flush_buffers(video->decoder);
    const bool sent = avcodec_send_packet(video->decoder, video->packet) >= 0;
    av_packet_unref(video->packet);
    // Nothing more is coming, so that the decoder gives the frame up at once.
    avcodec_send_packet(video->decoder, nullptr);
    return sent && avcodec_receive_frame(video->decoder, video->scratch) >= 0;
}

// Makes the frame in video->scratch the one kept, in video->frame.
void keep(Video *video)
{
    av_frame_unref(video->frame);
    av_frame_move_ref(video->frame, video->scratch);
}

// The cheap way to a picture from `from` or soon after (both in the stream's own
// units, as is `last`, the latest it may be from): a keyframe is a whole picture by
// itself, and one that comes in that stretch is taken as it stands. If it is dark the
// next is tried, a second or more further on, and so on for a few; the brightest of
// those tried is what is left in video->frame. False if there is no keyframe in the
// stretch at all, which leaves the other way.
bool keyframeFrom(Video *video, int64_t from, int64_t last, int64_t second)
{
    double best = -1;
    for (int tried = 0; tried < keyframesTried; ++tried) {
        int64_t at = 0;
        if (av_seek_frame(video->format, video->stream, from, 0) < 0 || !nextKeyframe(video, &at))
            break;
        if (at < from || at > last) {
            av_packet_unref(video->packet);
            break;
        }
        if (!decodeKeyframe(video))
            break;
        const double brightness = brightnessOf(video->scratch);
        if (brightness > best) {
            best = brightness;
            keep(video);
        }
        if (brightness >= dark)
            break;
        from = at + second;
    }
    return best >= 0;
}

// The other way: from the keyframe before the moment, every frame up to it, which is
// what a player does to show an exact moment. The file having been sought to that
// keyframe, this decodes on until the frame for `target` comes out and leaves it in
// video->frame; if that frame is dark it goes on, looking at one every `step`, as far
// as `last`, and what is left is the brightest it looked at. If `budget` milliseconds
// of `clock` run out before the moment is reached, or the file ends, what is left is
// the latest frame there was. False if no frame came out at all.
bool frameAt(Video *video, int64_t target, int64_t last, int64_t step, const QElapsedTimer &clock, qint64 budget)
{
    bool got = false;
    bool ended = false;
    // Negative until a frame has been looked at for how bright it is
    double best = -1;
    int64_t next = target;
    for (int read = 0; read < packetLimit && !ended; ++read) {
        if (av_read_frame(video->format, video->packet) >= 0) {
            if (video->packet->stream_index == video->stream)
                avcodec_send_packet(video->decoder, video->packet);
            av_packet_unref(video->packet);
        } else {
            // The end of the file: whatever the decoder is still holding.
            avcodec_send_packet(video->decoder, nullptr);
            ended = true;
        }
        while (avcodec_receive_frame(video->decoder, video->scratch) >= 0) {
            const int64_t at = video->scratch->best_effort_timestamp;
            if (at != AV_NOPTS_VALUE && at < next) {
                // Not there yet. Until there is a frame from the moment itself, the
                // one got to is the one to have.
                if (best < 0) {
                    keep(video);
                    got = true;
                }
                continue;
            }
            const double brightness = brightnessOf(video->scratch);
            if (brightness > best) {
                best = brightness;
                keep(video);
                got = true;
            }
            if (brightness >= dark || at == AV_NOPTS_VALUE || at >= last)
                return true;
            next = at + step;
        }
        if (clock.elapsed() > budget)
            break;
    }
    return got;
}

// How far the video is to be turned to be upright, in degrees clockwise: a phone
// records sideways and says so.
int rotationOf(const AVStream *stream)
{
    const int32_t *matrix = nullptr;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(60, 31, 102)
    const AVPacketSideData *data = av_packet_side_data_get(stream->codecpar->coded_side_data,
                                                           stream->codecpar->nb_coded_side_data,
                                                           AV_PKT_DATA_DISPLAYMATRIX);
    if (data)
        matrix = reinterpret_cast<const int32_t *>(data->data);
#else
    matrix = reinterpret_cast<const int32_t *>(av_stream_get_side_data(stream, AV_PKT_DATA_DISPLAYMATRIX, nullptr));
#endif
    if (!matrix)
        return 0;
    // The matrix turns anticlockwise.
    const int degrees = qRound(-av_display_rotation_get(matrix));
    return ((degrees % 360) + 360) % 360;
}

} // namespace

QImage grabVideoFrame(const QString &path, int width)
{
    QElapsedTimer started;
    started.start();

    Video video;
    video.format = avformat_alloc_context();
    if (!video.format || !video.packet || !video.frame)
        return {};
    video.format->interrupt_callback.callback = outOfPatience;
    video.format->interrupt_callback.opaque = &started;
    if (avformat_open_input(&video.format, path.toUtf8().constData(), nullptr, nullptr) < 0)
        return {};

    // The usual next step reads into the file, decoding as it goes, to work out what
    // the streams are. Most containers say outright (an MP4 does), and then that is
    // most of the time a thumbnail takes, spent finding out what is already known.
    bool described = false;
    for (unsigned i = 0; i < video.format->nb_streams; ++i) {
        const AVCodecParameters *parameters = video.format->streams[i]->codecpar;
        if (parameters->codec_type == AVMEDIA_TYPE_VIDEO && parameters->codec_id != AV_CODEC_ID_NONE
            && parameters->width > 0 && parameters->height > 0)
            described = true;
    }
    if (!described && avformat_find_stream_info(video.format, nullptr) < 0)
        return {};

    const AVCodec *codec = nullptr;
    video.stream = av_find_best_stream(video.format, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (video.stream < 0 || !codec)
        return {};
    const AVStream *stream = video.format->streams[video.stream];
    video.decoder = avcodec_alloc_context3(codec);
    if (!video.decoder || avcodec_parameters_to_context(video.decoder, stream->codecpar) < 0)
        return {};
    // One frame is wanted, and several files are done at once: threads within a
    // decoder would only get in each other's way.
    video.decoder->thread_count = 1;
    if (avcodec_open2(video.decoder, codec, nullptr) < 0)
        return {};

    // How long the video is, and where its clock starts. The file as a whole only says
    // once its streams have been looked into, which was passed over above where it
    // could be; the video's own stream says at once.
    int64_t duration = video.format->duration;
    if (duration <= 0 && stream->duration > 0)
        duration = av_rescale_q(stream->duration, stream->time_base, AV_TIME_BASE_Q);
    int64_t start = video.format->start_time;
    if (start == AV_NOPTS_VALUE)
        start = stream->start_time != AV_NOPTS_VALUE ? av_rescale_q(stream->start_time, stream->time_base, AV_TIME_BASE_Q) : 0;

    // Five seconds in, or half way through a video shorter than ten; from the start if
    // the length is not known.
    bool got = false;
    if (duration > 0) {
        const AVRational units = stream->time_base;
        const auto inUnits = [units](double seconds) {
            return av_rescale_q(int64_t(seconds * AV_TIME_BASE), AV_TIME_BASE_Q, units);
        };
        const double length = duration / double(AV_TIME_BASE);
        const double wanted = qMin(wantedAt, length / 2);
        const int64_t first = av_rescale_q(start, AV_TIME_BASE_Q, units);
        const int64_t target = first + inUnits(wanted);
        const int64_t last = first + inUnits(qMax(wanted, qMin(wanted + window, length * latestShare)));

        got = keyframeFrom(&video, target, last, inUnits(1));
        if (!got) {
            avcodec_flush_buffers(video.decoder);
            // Frames that nothing after them is built from can be passed over.
            video.decoder->skip_frame = AVDISCARD_NONREF;
            if (av_seek_frame(video.format, video.stream, target, AVSEEK_FLAG_BACKWARD) >= 0) {
                QElapsedTimer decoding;
                decoding.start();
                got = frameAt(&video, target, last, inUnits(0.5), decoding, decodingBudget);
            }
        }
    }
    if (!got) {
        // Not every file marks its keyframes, or can be sought in. From the top, then,
        // taking the first frame that comes out.
        avcodec_flush_buffers(video.decoder);
        video.decoder->skip_frame = AVDISCARD_DEFAULT;
        if (av_seek_frame(video.format, -1, 0, AVSEEK_FLAG_BACKWARD) < 0)
            return {};
        got = nextFrame(&video, false);
    }
    const AVFrame *frame = video.frame;
    if (!got || frame->width <= 0 || frame->height <= 0)
        return {};

    // The size it is shown at: pixels are not always square.
    const AVRational aspect = av_guess_sample_aspect_ratio(video.format, video.format->streams[video.stream], video.frame);
    const double shownWidth = aspect.num > 0 && aspect.den > 0 ? frame->width * double(aspect.num) / aspect.den
                                                               : frame->width;
    const int rotation = rotationOf(stream);
    // Scaled so that it is `width` wide once turned upright.
    const bool sideways = rotation == 90 || rotation == 270;
    const double scale = qMin(1.0, width / (sideways ? double(frame->height) : shownWidth));
    const int scaledWidth = qMax(1, qRound(shownWidth * scale));
    const int scaledHeight = qMax(1, qRound(frame->height * scale));

    // Averaging over the area each new pixel covers is the right way to make a picture
    // much smaller, and quick.
    const QImage image = toImage(frame, scaledWidth, scaledHeight, SWS_AREA);
    if (image.isNull())
        return {};

    return rotation == 0 ? image : image.transformed(QTransform().rotate(rotation));
}
