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
    AVFrame *frame = av_frame_alloc();
    int stream = -1;

    ~Video()
    {
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
    video.decoder->skip_frame = AVDISCARD_NONKEY;
    if (avcodec_open2(video.decoder, codec, nullptr) < 0)
        return {};

    // A tenth of the way in, five seconds at most; from the start if the length is not known.
    bool got = false;
    if (video.format->duration > 0) {
        const int64_t target = qMin<int64_t>(video.format->duration / 10, 5 * AV_TIME_BASE);
        if (av_seek_frame(video.format, -1, target, AVSEEK_FLAG_BACKWARD) >= 0)
            got = nextFrame(&video, true);
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

    const AVPixFmtDescriptor *description = av_pix_fmt_desc_get(AVPixelFormat(frame->format));
    const bool hasAlpha = description && (description->flags & AV_PIX_FMT_FLAG_ALPHA);
    QImage image(scaledWidth, scaledHeight, hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    if (image.isNull())
        return {};

    // Averaging over the area each new pixel covers is the right way to make a picture
    // much smaller, and quick.
    SwsContext *scaler = sws_getContext(frame->width, frame->height, AVPixelFormat(frame->format), scaledWidth,
                                        scaledHeight, AV_PIX_FMT_RGB32, SWS_AREA, nullptr, nullptr, nullptr);
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

    return rotation == 0 ? image : image.transformed(QTransform().rotate(rotation));
}
