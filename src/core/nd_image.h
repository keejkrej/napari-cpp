#pragma once

#include <QByteArray>
#include <QColor>
#include <QPair>
#include <QSize>
#include <QString>
#include <QVector>

namespace napari_cpp {

enum class DataType {
    UInt8,
    UInt16,
    Float32,
};

enum class ChannelMode {
    Scalar,
    RGB,
    RGBA,
};

class NdImage {
public:
    NdImage() = default;
    NdImage(QVector<int> shape, DataType dataType, ChannelMode channelMode, QByteArray bytes, QString name = {});

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] int ndim() const;
    [[nodiscard]] const QVector<int> &shape() const;
    [[nodiscard]] DataType dataType() const;
    [[nodiscard]] ChannelMode channelMode() const;
    [[nodiscard]] const QByteArray &rawBytes() const;
    [[nodiscard]] QByteArray &rawBytes();
    [[nodiscard]] QString name() const;
    void setName(const QString &name);

    [[nodiscard]] bool isScalar() const;
    [[nodiscard]] bool isColor() const;
    [[nodiscard]] bool isInteger() const;
    [[nodiscard]] int channelCount() const;
    [[nodiscard]] int bytesPerElement() const;
    [[nodiscard]] qsizetype scalarElementCount() const;
    [[nodiscard]] qsizetype valueCount() const;
    [[nodiscard]] qsizetype byteCount() const;
    [[nodiscard]] QSize planeSize() const;
    [[nodiscard]] QVector<qsizetype> scalarStrides() const;
    [[nodiscard]] double scalarValue(qsizetype scalarIndex) const;
    [[nodiscard]] double scalarValue(const QVector<int> &indices) const;
    [[nodiscard]] QPair<double, double> scalarMinMax() const;
    [[nodiscard]] quint32 integerValue(qsizetype scalarIndex) const;
    bool setIntegerValue(qsizetype scalarIndex, quint32 value);
    [[nodiscard]] QRgb rgbaValue(qsizetype pixelIndex) const;

    static NdImage zeros(QVector<int> shape, DataType dataType, QString name = {});

private:
    QVector<int> shape_;
    DataType dataType_ = DataType::UInt8;
    ChannelMode channelMode_ = ChannelMode::Scalar;
    QByteArray bytes_;
    QString name_;
};

}  // namespace napari_cpp
