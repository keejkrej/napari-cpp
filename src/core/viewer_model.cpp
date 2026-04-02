#include "core/viewer_model.h"

#include <QtGlobal>

namespace napari_cpp {

ViewerModel::ViewerModel(QObject *parent)
    : QObject(parent), dims_(this), camera_(this)
{
    connect(&dims_, &DimsModel::indicesChanged, this, &ViewerModel::repaintRequested);
    connect(&camera_, &Camera2D::changed, this, &ViewerModel::repaintRequested);
}

DimsModel *ViewerModel::dimsModel()
{
    return &dims_;
}

const DimsModel *ViewerModel::dimsModel() const
{
    return &dims_;
}

Camera2D *ViewerModel::camera()
{
    return &camera_;
}

const Camera2D *ViewerModel::camera() const
{
    return &camera_;
}

QVector<ImageLayer *> ViewerModel::layers() const
{
    return layers_;
}

int ViewerModel::layerCount() const
{
    return layers_.size();
}

ImageLayer *ViewerModel::layerAt(const int index) const
{
    if (index < 0 || index >= layers_.size()) {
        return nullptr;
    }
    return layers_.at(index);
}

int ViewerModel::activeLayerIndex() const
{
    return activeLayerIndex_;
}

ImageLayer *ViewerModel::activeLayer() const
{
    return layerAt(activeLayerIndex_);
}

QString ViewerModel::statusText() const
{
    return statusText_;
}

ImageLayer *ViewerModel::addLayer(std::unique_ptr<ImageLayer> layer)
{
    if (!layer) {
        return nullptr;
    }

    layer->setParent(this);
    ImageLayer *rawLayer = layer.release();
    layers_.append(rawLayer);
    connectLayer(rawLayer);
    setActiveLayerIndex(layers_.size() - 1);
    updateDimsFromLayers();
    updateStatusText();
    emit layersChanged();
    emit repaintRequested();
    return rawLayer;
}

void ViewerModel::addLayers(std::vector<std::unique_ptr<ImageLayer>> layers)
{
    for (auto &layer : layers) {
        addLayer(std::move(layer));
    }
}

void ViewerModel::removeActiveLayer()
{
    removeLayer(activeLayerIndex_);
}

void ViewerModel::removeLayer(const int index)
{
    if (index < 0 || index >= layers_.size()) {
        return;
    }

    ImageLayer *layer = layers_.takeAt(index);
    if (activeLayerIndex_ >= layers_.size()) {
        activeLayerIndex_ = layers_.isEmpty() ? -1 : layers_.size() - 1;
    } else if (index <= activeLayerIndex_) {
        activeLayerIndex_ = qMax(-1, activeLayerIndex_ - 1);
    }

    delete layer;
    updateDimsFromLayers();
    updateStatusText();
    emit layersChanged();
    emit activeLayerChanged(activeLayer());
    emit repaintRequested();
}

void ViewerModel::moveLayer(const int from, const int to)
{
    if (from < 0 || from >= layers_.size() || to < 0 || to >= layers_.size() || from == to) {
        return;
    }

    layers_.move(from, to);
    if (activeLayerIndex_ == from) {
        activeLayerIndex_ = to;
    } else if (from < activeLayerIndex_ && to >= activeLayerIndex_) {
        --activeLayerIndex_;
    } else if (from > activeLayerIndex_ && to <= activeLayerIndex_) {
        ++activeLayerIndex_;
    }

    emit layersChanged();
    emit repaintRequested();
}

void ViewerModel::setActiveLayerIndex(const int index)
{
    const int normalized = (index >= 0 && index < layers_.size()) ? index : -1;
    if (activeLayerIndex_ == normalized) {
        return;
    }

    activeLayerIndex_ = normalized;
    updateStatusText();
    emit activeLayerChanged(activeLayer());
    emit repaintRequested();
}

void ViewerModel::fitToActiveOrVisible(const QSize &canvasSize)
{
    const QSize plane = activeOrVisiblePlaneSize();
    if (plane.isValid()) {
        camera_.fitToImage(QSizeF(plane), canvasSize);
    }
}

void ViewerModel::connectLayer(ImageLayer *layer)
{
    connect(layer, &ImageLayer::changed, this, [this]() {
        updateStatusText();
        emit repaintRequested();
    });
}

void ViewerModel::updateDimsFromLayers()
{
    dims_.setShape(combinedShape());
}

void ViewerModel::updateStatusText()
{
    QStringList bits;
    bits << QStringLiteral("%1 layer%2").arg(layers_.size()).arg(layers_.size() == 1 ? QString() : QStringLiteral("s"));
    if (activeLayer() != nullptr) {
        bits << QStringLiteral("active: %1").arg(activeLayer()->name());
    }
    if (dims_.ndim() > 2) {
        QStringList positions;
        for (const int axis : dims_.nonDisplayedAxes()) {
            positions << QStringLiteral("%1=%2").arg(axis).arg(dims_.currentIndex(axis));
        }
        bits << positions.join(QStringLiteral(", "));
    }

    statusText_ = bits.join(QStringLiteral(" | "));
    emit statusTextChanged(statusText_);
}

QVector<int> ViewerModel::combinedShape() const
{
    int maxNdim = 0;
    for (const ImageLayer *layer : layers_) {
        maxNdim = qMax(maxNdim, layer->image().ndim());
    }

    QVector<int> shape(maxNdim, 1);
    for (const ImageLayer *layer : layers_) {
        const QVector<int> layerShape = layer->image().shape();
        const int offset = maxNdim - layerShape.size();
        for (int axis = 0; axis < layerShape.size(); ++axis) {
            shape[offset + axis] = qMax(shape.at(offset + axis), layerShape.at(axis));
        }
    }
    return shape;
}

QSize ViewerModel::activeOrVisiblePlaneSize() const
{
    if (activeLayer() != nullptr) {
        return activeLayer()->image().planeSize();
    }

    for (auto it = layers_.crbegin(); it != layers_.crend(); ++it) {
        if ((*it)->visible()) {
            return (*it)->image().planeSize();
        }
    }

    return {};
}

}  // namespace napari_cpp
