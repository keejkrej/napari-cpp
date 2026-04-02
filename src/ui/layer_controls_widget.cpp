#include "ui/layer_controls_widget.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "core/image_layer.h"
#include "core/labels_layer.h"
#include "core/viewer_model.h"

namespace napari_cpp {

LayerControlsWidget::LayerControlsWidget(ViewerModel *viewer, QWidget *parent)
    : QWidget(parent), viewer_(viewer)
{
    buildUi();
    connectUi();
    connect(viewer_, &ViewerModel::activeLayerChanged, this, &LayerControlsWidget::syncFromActiveLayer);
    connect(viewer_, &ViewerModel::layersChanged, this, &LayerControlsWidget::syncFromActiveLayer);
    syncFromActiveLayer();
}

void LayerControlsWidget::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(10);

    auto *commonGroup = new QGroupBox(QStringLiteral("Layer"), this);
    auto *commonLayout = new QFormLayout(commonGroup);
    nameEdit_ = new QLineEdit(commonGroup);
    nameEdit_->setObjectName(QStringLiteral("layerNameEdit"));
    visibleCheckBox_ = new QCheckBox(QStringLiteral("Visible"), commonGroup);
    visibleCheckBox_->setObjectName(QStringLiteral("layerVisibleCheckBox"));
    opacitySlider_ = new QSlider(Qt::Horizontal, commonGroup);
    opacitySlider_->setObjectName(QStringLiteral("layerOpacitySlider"));
    opacitySlider_->setRange(0, 100);
    commonLayout->addRow(QStringLiteral("Name"), nameEdit_);
    commonLayout->addRow(QStringLiteral("Visible"), visibleCheckBox_);
    commonLayout->addRow(QStringLiteral("Opacity"), opacitySlider_);
    rootLayout->addWidget(commonGroup);

    stack_ = new QStackedWidget(this);
    stack_->setObjectName(QStringLiteral("layerControlsStack"));

    emptyPage_ = new QWidget(stack_);
    auto *emptyLayout = new QVBoxLayout(emptyPage_);
    emptyLayout->addWidget(new QLabel(QStringLiteral("No active layer"), emptyPage_));
    emptyLayout->addStretch(1);
    stack_->addWidget(emptyPage_);

    imagePage_ = new QWidget(stack_);
    imagePage_->setObjectName(QStringLiteral("imageControlsPage"));
    auto *imageLayout = new QFormLayout(imagePage_);
    imageColormapCombo_ = new QComboBox(imagePage_);
    imageColormapCombo_->setObjectName(QStringLiteral("imageColormapComboBox"));
    imageColormapCombo_->addItems(
        {QStringLiteral("gray"),
         QStringLiteral("invert"),
         QStringLiteral("red"),
         QStringLiteral("green"),
         QStringLiteral("blue"),
         QStringLiteral("magenta"),
         QStringLiteral("cyan"),
         QStringLiteral("yellow")});
    imageContrastMinSpinBox_ = new QDoubleSpinBox(imagePage_);
    imageContrastMinSpinBox_->setObjectName(QStringLiteral("imageContrastMinSpinBox"));
    imageContrastMinSpinBox_->setDecimals(2);
    imageContrastMinSpinBox_->setRange(-1e9, 1e9);
    imageContrastMaxSpinBox_ = new QDoubleSpinBox(imagePage_);
    imageContrastMaxSpinBox_->setObjectName(QStringLiteral("imageContrastMaxSpinBox"));
    imageContrastMaxSpinBox_->setDecimals(2);
    imageContrastMaxSpinBox_->setRange(-1e9, 1e9);
    imageLayout->addRow(QStringLiteral("Colormap"), imageColormapCombo_);
    imageLayout->addRow(QStringLiteral("Contrast Min"), imageContrastMinSpinBox_);
    imageLayout->addRow(QStringLiteral("Contrast Max"), imageContrastMaxSpinBox_);
    stack_->addWidget(imagePage_);

    labelsPage_ = new QWidget(stack_);
    labelsPage_->setObjectName(QStringLiteral("labelsControlsPage"));
    auto *labelsLayout = new QFormLayout(labelsPage_);
    labelsSelectedSpinBox_ = new QSpinBox(labelsPage_);
    labelsSelectedSpinBox_->setObjectName(QStringLiteral("selectedLabelSpinBox"));
    labelsSelectedSpinBox_->setRange(0, 65535);
    labelsBrushSpinBox_ = new QSpinBox(labelsPage_);
    labelsBrushSpinBox_->setObjectName(QStringLiteral("brushSizeSpinBox"));
    labelsBrushSpinBox_->setRange(1, 256);
    labelsContiguousCheckBox_ = new QCheckBox(QStringLiteral("Contiguous fill"), labelsPage_);
    labelsContiguousCheckBox_->setObjectName(QStringLiteral("contiguousCheckBox"));
    labelsShowSelectedCheckBox_ = new QCheckBox(QStringLiteral("Show selected only"), labelsPage_);
    labelsShowSelectedCheckBox_->setObjectName(QStringLiteral("showSelectedLabelCheckBox"));

    auto *modeRow = new QWidget(labelsPage_);
    auto *modeLayout = new QHBoxLayout(modeRow);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    labelsModeGroup_ = new QButtonGroup(modeRow);
    const QList<QPair<QString, int>> modes = {
        {QStringLiteral("Pan"), static_cast<int>(LabelsLayer::Mode::PanZoom)},
        {QStringLiteral("Paint"), static_cast<int>(LabelsLayer::Mode::Paint)},
        {QStringLiteral("Erase"), static_cast<int>(LabelsLayer::Mode::Erase)},
        {QStringLiteral("Fill"), static_cast<int>(LabelsLayer::Mode::Fill)},
        {QStringLiteral("Pick"), static_cast<int>(LabelsLayer::Mode::Pick)},
    };
    for (const auto &mode : modes) {
        auto *button = new QToolButton(modeRow);
        button->setText(mode.first);
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("%1ModeButton").arg(mode.first.toLower()));
        labelsModeGroup_->addButton(button, mode.second);
        modeLayout->addWidget(button);
    }

    labelsLayout->addRow(QStringLiteral("Selected Label"), labelsSelectedSpinBox_);
    labelsLayout->addRow(QStringLiteral("Brush Size"), labelsBrushSpinBox_);
    labelsLayout->addRow(QStringLiteral("Mode"), modeRow);
    labelsLayout->addRow(QStringLiteral("Contiguous"), labelsContiguousCheckBox_);
    labelsLayout->addRow(QStringLiteral("Selected Only"), labelsShowSelectedCheckBox_);
    stack_->addWidget(labelsPage_);

    rootLayout->addWidget(stack_, 1);
}

void LayerControlsWidget::connectUi()
{
    connect(nameEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (Layer *layer = viewer_->activeLayer(); layer != nullptr) {
            layer->setName(text);
        }
    });
    connect(visibleCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (Layer *layer = viewer_->activeLayer(); layer != nullptr) {
            layer->setVisible(checked);
        }
    });
    connect(opacitySlider_, &QSlider::valueChanged, this, [this](const int value) {
        if (Layer *layer = viewer_->activeLayer(); layer != nullptr) {
            layer->setOpacity(static_cast<double>(value) / 100.0);
        }
    });

    connect(imageColormapCombo_, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        if (ImageLayer *layer = viewer_->activeImageLayer(); layer != nullptr) {
            layer->setColormapName(text);
        }
    });
    connect(imageContrastMinSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](const double value) {
        if (ImageLayer *layer = viewer_->activeImageLayer(); layer != nullptr) {
            layer->setContrastLimits(value, imageContrastMaxSpinBox_->value());
        }
    });
    connect(imageContrastMaxSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](const double value) {
        if (ImageLayer *layer = viewer_->activeImageLayer(); layer != nullptr) {
            layer->setContrastLimits(imageContrastMinSpinBox_->value(), value);
        }
    });

    connect(labelsSelectedSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [this](const int value) {
        if (LabelsLayer *layer = viewer_->activeLabelsLayer(); layer != nullptr) {
            layer->setSelectedLabel(static_cast<quint32>(value));
        }
    });
    connect(labelsBrushSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [this](const int value) {
        if (LabelsLayer *layer = viewer_->activeLabelsLayer(); layer != nullptr) {
            layer->setBrushSize(value);
        }
    });
    connect(labelsContiguousCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (LabelsLayer *layer = viewer_->activeLabelsLayer(); layer != nullptr) {
            layer->setContiguous(checked);
        }
    });
    connect(labelsShowSelectedCheckBox_, &QCheckBox::toggled, this, [this](const bool checked) {
        if (LabelsLayer *layer = viewer_->activeLabelsLayer(); layer != nullptr) {
            layer->setShowSelectedLabel(checked);
        }
    });
    connect(labelsModeGroup_, &QButtonGroup::idClicked, this, [this](const int id) {
        if (LabelsLayer *layer = viewer_->activeLabelsLayer(); layer != nullptr) {
            layer->setMode(static_cast<LabelsLayer::Mode>(id));
        }
    });
}

void LayerControlsWidget::syncFromActiveLayer()
{
    Layer *layer = viewer_->activeLayer();
    connectActiveLayer(layer);
    syncCommonControls(layer);

    if (auto *imageLayer = qobject_cast<ImageLayer *>(layer); imageLayer != nullptr) {
        stack_->setCurrentWidget(imagePage_);
        syncImageControls(imageLayer);
    } else if (auto *labelsLayer = qobject_cast<LabelsLayer *>(layer); labelsLayer != nullptr) {
        stack_->setCurrentWidget(labelsPage_);
        syncLabelsControls(labelsLayer);
    } else {
        stack_->setCurrentWidget(emptyPage_);
    }
}

void LayerControlsWidget::connectActiveLayer(Layer *layer)
{
    if (activeLayerConnection_) {
        disconnect(activeLayerConnection_);
    }
    if (layer != nullptr) {
        activeLayerConnection_ = connect(layer, &Layer::changed, this, &LayerControlsWidget::syncFromActiveLayer);
    }
}

void LayerControlsWidget::syncCommonControls(Layer *layer)
{
    const bool enabled = layer != nullptr;
    nameEdit_->setEnabled(enabled);
    visibleCheckBox_->setEnabled(enabled);
    opacitySlider_->setEnabled(enabled);

    QSignalBlocker nameBlocker(nameEdit_);
    QSignalBlocker visibleBlocker(visibleCheckBox_);
    QSignalBlocker opacityBlocker(opacitySlider_);
    nameEdit_->setText(layer != nullptr ? layer->name() : QString());
    visibleCheckBox_->setChecked(layer != nullptr && layer->visible());
    opacitySlider_->setValue(layer != nullptr ? qRound(layer->opacity() * 100.0) : 100);
}

void LayerControlsWidget::syncImageControls(ImageLayer *layer)
{
    QSignalBlocker comboBlocker(imageColormapCombo_);
    QSignalBlocker minBlocker(imageContrastMinSpinBox_);
    QSignalBlocker maxBlocker(imageContrastMaxSpinBox_);
    imageColormapCombo_->setCurrentText(layer->colormapName());
    imageContrastMinSpinBox_->setValue(layer->contrastLimits().first);
    imageContrastMaxSpinBox_->setValue(layer->contrastLimits().second);
}

void LayerControlsWidget::syncLabelsControls(LabelsLayer *layer)
{
    QSignalBlocker selectedBlocker(labelsSelectedSpinBox_);
    QSignalBlocker brushBlocker(labelsBrushSpinBox_);
    QSignalBlocker contiguousBlocker(labelsContiguousCheckBox_);
    QSignalBlocker showSelectedBlocker(labelsShowSelectedCheckBox_);
    labelsSelectedSpinBox_->setValue(static_cast<int>(layer->selectedLabel()));
    labelsBrushSpinBox_->setValue(layer->brushSize());
    labelsContiguousCheckBox_->setChecked(layer->contiguous());
    labelsShowSelectedCheckBox_->setChecked(layer->showSelectedLabel());

    if (QAbstractButton *button = labelsModeGroup_->button(static_cast<int>(layer->mode())); button != nullptr) {
        QSignalBlocker modeBlocker(labelsModeGroup_);
        button->setChecked(true);
    }
}

}  // namespace napari_cpp
