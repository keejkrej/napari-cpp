#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/camera2d.h"
#include "core/dims_model.h"
#include "core/image_layer.h"
#include "core/labels_layer.h"

namespace napari_cpp {

class ViewerModel : public QObject {
    Q_OBJECT

public:
    explicit ViewerModel(QObject *parent = nullptr);

    [[nodiscard]] DimsModel *dimsModel();
    [[nodiscard]] const DimsModel *dimsModel() const;
    [[nodiscard]] Camera2D *camera();
    [[nodiscard]] const Camera2D *camera() const;
    [[nodiscard]] QVector<Layer *> layers() const;
    [[nodiscard]] int layerCount() const;
    [[nodiscard]] Layer *layerAt(int index) const;
    [[nodiscard]] int activeLayerIndex() const;
    [[nodiscard]] Layer *activeLayer() const;
    [[nodiscard]] ImageLayer *activeImageLayer() const;
    [[nodiscard]] LabelsLayer *activeLabelsLayer() const;
    [[nodiscard]] QString statusText() const;

    Layer *addLayer(std::unique_ptr<Layer> layer);
    void addLayers(std::vector<std::unique_ptr<Layer>> layers);
    ImageLayer *addImage(NdImage image, QString name = {});
    LabelsLayer *addLabels(NdImage image, QString name = {});
    LabelsLayer *newLabels();
    void removeActiveLayer();
    void removeLayer(int index);
    void moveLayer(int from, int to);
    void setActiveLayerIndex(int index);
    void fitToActiveOrVisible(const QSize &canvasSize);

signals:
    void layersChanged();
    void activeLayerChanged(Layer *layer);
    void layerDataChanged(int index);
    void statusTextChanged(const QString &text);
    void repaintRequested();

private:
    void connectLayer(Layer *layer);
    void updateDimsFromLayers();
    void updateStatusText();
    QVector<int> combinedShape() const;
    QSize activeOrVisiblePlaneSize() const;
    ImageLayer *firstImageLayer() const;

    DimsModel dims_;
    Camera2D camera_;
    QVector<Layer *> layers_;
    int activeLayerIndex_ = -1;
    QString statusText_ {QStringLiteral("Ready")};
};

}  // namespace napari_cpp
