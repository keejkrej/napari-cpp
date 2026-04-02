#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/camera2d.h"
#include "core/dims_model.h"
#include "core/image_layer.h"

namespace napari_cpp {

class ViewerModel : public QObject {
    Q_OBJECT

public:
    explicit ViewerModel(QObject *parent = nullptr);

    [[nodiscard]] DimsModel *dimsModel();
    [[nodiscard]] const DimsModel *dimsModel() const;
    [[nodiscard]] Camera2D *camera();
    [[nodiscard]] const Camera2D *camera() const;
    [[nodiscard]] QVector<ImageLayer *> layers() const;
    [[nodiscard]] int layerCount() const;
    [[nodiscard]] ImageLayer *layerAt(int index) const;
    [[nodiscard]] int activeLayerIndex() const;
    [[nodiscard]] ImageLayer *activeLayer() const;
    [[nodiscard]] QString statusText() const;

    ImageLayer *addLayer(std::unique_ptr<ImageLayer> layer);
    void addLayers(std::vector<std::unique_ptr<ImageLayer>> layers);
    void removeActiveLayer();
    void removeLayer(int index);
    void moveLayer(int from, int to);
    void setActiveLayerIndex(int index);
    void fitToActiveOrVisible(const QSize &canvasSize);

signals:
    void layersChanged();
    void activeLayerChanged(ImageLayer *layer);
    void statusTextChanged(const QString &text);
    void repaintRequested();

private:
    void connectLayer(ImageLayer *layer);
    void updateDimsFromLayers();
    void updateStatusText();
    QVector<int> combinedShape() const;
    QSize activeOrVisiblePlaneSize() const;

    DimsModel dims_;
    Camera2D camera_;
    QVector<ImageLayer *> layers_;
    int activeLayerIndex_ = -1;
    QString statusText_ {QStringLiteral("Ready")};
};

}  // namespace napari_cpp
