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

QVector<Layer *> ViewerModel::layers() const
{
    return layers_;
}

int ViewerModel::layerCount() const
{
    return layers_.size();
}

Layer *ViewerModel::layerAt(const int index) const
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

Layer *ViewerModel::activeLayer() const
{
    return layerAt(activeLayerIndex_);
}

ImageLayer *ViewerModel::activeImageLayer() const
{
    return qobject_cast<ImageLayer *>(activeLayer());
}

LabelsLayer *ViewerModel::activeLabelsLayer() const
{
    return qobject_cast<LabelsLayer *>(activeLayer());
}

QString ViewerModel::statusText() const
{
    return statusText_;
}

Layer *ViewerModel::addLayer(std::unique_ptr<Layer> layer)
{
    if (!layer) {
        return nullptr;
    }

    layer->setParent(this);
    Layer *rawLayer = layer.release();
    layers_.append(rawLayer);
    connectLayer(rawLayer);
    setActiveLayerIndex(layers_.size() - 1);
    updateDimsFromLayers();
    updateStatusText();
    emit layersChanged();
    emit repaintRequested();
    return rawLayer;
}

void ViewerModel::addLayers(std::vector<std::unique_ptr<Layer>> layers)
{
    for (auto &layer : layers) {
        addLayer(std::move(layer));
    }
}

ImageLayer *ViewerModel::addImage(NdImage image, QString name)
{
    return qobject_cast<ImageLayer *>(addLayer(std::make_unique<ImageLayer>(std::move(image), std::move(name))));
}

LabelsLayer *ViewerModel::addLabels(NdImage image, QString name)
{
    return qobject_cast<LabelsLayer *>(addLayer(std::make_unique<LabelsLayer>(std::move(image), std::move(name))));
}

LabelsLayer *ViewerModel::newLabels()
{
    QSize size(512, 512);
    if (activeImageLayer() != nullptr) {
        size = activeImageLayer()->planeSize();
    } else if (ImageLayer *firstImage = firstImageLayer(); firstImage != nullptr) {
        size = firstImage->planeSize();
    }

    return addLabels(NdImage::zeros({size.height(), size.width()}, DataType::UInt16, QStringLiteral("Labels")));
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

    Layer *layer = layers_.takeAt(index);
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

void ViewerModel::connectLayer(Layer *layer)
{
    connect(layer, &Layer::changed, this, [this, layer]() {
        updateStatusText();
        emit layerDataChanged(layers_.indexOf(layer));
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
        bits << QStringLiteral("active: %1 (%2)").arg(activeLayer()->name(), activeLayer()->kindName());
        if (LabelsLayer *labelsLayer = activeLabelsLayer(); labelsLayer != nullptr) {
            QString modeName = QStringLiteral("pan_zoom");
            switch (labelsLayer->mode()) {
            case LabelsLayer::Mode::PanZoom:
                modeName = QStringLiteral("pan_zoom");
                break;
            case LabelsLayer::Mode::Paint:
                modeName = QStringLiteral("paint");
                break;
            case LabelsLayer::Mode::Erase:
                modeName = QStringLiteral("erase");
                break;
            case LabelsLayer::Mode::Fill:
                modeName = QStringLiteral("fill");
                break;
            case LabelsLayer::Mode::Pick:
                modeName = QStringLiteral("pick");
                break;
            }
            bits << QStringLiteral("mode: %1").arg(modeName);
            bits << QStringLiteral("label=%1").arg(labelsLayer->selectedLabel());
        }
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
    for (const Layer *layer : layers_) {
        maxNdim = qMax(maxNdim, layer->shape().size());
    }

    QVector<int> shape(maxNdim, 1);
    for (const Layer *layer : layers_) {
        const QVector<int> layerShape = layer->shape();
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
        return activeLayer()->planeSize();
    }

    for (auto it = layers_.crbegin(); it != layers_.crend(); ++it) {
        if ((*it)->visible()) {
            return (*it)->planeSize();
        }
    }

    return {};
}

ImageLayer *ViewerModel::firstImageLayer() const
{
    for (Layer *layer : layers_) {
        if (auto *imageLayer = qobject_cast<ImageLayer *>(layer); imageLayer != nullptr) {
            return imageLayer;
        }
    }

    return nullptr;
}

}  // namespace napari_cpp
