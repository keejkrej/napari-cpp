#include "core/nd_image.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace napari_cpp {

namespace {

template <typename T>
T loadTypedValue(const char *data, qsizetype index)
{
    T value {};
    std::memcpy(&value, data + index * static_cast<qsizetype>(sizeof(T)), sizeof(T));
    return value;
}

double normalizeComponent(double value, DataType type)
{
    switch (type) {
    case DataType::UInt8:
        return std::clamp(value, 0.0, 255.0);
    case DataType::UInt16:
        return std::clamp(value / 65535.0 * 255.0, 0.0, 255.0);
    case DataType::Float32:
        return std::clamp(value, 0.0, 1.0) * 255.0;
    }
    return 0.0;
}

qsizetype product(const QVector<int> &shape)
{
    if (shape.isEmpty()) {
        return 0;
    }

    qsizetype total = 1;
    for (const int extent : shape) {
        total *= static_cast<qsizetype>(extent);
    }
    return total;
}

}  // namespace

NdImage::NdImage(QVector<int> shape, DataType dataType, ChannelMode channelMode, QByteArray bytes, QString name)
    : shape_(std::move(shape)),
      dataType_(dataType),
      channelMode_(channelMode),
      bytes_(std::move(bytes)),
      name_(std::move(name))
{
    if (!isValid()) {
        throw std::runtime_error("Invalid image payload");
    }
}

bool NdImage::isValid() const
{
    if (shape_.size() < 2) {
        return false;
    }

    if (isColor() && shape_.size() != 2) {
        return false;
    }

    for (const int extent : shape_) {
        if (extent <= 0) {
            return false;
        }
    }

    return bytes_.size() == byteCount();
}

int NdImage::ndim() const
{
    return shape_.size();
}

const QVector<int> &NdImage::shape() const
{
    return shape_;
}

DataType NdImage::dataType() const
{
    return dataType_;
}

ChannelMode NdImage::channelMode() const
{
    return channelMode_;
}

const QByteArray &NdImage::rawBytes() const
{
    return bytes_;
}

QByteArray &NdImage::rawBytes()
{
    return bytes_;
}

QString NdImage::name() const
{
    return name_;
}

void NdImage::setName(const QString &name)
{
    name_ = name;
}

bool NdImage::isScalar() const
{
    return channelMode_ == ChannelMode::Scalar;
}

bool NdImage::isColor() const
{
    return channelMode_ == ChannelMode::RGB || channelMode_ == ChannelMode::RGBA;
}

bool NdImage::isInteger() const
{
    return dataType_ == DataType::UInt8 || dataType_ == DataType::UInt16;
}

int NdImage::channelCount() const
{
    switch (channelMode_) {
    case ChannelMode::Scalar:
        return 1;
    case ChannelMode::RGB:
        return 3;
    case ChannelMode::RGBA:
        return 4;
    }
    return 1;
}

int NdImage::bytesPerElement() const
{
    switch (dataType_) {
    case DataType::UInt8:
        return 1;
    case DataType::UInt16:
        return 2;
    case DataType::Float32:
        return 4;
    }
    return 1;
}

qsizetype NdImage::scalarElementCount() const
{
    return product(shape_);
}

qsizetype NdImage::valueCount() const
{
    return scalarElementCount() * static_cast<qsizetype>(channelCount());
}

qsizetype NdImage::byteCount() const
{
    return valueCount() * static_cast<qsizetype>(bytesPerElement());
}

QSize NdImage::planeSize() const
{
    if (shape_.size() < 2) {
        return {};
    }
    return QSize(shape_.constLast(), shape_.at(shape_.size() - 2));
}

QVector<qsizetype> NdImage::scalarStrides() const
{
    QVector<qsizetype> strides(shape_.size(), 1);
    qsizetype current = 1;
    for (int axis = shape_.size() - 1; axis >= 0; --axis) {
        strides[axis] = current;
        current *= static_cast<qsizetype>(shape_.at(axis));
    }
    return strides;
}

double NdImage::scalarValue(qsizetype scalarIndex) const
{
    const char *data = bytes_.constData();
    switch (dataType_) {
    case DataType::UInt8:
        return static_cast<double>(loadTypedValue<quint8>(data, scalarIndex));
    case DataType::UInt16:
        return static_cast<double>(loadTypedValue<quint16>(data, scalarIndex));
    case DataType::Float32:
        return static_cast<double>(loadTypedValue<float>(data, scalarIndex));
    }
    return 0.0;
}

double NdImage::scalarValue(const QVector<int> &indices) const
{
    if (indices.size() != shape_.size()) {
        return 0.0;
    }

    const QVector<qsizetype> strides = scalarStrides();
    qsizetype offset = 0;
    for (int axis = 0; axis < indices.size(); ++axis) {
        offset += strides.at(axis) * static_cast<qsizetype>(indices.at(axis));
    }
    return scalarValue(offset);
}

QPair<double, double> NdImage::scalarMinMax() const
{
    if (!isScalar() || scalarElementCount() == 0) {
        return qMakePair(0.0, 0.0);
    }

    double minimum = std::numeric_limits<double>::max();
    double maximum = std::numeric_limits<double>::lowest();
    for (qsizetype index = 0; index < scalarElementCount(); ++index) {
        const double value = scalarValue(index);
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
    }

    if (minimum == std::numeric_limits<double>::max()) {
        minimum = 0.0;
        maximum = 0.0;
    }

    return qMakePair(minimum, maximum);
}

quint32 NdImage::integerValue(const qsizetype scalarIndex) const
{
    if (!isScalar() || !isInteger()) {
        return 0;
    }

    const char *data = bytes_.constData();
    switch (dataType_) {
    case DataType::UInt8:
        return loadTypedValue<quint8>(data, scalarIndex);
    case DataType::UInt16:
        return loadTypedValue<quint16>(data, scalarIndex);
    case DataType::Float32:
        return 0;
    }

    return 0;
}

bool NdImage::setIntegerValue(const qsizetype scalarIndex, const quint32 value)
{
    if (!isScalar() || !isInteger() || scalarIndex < 0 || scalarIndex >= scalarElementCount()) {
        return false;
    }

    char *data = bytes_.data();
    switch (dataType_) {
    case DataType::UInt8: {
        const quint8 stored = static_cast<quint8>(std::min<quint32>(value, std::numeric_limits<quint8>::max()));
        std::memcpy(data + scalarIndex * sizeof(quint8), &stored, sizeof(quint8));
        return true;
    }
    case DataType::UInt16: {
        const quint16 stored = static_cast<quint16>(std::min<quint32>(value, std::numeric_limits<quint16>::max()));
        std::memcpy(data + scalarIndex * sizeof(quint16), &stored, sizeof(quint16));
        return true;
    }
    case DataType::Float32:
        return false;
    }

    return false;
}

QRgb NdImage::rgbaValue(qsizetype pixelIndex) const
{
    if (!isColor()) {
        return qRgba(0, 0, 0, 255);
    }

    const int components = channelCount();
    const qsizetype valueOffset = pixelIndex * components;

    const double red = normalizeComponent(scalarValue(valueOffset), dataType_);
    const double green = normalizeComponent(scalarValue(valueOffset + 1), dataType_);
    const double blue = normalizeComponent(scalarValue(valueOffset + 2), dataType_);
    const double alpha = (components == 4)
                             ? normalizeComponent(scalarValue(valueOffset + 3), dataType_)
                             : 255.0;

    return qRgba(static_cast<int>(std::round(red)),
                 static_cast<int>(std::round(green)),
                 static_cast<int>(std::round(blue)),
                 static_cast<int>(std::round(alpha)));
}

NdImage NdImage::zeros(QVector<int> shape, const DataType dataType, QString name)
{
    const qsizetype scalarElements = product(shape);
    const int bytesPerElement = (dataType == DataType::UInt8) ? 1 : (dataType == DataType::UInt16 ? 2 : 4);
    return NdImage(std::move(shape),
                   dataType,
                   ChannelMode::Scalar,
                   QByteArray(scalarElements * bytesPerElement, 0),
                   std::move(name));
}

}  // namespace napari_cpp
