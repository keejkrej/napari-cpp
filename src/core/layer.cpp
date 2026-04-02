#include "core/layer.h"

#include <QtGlobal>

namespace napari_cpp {

Layer::Layer(QString name, QObject *parent)
    : QObject(parent), name_(std::move(name))
{
}

QString Layer::name() const
{
    return name_;
}

bool Layer::visible() const
{
    return visible_;
}

double Layer::opacity() const
{
    return opacity_;
}

void Layer::setName(const QString &name)
{
    if (name_ == name) {
        return;
    }

    name_ = name;
    emit nameChanged(name_);
    emit changed();
}

void Layer::setVisible(const bool visible)
{
    if (visible_ == visible) {
        return;
    }

    visible_ = visible;
    emit visibilityChanged(visible_);
    emit changed();
}

void Layer::setOpacity(const double opacity)
{
    const double clamped = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(opacity_, clamped)) {
        return;
    }

    opacity_ = clamped;
    emit opacityChanged(opacity_);
    emit changed();
}

QString Layer::kindName() const
{
    switch (kind()) {
    case Kind::Image:
        return QStringLiteral("Image");
    case Kind::Labels:
        return QStringLiteral("Labels");
    }

    return QStringLiteral("Layer");
}

}  // namespace napari_cpp
