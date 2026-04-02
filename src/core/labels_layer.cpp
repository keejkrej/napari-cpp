#include "core/labels_layer.h"

#include <QQueue>

#include <stdexcept>

#include "core/dims_model.h"

namespace napari_cpp {

LabelsLayer::LabelsLayer(NdImage image, QString name, QObject *parent)
    : Layer(name.isEmpty() ? image.name() : name, parent), image_(std::move(image))
{
    if (!image_.isScalar() || !image_.isInteger()) {
        throw std::runtime_error("LabelsLayer requires scalar integer image data");
    }
    if (this->name().isEmpty()) {
        setName(QStringLiteral("Labels"));
    }
}

Layer::Kind LabelsLayer::kind() const
{
    return Kind::Labels;
}

QVector<int> LabelsLayer::shape() const
{
    return image_.shape();
}

QSize LabelsLayer::planeSize() const
{
    return image_.planeSize();
}

const NdImage &LabelsLayer::image() const
{
    return image_;
}

NdImage &LabelsLayer::image()
{
    return image_;
}

quint32 LabelsLayer::selectedLabel() const
{
    return selectedLabel_;
}

int LabelsLayer::brushSize() const
{
    return brushSize_;
}

LabelsLayer::Mode LabelsLayer::mode() const
{
    return mode_;
}

bool LabelsLayer::showSelectedLabel() const
{
    return showSelectedLabel_;
}

bool LabelsLayer::contiguous() const
{
    return contiguous_;
}

void LabelsLayer::setSelectedLabel(const quint32 label)
{
    if (selectedLabel_ == label) {
        return;
    }

    selectedLabel_ = label;
    emit selectedLabelChanged(selectedLabel_);
    emit changed();
}

void LabelsLayer::setBrushSize(const int brushSize)
{
    const int clamped = qMax(1, brushSize);
    if (brushSize_ == clamped) {
        return;
    }

    brushSize_ = clamped;
    emit brushSizeChanged(brushSize_);
    emit changed();
}

void LabelsLayer::setMode(const Mode mode)
{
    if (mode_ == mode) {
        return;
    }

    mode_ = mode;
    emit modeChanged(mode_);
    emit changed();
}

void LabelsLayer::setShowSelectedLabel(const bool showSelectedLabel)
{
    if (showSelectedLabel_ == showSelectedLabel) {
        return;
    }

    showSelectedLabel_ = showSelectedLabel;
    emit showSelectedLabelChanged(showSelectedLabel_);
    emit changed();
}

void LabelsLayer::setContiguous(const bool contiguous)
{
    if (contiguous_ == contiguous) {
        return;
    }

    contiguous_ = contiguous;
    emit contiguousChanged(contiguous_);
    emit changed();
}

quint32 LabelsLayer::pickLabel(const QPoint &point, const DimsModel &dims) const
{
    qsizetype scalarIndex = 0;
    qsizetype baseOffset = 0;
    qsizetype rowStride = 0;
    qsizetype columnStride = 0;
    if (!pointToScalarIndex(point, dims, scalarIndex, baseOffset, rowStride, columnStride)) {
        return 0;
    }

    return image_.integerValue(scalarIndex);
}

bool LabelsLayer::pickAndSetLabel(const QPoint &point, const DimsModel &dims)
{
    setSelectedLabel(pickLabel(point, dims));
    return true;
}

bool LabelsLayer::paint(const QPoint &point, const DimsModel &dims)
{
    return applyBrush(point, dims, selectedLabel_);
}

bool LabelsLayer::erase(const QPoint &point, const DimsModel &dims)
{
    return applyBrush(point, dims, 0);
}

bool LabelsLayer::fill(const QPoint &point, const DimsModel &dims)
{
    return applyFill(point, dims, selectedLabel_);
}

bool LabelsLayer::sliceInfo(const DimsModel &dims,
                            qsizetype &baseOffset,
                            qsizetype &rowStride,
                            qsizetype &columnStride) const
{
    const QVector<qsizetype> strides = image_.scalarStrides();
    const QVector<int> layerShape = image_.shape();
    const int globalNdim = dims.ndim();
    const int layerNdim = image_.ndim();
    const int axisOffset = qMax(0, globalNdim - layerNdim);

    baseOffset = 0;
    for (int axis = 0; axis < layerNdim - 2; ++axis) {
        const int globalAxis = axis + axisOffset;
        const int index = qBound(0, dims.currentIndex(globalAxis), qMax(0, layerShape.at(axis) - 1));
        baseOffset += strides.at(axis) * static_cast<qsizetype>(index);
    }

    rowStride = strides.at(layerNdim - 2);
    columnStride = strides.at(layerNdim - 1);
    return true;
}

bool LabelsLayer::pointToScalarIndex(const QPoint &point,
                                     const DimsModel &dims,
                                     qsizetype &scalarIndex,
                                     qsizetype &baseOffset,
                                     qsizetype &rowStride,
                                     qsizetype &columnStride) const
{
    if (point.x() < 0 || point.y() < 0 || point.x() >= planeSize().width() || point.y() >= planeSize().height()) {
        return false;
    }

    if (!sliceInfo(dims, baseOffset, rowStride, columnStride)) {
        return false;
    }

    scalarIndex = baseOffset + static_cast<qsizetype>(point.y()) * rowStride
                  + static_cast<qsizetype>(point.x()) * columnStride;
    return true;
}

bool LabelsLayer::applyBrush(const QPoint &point, const DimsModel &dims, const quint32 label)
{
    qsizetype baseOffset = 0;
    qsizetype rowStride = 0;
    qsizetype columnStride = 0;
    qsizetype scalarIndex = 0;
    if (!pointToScalarIndex(point, dims, scalarIndex, baseOffset, rowStride, columnStride)) {
        return false;
    }

    const int radius = qMax(0, brushSize_ / 2);
    bool changedData = false;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > radius * radius) {
                continue;
            }

            const QPoint current(point.x() + dx, point.y() + dy);
            if (current.x() < 0 || current.y() < 0 || current.x() >= planeSize().width()
                || current.y() >= planeSize().height()) {
                continue;
            }

            const qsizetype currentIndex = baseOffset + static_cast<qsizetype>(current.y()) * rowStride
                                           + static_cast<qsizetype>(current.x()) * columnStride;
            if (image_.integerValue(currentIndex) == label) {
                continue;
            }
            changedData |= image_.setIntegerValue(currentIndex, label);
        }
    }

    if (changedData) {
        emitImageMutation();
    }
    return changedData;
}

bool LabelsLayer::applyFill(const QPoint &point, const DimsModel &dims, const quint32 label)
{
    qsizetype baseOffset = 0;
    qsizetype rowStride = 0;
    qsizetype columnStride = 0;
    qsizetype startIndex = 0;
    if (!pointToScalarIndex(point, dims, startIndex, baseOffset, rowStride, columnStride)) {
        return false;
    }

    const quint32 targetLabel = image_.integerValue(startIndex);
    if (targetLabel == label) {
        return false;
    }

    bool changedData = false;
    const int width = planeSize().width();
    const int height = planeSize().height();
    if (!contiguous_) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const qsizetype currentIndex = baseOffset + static_cast<qsizetype>(y) * rowStride
                                               + static_cast<qsizetype>(x) * columnStride;
                if (image_.integerValue(currentIndex) == targetLabel) {
                    changedData |= image_.setIntegerValue(currentIndex, label);
                }
            }
        }
    } else {
        QVector<quint8> visited(width * height, 0);
        QQueue<QPoint> queue;
        queue.enqueue(point);

        while (!queue.isEmpty()) {
            const QPoint current = queue.dequeue();
            const int flatIndex = current.y() * width + current.x();
            if (visited[flatIndex] != 0) {
                continue;
            }
            visited[flatIndex] = 1;

            const qsizetype currentIndex = baseOffset + static_cast<qsizetype>(current.y()) * rowStride
                                           + static_cast<qsizetype>(current.x()) * columnStride;
            if (image_.integerValue(currentIndex) != targetLabel) {
                continue;
            }

            changedData |= image_.setIntegerValue(currentIndex, label);

            if (current.x() > 0) {
                queue.enqueue(QPoint(current.x() - 1, current.y()));
            }
            if (current.x() + 1 < width) {
                queue.enqueue(QPoint(current.x() + 1, current.y()));
            }
            if (current.y() > 0) {
                queue.enqueue(QPoint(current.x(), current.y() - 1));
            }
            if (current.y() + 1 < height) {
                queue.enqueue(QPoint(current.x(), current.y() + 1));
            }
        }
    }

    if (changedData) {
        emitImageMutation();
    }
    return changedData;
}

void LabelsLayer::emitImageMutation()
{
    emit imageChanged();
    emit changed();
}

}  // namespace napari_cpp
