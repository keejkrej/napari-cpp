#include "core/slice_extractor.h"

#include <QtGlobal>

#include <cmath>

#include "core/dims_model.h"
#include "core/image_layer.h"

namespace napari_cpp {

namespace {

double normalizeScalar(const double value, const QPair<double, double> &limits)
{
    const double lower = limits.first;
    const double upper = limits.second;
    if (qFuzzyCompare(lower, upper)) {
        return value >= upper ? 255.0 : 0.0;
    }

    const double normalized = (value - lower) / (upper - lower);
    return std::clamp(normalized, 0.0, 1.0) * 255.0;
}

}  // namespace

SliceResult SliceExtractor::extractRgba(const ImageLayer &layer, const DimsModel &dims)
{
    const NdImage &image = layer.image();
    const QSize planeSize = image.planeSize();
    if (!planeSize.isValid()) {
        return {};
    }

    QImage rgbaImage(planeSize, QImage::Format_ARGB32);
    if (rgbaImage.isNull()) {
        return {};
    }

    if (image.isColor()) {
        for (int y = 0; y < planeSize.height(); ++y) {
            auto *row = reinterpret_cast<QRgb *>(rgbaImage.scanLine(y));
            for (int x = 0; x < planeSize.width(); ++x) {
                const qsizetype pixelIndex = static_cast<qsizetype>(y) * planeSize.width() + x;
                row[x] = image.rgbaValue(pixelIndex);
            }
        }
        return {rgbaImage};
    }

    const QVector<qsizetype> strides = image.scalarStrides();
    const QVector<int> layerShape = image.shape();
    const int globalNdim = dims.ndim();
    const int layerNdim = image.ndim();
    const int axisOffset = qMax(0, globalNdim - layerNdim);

    qsizetype baseOffset = 0;
    for (int axis = 0; axis < layerNdim - 2; ++axis) {
        const int globalAxis = axis + axisOffset;
        const int index = qBound(0, dims.currentIndex(globalAxis), qMax(0, layerShape.at(axis) - 1));
        baseOffset += strides.at(axis) * static_cast<qsizetype>(index);
    }

    const qsizetype rowStride = strides.at(layerNdim - 2);
    const qsizetype columnStride = strides.at(layerNdim - 1);

    for (int y = 0; y < planeSize.height(); ++y) {
        auto *row = reinterpret_cast<QRgb *>(rgbaImage.scanLine(y));
        for (int x = 0; x < planeSize.width(); ++x) {
            const qsizetype scalarIndex = baseOffset + static_cast<qsizetype>(y) * rowStride
                                          + static_cast<qsizetype>(x) * columnStride;
            const int gray = static_cast<int>(std::round(normalizeScalar(image.scalarValue(scalarIndex),
                                                                          layer.contrastLimits())));
            row[x] = qRgba(gray, gray, gray, static_cast<int>(std::round(layer.opacity() * 255.0)));
        }
    }

    return {rgbaImage};
}

}  // namespace napari_cpp
