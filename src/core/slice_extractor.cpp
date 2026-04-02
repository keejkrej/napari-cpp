#include "core/slice_extractor.h"

#include <QtGlobal>

#include <cmath>

#include "core/dims_model.h"
#include "core/image_layer.h"
#include "core/labels_layer.h"
#include "core/layer.h"

namespace napari_cpp {

namespace {

struct SliceGeometry {
    qsizetype baseOffset = 0;
    qsizetype rowStride = 0;
    qsizetype columnStride = 0;
};

SliceGeometry computeSliceGeometry(const NdImage &image, const DimsModel &dims)
{
    const QVector<qsizetype> strides = image.scalarStrides();
    const QVector<int> layerShape = image.shape();
    const int globalNdim = dims.ndim();
    const int layerNdim = image.ndim();
    const int axisOffset = qMax(0, globalNdim - layerNdim);

    SliceGeometry geometry;
    for (int axis = 0; axis < layerNdim - 2; ++axis) {
        const int globalAxis = axis + axisOffset;
        const int index = qBound(0, dims.currentIndex(globalAxis), qMax(0, layerShape.at(axis) - 1));
        geometry.baseOffset += strides.at(axis) * static_cast<qsizetype>(index);
    }
    geometry.rowStride = strides.at(layerNdim - 2);
    geometry.columnStride = strides.at(layerNdim - 1);
    return geometry;
}

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

QColor imageColorForIntensity(const int intensity, const QString &colormapName)
{
    if (colormapName == QStringLiteral("invert")) {
        const int value = 255 - intensity;
        return QColor(value, value, value, 255);
    }
    if (colormapName == QStringLiteral("red")) {
        return QColor(intensity, 0, 0, 255);
    }
    if (colormapName == QStringLiteral("green")) {
        return QColor(0, intensity, 0, 255);
    }
    if (colormapName == QStringLiteral("blue")) {
        return QColor(0, 0, intensity, 255);
    }
    if (colormapName == QStringLiteral("magenta")) {
        return QColor(intensity, 0, intensity, 255);
    }
    if (colormapName == QStringLiteral("cyan")) {
        return QColor(0, intensity, intensity, 255);
    }
    if (colormapName == QStringLiteral("yellow")) {
        return QColor(intensity, intensity, 0, 255);
    }
    return QColor(intensity, intensity, intensity, 255);
}

QColor labelColor(const quint32 label)
{
    const int hue = static_cast<int>((label * 137U) % 360U);
    return QColor::fromHsv(hue, 180 + static_cast<int>((label * 29U) % 60U), 220, 255);
}

SliceResult extractImageRgba(const ImageLayer &layer, const DimsModel &dims)
{
    const NdImage &image = layer.image();
    const QSize planeSize = layer.planeSize();
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

    const SliceGeometry geometry = computeSliceGeometry(image, dims);
    for (int y = 0; y < planeSize.height(); ++y) {
        auto *row = reinterpret_cast<QRgb *>(rgbaImage.scanLine(y));
        for (int x = 0; x < planeSize.width(); ++x) {
            const qsizetype scalarIndex = geometry.baseOffset + static_cast<qsizetype>(y) * geometry.rowStride
                                          + static_cast<qsizetype>(x) * geometry.columnStride;
            const int gray = static_cast<int>(std::round(normalizeScalar(image.scalarValue(scalarIndex),
                                                                          layer.contrastLimits())));
            row[x] = imageColorForIntensity(gray, layer.colormapName()).rgba();
        }
    }

    return {rgbaImage};
}

SliceResult extractLabelsRgba(const LabelsLayer &layer, const DimsModel &dims)
{
    const NdImage &image = layer.image();
    const QSize planeSize = layer.planeSize();
    if (!planeSize.isValid()) {
        return {};
    }

    QImage rgbaImage(planeSize, QImage::Format_ARGB32);
    if (rgbaImage.isNull()) {
        return {};
    }

    const SliceGeometry geometry = computeSliceGeometry(image, dims);
    for (int y = 0; y < planeSize.height(); ++y) {
        auto *row = reinterpret_cast<QRgb *>(rgbaImage.scanLine(y));
        for (int x = 0; x < planeSize.width(); ++x) {
            const qsizetype scalarIndex = geometry.baseOffset + static_cast<qsizetype>(y) * geometry.rowStride
                                          + static_cast<qsizetype>(x) * geometry.columnStride;
            const quint32 labelValue = image.integerValue(scalarIndex);
            if (labelValue == 0) {
                row[x] = qRgba(0, 0, 0, 0);
                continue;
            }
            if (layer.showSelectedLabel() && labelValue != layer.selectedLabel()) {
                row[x] = qRgba(0, 0, 0, 0);
                continue;
            }
            row[x] = labelColor(labelValue).rgba();
        }
    }

    return {rgbaImage};
}

}  // namespace

SliceResult SliceExtractor::extractRgba(const Layer &layer, const DimsModel &dims)
{
    switch (layer.kind()) {
    case Layer::Kind::Image:
        return extractImageRgba(static_cast<const ImageLayer &>(layer), dims);
    case Layer::Kind::Labels:
        return extractLabelsRgba(static_cast<const LabelsLayer &>(layer), dims);
    }

    return {};
}

}  // namespace napari_cpp
