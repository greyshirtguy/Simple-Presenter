#include "textshadow.h"

#include <QPainter>
#include <QtMath>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

// The widths of three box blurs that, one after another, blur about as much as a Gaussian of
// `sigma` does: a box `w` wide spreads by (w² - 1) / 12, the three adding up to sigma², in
// odd widths of two sizes, the narrower first. (Three boxes are within a few per cent of a
// Gaussian, at a cost that does not grow with the blur.)
std::array<int, 3> threeBoxes(double sigma)
{
    int narrow = int(std::floor(std::sqrt(4 * sigma * sigma + 1)));
    if (narrow % 2 == 0)
        --narrow;
    const int narrowOnes = int(std::round((12 * sigma * sigma - 3 * narrow * narrow - 12 * narrow - 9) / (-4.0 * narrow - 4)));
    std::array<int, 3> widths;
    for (int i = 0; i < 3; ++i)
        widths[size_t(i)] = i < narrowOnes ? narrow : narrow + 2;
    return widths;
}

// A box blur `width` wide (odd) along each row of a `columns` by `rows` grid of values,
// from `in` to `out`, what is beyond the grid counting as nothing. (The values are covers
// of 0 to 255 with eight bits more below the point: kept to whole numbers of 0 to 255 at
// each of the six blurs, the faint edge of a wide shadow was rounded away, and a dot's
// shadow came out with 60% of its cover at a sigma of 12. tests/unit/tst_textshadow.)
void boxAcross(const uint16_t *in, uint16_t *out, int columns, int rows, int width)
{
    const int reach = width / 2;
    const uint64_t scale = (uint64_t(1) << 32) / uint64_t(width);
    const auto mean = [scale](uint32_t sum) { return uint16_t((sum * scale + (uint64_t(1) << 31)) >> 32); };
    // (Where the box reaches past neither end, past the start, past the end, or both)
    const int start = std::min(reach + 1, columns);
    const int end = std::max(start, columns - reach);
    for (int y = 0; y < rows; ++y) {
        const uint16_t *from = in + qsizetype(y) * columns;
        uint16_t *to = out + qsizetype(y) * columns;
        uint32_t sum = 0;
        for (int x = 0; x < std::min(reach, columns); ++x)
            sum += from[x];
        int x = 0;
        for (; x < start; ++x) {
            if (x + reach < columns)
                sum += from[x + reach];
            to[x] = mean(sum);
        }
        for (; x < end; ++x) {
            sum += from[x + reach];
            sum -= from[x - reach - 1];
            to[x] = mean(sum);
        }
        for (; x < columns; ++x) {
            if (x + reach < columns)
                sum += from[x + reach];
            sum -= from[x - reach - 1];
            to[x] = mean(sum);
        }
    }
}

// The same down each column: a row at a time, with a running sum for each column, which
// goes along memory as it lies
void boxDown(const uint16_t *in, uint16_t *out, int columns, int rows, int width)
{
    const int reach = width / 2;
    const uint64_t scale = (uint64_t(1) << 32) / uint64_t(width);
    std::vector<uint32_t> sums(size_t(columns), 0);
    uint32_t *sum = sums.data();
    const auto add = [&](int y) {
        const uint16_t *row = in + qsizetype(y) * columns;
        for (int x = 0; x < columns; ++x)
            sum[x] += row[x];
    };
    const auto take = [&](int y) {
        const uint16_t *row = in + qsizetype(y) * columns;
        for (int x = 0; x < columns; ++x)
            sum[x] -= row[x];
    };
    for (int y = 0; y < std::min(reach, rows); ++y)
        add(y);
    for (int y = 0; y < rows; ++y) {
        if (y + reach < rows)
            add(y + reach);
        if (y - reach - 1 >= 0)
            take(y - reach - 1);
        uint16_t *to = out + qsizetype(y) * columns;
        for (int x = 0; x < columns; ++x)
            to[x] = uint16_t((sum[x] * scale + (uint64_t(1) << 31)) >> 32);
    }
}

} // namespace

namespace textshadow {

double multiEffectSigma(double radius)
{
    static const double measured[][2] = {{0, 0},     {3.2, 0.75}, {6.4, 1.25},  {9.6, 1.75},  {12.8, 2.25}, {16, 2.75},
                                         {19.2, 3.25}, {25.6, 4.75}, {32, 6.75}, {38.4, 8.75}, {51.2, 13.5}, {64, 18.75}};
    const int count = int(std::size(measured));
    if (radius <= 0)
        return 0;
    if (radius >= measured[count - 1][0])
        return measured[count - 1][1];
    int k = 1;
    while (measured[k][0] < radius)
        ++k;
    const double along = (radius - measured[k - 1][0]) / (measured[k][0] - measured[k - 1][0]);
    return measured[k - 1][1] + along * (measured[k][1] - measured[k - 1][1]);
}

void cast(QImage &picture, const QColor &color, QPointF offset, double sigma)
{
    if (color.alpha() == 0 || picture.isNull())
        return;
    const int step = sigma < 1.5 ? 1 : sigma < 4 ? 2 : sigma < 8 ? 4 : 8;
    const int width = picture.width();
    const int height = picture.height();
    // The cover, a cell of `step` pixels square to each value, with room round it for the blur
    const int pad = int(std::ceil(3 * sigma / step)) + 1;
    const int columns = (width + step - 1) / step + 2 * pad;
    const int rows = (height + step - 1) / step + 2 * pad;
    std::vector<uint16_t> cover(size_t(columns) * size_t(rows), 0);
    std::vector<uint16_t> other(cover.size(), 0);
    // (A row of cells at a time: the rows of pixels in it summed, then each cell's part of
    // the sum. Each pixel's cover is its opacity, the top byte of a pixel of the picture's
    // kind. Read from a picture let go before the painting below, which would otherwise
    // copy the picture to paint on, the two sharing it.)
    {
        const QImage covered = picture.format() == QImage::Format_ARGB32_Premultiplied
            ? picture : picture.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const int cells = (width + step - 1) / step;
        std::vector<uint32_t> sums(size_t(cells) * size_t(step), 0);
        uint32_t *sum = sums.data();
        const uint32_t cell = uint32_t(step * step);
        for (int top = 0; top < height; top += step) {
            std::fill(sums.begin(), sums.end(), 0);
            for (int y = top; y < std::min(height, top + step); ++y) {
                const QRgb *line = reinterpret_cast<const QRgb *>(covered.constScanLine(y));
                for (int x = 0; x < width; ++x)
                    sum[x] += line[x] >> 24;
            }
            uint16_t *to = cover.data() + size_t(top / step + pad) * size_t(columns) + size_t(pad);
            for (int c = 0; c < cells; ++c) {
                uint32_t total = 0;
                for (int i = c * step; i < c * step + step; ++i)
                    total += sum[i];
                to[c] = uint16_t((total * 256 + cell / 2) / cell);
            }
        }
    }
    // Blurred: three boxes across, then three down
    const std::array<int, 3> boxes = threeBoxes(sigma / step);
    for (const int box : boxes) {
        if (box > 1) {
            boxAcross(cover.data(), other.data(), columns, rows, box);
            cover.swap(other);
        }
    }
    for (const int box : boxes) {
        if (box > 1) {
            boxDown(cover.data(), other.data(), columns, rows, box);
            cover.swap(other);
        }
    }
    // In the shadow's colour, each cover a shade of it
    std::array<QRgb, 256> shades;
    for (int i = 0; i < 256; ++i)
        shades[size_t(i)] = qPremultiply(qRgba(color.red(), color.green(), color.blue(), (color.alpha() * i + 127) / 255));
    QImage shadow(columns, rows, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < rows; ++y) {
        QRgb *to = reinterpret_cast<QRgb *>(shadow.scanLine(y));
        const uint16_t *from = cover.data() + size_t(y) * size_t(columns);
        for (int x = 0; x < columns; ++x)
            to[x] = shades[size_t((from[x] + 128) >> 8)];
    }
    // Put under what is there, moved by the offset (the painter draws in the picture's
    // pixels over its ratio)
    const qreal ratio = picture.devicePixelRatio();
    QPainter painter(&picture);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, step > 1);
    painter.drawImage(QRectF((QPointF(-pad * step, -pad * step) + offset) / ratio, QSizeF(columns * step, rows * step) / ratio), shadow);
}

} // namespace textshadow
