#pragma once

#include <QPair>
#include <QString>

#include "core/layer.h"
#include "core/nd_image.h"

namespace napari_cpp {

class ImageLayer : public Layer {
    Q_OBJECT

public:
    explicit ImageLayer(NdImage image, QString name = {}, QObject *parent = nullptr);

    [[nodiscard]] Kind kind() const override;
    [[nodiscard]] QVector<int> shape() const override;
    [[nodiscard]] QSize planeSize() const override;
    [[nodiscard]] const NdImage &image() const;
    [[nodiscard]] NdImage &image();
    [[nodiscard]] QPair<double, double> contrastLimits() const;
    [[nodiscard]] QString colormapName() const;

    void setContrastLimits(double lower, double upper);
    void setColormapName(const QString &name);

signals:
    void imageChanged();
    void contrastLimitsChanged(double lower, double upper);
    void colormapNameChanged(const QString &name);

private:
    NdImage image_;
    QPair<double, double> contrastLimits_ {0.0, 255.0};
    QString colormapName_ {QStringLiteral("gray")};
};

}  // namespace napari_cpp
