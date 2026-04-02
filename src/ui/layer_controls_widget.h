#pragma once

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QMetaObject>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QWidget>

namespace napari_cpp {

class ImageLayer;
class LabelsLayer;
class Layer;
class ViewerModel;

class LayerControlsWidget : public QWidget {
    Q_OBJECT

public:
    explicit LayerControlsWidget(ViewerModel *viewer, QWidget *parent = nullptr);

private:
    void buildUi();
    void connectUi();
    void syncFromActiveLayer();
    void connectActiveLayer(Layer *layer);
    void syncCommonControls(Layer *layer);
    void syncImageControls(ImageLayer *layer);
    void syncLabelsControls(LabelsLayer *layer);

    ViewerModel *viewer_ = nullptr;
    QMetaObject::Connection activeLayerConnection_;

    QLineEdit *nameEdit_ = nullptr;
    QCheckBox *visibleCheckBox_ = nullptr;
    QSlider *opacitySlider_ = nullptr;

    QStackedWidget *stack_ = nullptr;
    QWidget *emptyPage_ = nullptr;
    QWidget *imagePage_ = nullptr;
    QWidget *labelsPage_ = nullptr;

    QComboBox *imageColormapCombo_ = nullptr;
    QDoubleSpinBox *imageContrastMinSpinBox_ = nullptr;
    QDoubleSpinBox *imageContrastMaxSpinBox_ = nullptr;

    QSpinBox *labelsSelectedSpinBox_ = nullptr;
    QSpinBox *labelsBrushSpinBox_ = nullptr;
    QCheckBox *labelsContiguousCheckBox_ = nullptr;
    QCheckBox *labelsShowSelectedCheckBox_ = nullptr;
    QButtonGroup *labelsModeGroup_ = nullptr;
};

}  // namespace napari_cpp
