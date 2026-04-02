#include "core/dims_model.h"

#include <QtGlobal>

namespace napari_cpp {

DimsModel::DimsModel(QObject *parent)
    : QObject(parent)
{
}

int DimsModel::ndim() const
{
    return shape_.size();
}

QVector<int> DimsModel::shape() const
{
    return shape_;
}

QStringList DimsModel::axisLabels() const
{
    return axisLabels_;
}

QVector<int> DimsModel::currentIndices() const
{
    return currentIndices_;
}

int DimsModel::currentIndex(const int axis) const
{
    if (axis < 0 || axis >= currentIndices_.size()) {
        return 0;
    }
    return currentIndices_.at(axis);
}

QVector<int> DimsModel::displayedAxes() const
{
    QVector<int> axes;
    if (shape_.size() >= 2) {
        axes << shape_.size() - 2 << shape_.size() - 1;
    }
    return axes;
}

QVector<int> DimsModel::nonDisplayedAxes() const
{
    QVector<int> axes;
    for (int axis = 0; axis < qMax(0, shape_.size() - 2); ++axis) {
        axes << axis;
    }
    return axes;
}

int DimsModel::axisExtent(const int axis) const
{
    if (axis < 0 || axis >= shape_.size()) {
        return 1;
    }
    return shape_.at(axis);
}

void DimsModel::setShape(const QVector<int> &shape)
{
    if (shape == shape_) {
        clampIndices();
        return;
    }

    const QVector<int> previousIndices = currentIndices_;
    const int oldNdim = shape_.size();
    shape_ = shape;

    currentIndices_ = QVector<int>(shape_.size(), 0);
    const int overlap = qMin(oldNdim, shape_.size());
    for (int offset = 0; offset < overlap; ++offset) {
        currentIndices_[shape_.size() - 1 - offset] = previousIndices[oldNdim - 1 - offset];
    }

    if (axisLabels_.size() != shape_.size()) {
        axisLabels_ = defaultLabels(shape_.size());
        emit axisLabelsChanged();
    }

    clampIndices();
    emit shapeChanged();
    emit indicesChanged();
}

void DimsModel::setAxisLabels(const QStringList &labels)
{
    if (labels.size() != shape_.size()) {
        return;
    }

    if (labels == axisLabels_) {
        return;
    }

    axisLabels_ = labels;
    emit axisLabelsChanged();
}

void DimsModel::setCurrentIndex(const int axis, const int index)
{
    if (axis < 0 || axis >= currentIndices_.size()) {
        return;
    }

    const int clamped = qBound(0, index, qMax(0, axisExtent(axis) - 1));
    if (currentIndices_.at(axis) == clamped) {
        return;
    }

    currentIndices_[axis] = clamped;
    emit currentIndexChanged(axis, clamped);
    emit indicesChanged();
}

void DimsModel::resetIndices()
{
    bool changed = false;
    for (int axis = 0; axis < currentIndices_.size(); ++axis) {
        if (currentIndices_[axis] != 0) {
            currentIndices_[axis] = 0;
            emit currentIndexChanged(axis, 0);
            changed = true;
        }
    }

    if (changed) {
        emit indicesChanged();
    }
}

QStringList DimsModel::defaultLabels(const int ndim) const
{
    QStringList labels;
    for (int axis = 0; axis < ndim; ++axis) {
        labels << QStringLiteral("Axis %1").arg(axis);
    }
    return labels;
}

void DimsModel::clampIndices()
{
    for (int axis = 0; axis < currentIndices_.size(); ++axis) {
        currentIndices_[axis] = qBound(0, currentIndices_.at(axis), qMax(0, axisExtent(axis) - 1));
    }
}

}  // namespace napari_cpp
