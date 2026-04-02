#pragma once

#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSize>

namespace napari_cpp {

class Camera2D : public QObject {
    Q_OBJECT

public:
    explicit Camera2D(QObject *parent = nullptr);

    [[nodiscard]] double zoom() const;
    [[nodiscard]] QPointF center() const;
    [[nodiscard]] QRectF viewRect(const QSize &canvasSize) const;
    [[nodiscard]] QPointF screenToImage(const QPointF &screenPosition, const QSize &canvasSize) const;
    [[nodiscard]] QPointF imageToScreen(const QPointF &imagePosition, const QSize &canvasSize) const;

    void reset();
    void fitToImage(const QSizeF &imageSize, const QSize &canvasSize);
    void pan(const QPointF &deltaImage);
    void zoomBy(double factor, const QPointF &anchorImage);

signals:
    void changed();

private:
    QPointF center_ {0.0, 0.0};
    double zoom_ = 1.0;
};

}  // namespace napari_cpp
