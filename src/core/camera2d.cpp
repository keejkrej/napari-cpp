#include "core/camera2d.h"

#include <QtGlobal>

#include <algorithm>

namespace napari_cpp {

Camera2D::Camera2D(QObject *parent)
    : QObject(parent)
{
}

double Camera2D::zoom() const
{
    return zoom_;
}

QPointF Camera2D::center() const
{
    return center_;
}

QRectF Camera2D::viewRect(const QSize &canvasSize) const
{
    if (canvasSize.width() <= 0 || canvasSize.height() <= 0 || zoom_ <= 0.0) {
        return {};
    }

    const double width = static_cast<double>(canvasSize.width()) / zoom_;
    const double height = static_cast<double>(canvasSize.height()) / zoom_;
    return QRectF(center_.x() - width / 2.0, center_.y() - height / 2.0, width, height);
}

QPointF Camera2D::screenToImage(const QPointF &screenPosition, const QSize &canvasSize) const
{
    const QRectF rect = viewRect(canvasSize);
    return QPointF(rect.left() + screenPosition.x() / zoom_, rect.top() + screenPosition.y() / zoom_);
}

QPointF Camera2D::imageToScreen(const QPointF &imagePosition, const QSize &canvasSize) const
{
    const QRectF rect = viewRect(canvasSize);
    return QPointF((imagePosition.x() - rect.left()) * zoom_, (imagePosition.y() - rect.top()) * zoom_);
}

void Camera2D::reset()
{
    center_ = QPointF(0.0, 0.0);
    zoom_ = 1.0;
    emit changed();
}

void Camera2D::fitToImage(const QSizeF &imageSize, const QSize &canvasSize)
{
    if (imageSize.width() <= 0.0 || imageSize.height() <= 0.0 || canvasSize.width() <= 0 || canvasSize.height() <= 0) {
        reset();
        return;
    }

    center_ = QPointF(imageSize.width() / 2.0, imageSize.height() / 2.0);
    const double xScale = static_cast<double>(canvasSize.width()) / imageSize.width();
    const double yScale = static_cast<double>(canvasSize.height()) / imageSize.height();
    zoom_ = std::max(0.01, std::min(xScale, yScale));
    emit changed();
}

void Camera2D::pan(const QPointF &deltaImage)
{
    center_ += deltaImage;
    emit changed();
}

void Camera2D::zoomBy(const double factor, const QPointF &anchorImage)
{
    if (factor <= 0.0) {
        return;
    }

    const double newZoom = std::max(0.01, zoom_ * factor);
    const double appliedFactor = newZoom / zoom_;
    center_ = anchorImage + (center_ - anchorImage) / appliedFactor;
    zoom_ = newZoom;
    emit changed();
}

}  // namespace napari_cpp
