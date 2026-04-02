#include "ui/dims_widget.h"

#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QSlider>

#include "core/dims_model.h"

namespace napari_cpp {

DimsWidget::DimsWidget(DimsModel *dims, QWidget *parent)
    : QWidget(parent), dims_(dims), layout_(new QVBoxLayout(this)), emptyLabel_(new QLabel(this))
{
    layout_->setContentsMargins(8, 8, 8, 8);
    layout_->setSpacing(6);
    emptyLabel_->setText(QStringLiteral("No slice sliders for 2D data"));
    rebuild();

    connect(dims_, &DimsModel::shapeChanged, this, &DimsWidget::rebuild);
    connect(dims_, &DimsModel::axisLabelsChanged, this, &DimsWidget::rebuild);
    connect(dims_, &DimsModel::indicesChanged, this, &DimsWidget::syncValues);
}

void DimsWidget::rebuild()
{
    while (QLayoutItem *item = layout_->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    rows_.clear();

    const QVector<int> axes = dims_->nonDisplayedAxes();
    if (axes.isEmpty()) {
        layout_->addWidget(emptyLabel_);
        return;
    }

    const QStringList labels = dims_->axisLabels();
    for (const int axis : axes) {
        RowWidgets row;
        row.axis = axis;
        row.container = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(row.container);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        row.label = new QLabel(labels.value(axis, QStringLiteral("Axis %1").arg(axis)), row.container);
        row.slider = new QSlider(Qt::Horizontal, row.container);
        row.spinBox = new QSpinBox(row.container);

        const int maximum = qMax(0, dims_->axisExtent(axis) - 1);
        row.slider->setRange(0, maximum);
        row.spinBox->setRange(0, maximum);
        row.slider->setValue(dims_->currentIndex(axis));
        row.spinBox->setValue(dims_->currentIndex(axis));

        connect(row.slider, &QSlider::valueChanged, this, [this, axis](const int value) {
            dims_->setCurrentIndex(axis, value);
        });
        connect(row.spinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this, axis](const int value) {
            dims_->setCurrentIndex(axis, value);
        });

        rowLayout->addWidget(row.label);
        rowLayout->addWidget(row.slider, 1);
        rowLayout->addWidget(row.spinBox);
        layout_->addWidget(row.container);
        rows_.push_back(row);
    }
}

void DimsWidget::syncValues()
{
    for (const RowWidgets &row : std::as_const(rows_)) {
        QSignalBlocker sliderBlocker(row.slider);
        QSignalBlocker spinBlocker(row.spinBox);
        row.slider->setValue(dims_->currentIndex(row.axis));
        row.spinBox->setValue(dims_->currentIndex(row.axis));
    }
}

}  // namespace napari_cpp
