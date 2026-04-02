#pragma once

#include <QPoint>

#include "core/layer.h"
#include "core/nd_image.h"

namespace napari_cpp {

class DimsModel;

class LabelsLayer : public Layer {
    Q_OBJECT

public:
    enum class Mode {
        PanZoom,
        Paint,
        Erase,
        Fill,
        Pick,
    };
    Q_ENUM(Mode)

    explicit LabelsLayer(NdImage image, QString name = {}, QObject *parent = nullptr);

    [[nodiscard]] Kind kind() const override;
    [[nodiscard]] QVector<int> shape() const override;
    [[nodiscard]] QSize planeSize() const override;
    [[nodiscard]] const NdImage &image() const;
    [[nodiscard]] NdImage &image();
    [[nodiscard]] quint32 selectedLabel() const;
    [[nodiscard]] int brushSize() const;
    [[nodiscard]] Mode mode() const;
    [[nodiscard]] bool showSelectedLabel() const;
    [[nodiscard]] bool contiguous() const;

    void setSelectedLabel(quint32 label);
    void setBrushSize(int brushSize);
    void setMode(Mode mode);
    void setShowSelectedLabel(bool showSelectedLabel);
    void setContiguous(bool contiguous);

    [[nodiscard]] quint32 pickLabel(const QPoint &point, const DimsModel &dims) const;
    bool pickAndSetLabel(const QPoint &point, const DimsModel &dims);
    bool paint(const QPoint &point, const DimsModel &dims);
    bool erase(const QPoint &point, const DimsModel &dims);
    bool fill(const QPoint &point, const DimsModel &dims);

signals:
    void imageChanged();
    void selectedLabelChanged(quint32 label);
    void brushSizeChanged(int brushSize);
    void modeChanged(Mode mode);
    void showSelectedLabelChanged(bool showSelectedLabel);
    void contiguousChanged(bool contiguous);

private:
    bool sliceInfo(const DimsModel &dims, qsizetype &baseOffset, qsizetype &rowStride, qsizetype &columnStride) const;
    bool pointToScalarIndex(const QPoint &point,
                            const DimsModel &dims,
                            qsizetype &scalarIndex,
                            qsizetype &baseOffset,
                            qsizetype &rowStride,
                            qsizetype &columnStride) const;
    bool applyBrush(const QPoint &point, const DimsModel &dims, quint32 label);
    bool applyFill(const QPoint &point, const DimsModel &dims, quint32 label);
    void emitImageMutation();

    NdImage image_;
    quint32 selectedLabel_ = 1;
    int brushSize_ = 10;
    Mode mode_ = Mode::PanZoom;
    bool showSelectedLabel_ = false;
    bool contiguous_ = true;
};

}  // namespace napari_cpp
