#include "core/image_layer.h"

#include <algorithm>

namespace napari_cpp {

ImageLayer::ImageLayer(NdImage image, QString name, QObject *parent)
    : Layer(name.isEmpty() ? image.name() : name, parent), image_(std::move(image))
{
    if (this->name().isEmpty()) {
        setName(QStringLiteral("Image"));
    }

    if (image_.isScalar()) {
        contrastLimits_ = image_.scalarMinMax();
    }
}

Layer::Kind ImageLayer::kind() const
{
    return Kind::Image;
}

const NdImage &ImageLayer::image() const
{
    return image_;
}

NdImage &ImageLayer::image()
{
    return image_;
}

QPair<double, double> ImageLayer::contrastLimits() const
{
    return contrastLimits_;
}

QString ImageLayer::colormapName() const
{
    return colormapName_;
}

void ImageLayer::setContrastLimits(double lower, double upper)
{
    if (upper < lower) {
        std::swap(lower, upper);
    }

    if (contrastLimits_ == qMakePair(lower, upper)) {
        return;
    }

    contrastLimits_ = qMakePair(lower, upper);
    emit contrastLimitsChanged(lower, upper);
    emit changed();
}

void ImageLayer::setColormapName(const QString &name)
{
    if (colormapName_ == name) {
        return;
    }

    colormapName_ = name;
    emit colormapNameChanged(colormapName_);
    emit changed();
}

}  // namespace napari_cpp
