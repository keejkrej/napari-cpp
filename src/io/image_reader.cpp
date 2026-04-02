#include "io/image_reader.h"

#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QScopeGuard>
#include <QVector>

#include <cstdint>
#include <cstring>
#include <memory>

#include <tiffio.h>

#include "core/image_layer.h"
#include "core/labels_layer.h"

namespace napari_cpp {

namespace {

QString baseNameForPath(const QString &path)
{
    const QFileInfo info(path);
    return info.completeBaseName().isEmpty() ? info.fileName() : info.completeBaseName();
}

QByteArray tightlyPackedBytes(const QImage &image, const int bytesPerPixel)
{
    QByteArray bytes(image.height() * image.width() * bytesPerPixel, Qt::Uninitialized);
    for (int row = 0; row < image.height(); ++row) {
        std::memcpy(bytes.data() + static_cast<qsizetype>(row) * image.width() * bytesPerPixel,
                    image.constScanLine(row),
                    static_cast<size_t>(image.width()) * bytesPerPixel);
    }
    return bytes;
}

NdImage readStandardImage(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        throw std::runtime_error(reader.errorString().toStdString());
    }

    const QString name = baseNameForPath(path);
    if (image.format() == QImage::Format_Grayscale16) {
        return NdImage({image.height(), image.width()},
                       DataType::UInt16,
                       ChannelMode::Scalar,
                       tightlyPackedBytes(image, 2),
                       name);
    }

    if (image.isGrayscale()) {
        const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
        return NdImage({gray.height(), gray.width()},
                       DataType::UInt8,
                       ChannelMode::Scalar,
                       tightlyPackedBytes(gray, 1),
                       name);
    }

    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    return NdImage({rgba.height(), rgba.width()},
                   DataType::UInt8,
                   ChannelMode::RGBA,
                   tightlyPackedBytes(rgba, 4),
                   name);
}

DataType dataTypeFromTiff(const uint16_t bitDepth, const uint16_t sampleFormat)
{
    if (sampleFormat == SAMPLEFORMAT_IEEEFP && bitDepth == 32) {
        return DataType::Float32;
    }
    if ((sampleFormat == SAMPLEFORMAT_UINT || sampleFormat == SAMPLEFORMAT_VOID || sampleFormat == 0)
        && bitDepth == 8) {
        return DataType::UInt8;
    }
    if ((sampleFormat == SAMPLEFORMAT_UINT || sampleFormat == SAMPLEFORMAT_VOID || sampleFormat == 0)
        && bitDepth == 16) {
        return DataType::UInt16;
    }
    throw std::runtime_error("Unsupported TIFF sample format");
}

NdImage readTiff(const QString &path)
{
    TIFF *tiff = TIFFOpen(path.toUtf8().constData(), "r");
    if (tiff == nullptr) {
        throw std::runtime_error("Unable to open TIFF file");
    }

    auto closeTiff = qScopeGuard([&]() { TIFFClose(tiff); });

    uint16_t samplesPerPixel = 1;
    uint16_t bitDepth = 8;
    uint16_t sampleFormat = SAMPLEFORMAT_UINT;
    uint32_t width = 0;
    uint32_t height = 0;
    TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetFieldDefaulted(tiff, TIFFTAG_BITSPERSAMPLE, &bitDepth);
    TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);

    int directoryCount = 0;
    do {
        ++directoryCount;
    } while (TIFFReadDirectory(tiff) == 1);
    TIFFSetDirectory(tiff, 0);

    if (samplesPerPixel == 1) {
        const DataType dataType = dataTypeFromTiff(bitDepth, sampleFormat);
        const int bytesPerElement = (dataType == DataType::UInt8) ? 1 : (dataType == DataType::UInt16 ? 2 : 4);
        const qsizetype planeBytes = static_cast<qsizetype>(width) * static_cast<qsizetype>(height) * bytesPerElement;
        QByteArray bytes(planeBytes * directoryCount, Qt::Uninitialized);
        QVector<char> scanline(static_cast<int>(TIFFScanlineSize(tiff)));

        for (int directory = 0; directory < directoryCount; ++directory) {
            TIFFSetDirectory(tiff, static_cast<tdir_t>(directory));

            uint32_t localWidth = 0;
            uint32_t localHeight = 0;
            uint16_t localSamples = 1;
            uint16_t localBits = bitDepth;
            uint16_t localSampleFormat = sampleFormat;
            TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &localWidth);
            TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &localHeight);
            TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLESPERPIXEL, &localSamples);
            TIFFGetFieldDefaulted(tiff, TIFFTAG_BITSPERSAMPLE, &localBits);
            TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLEFORMAT, &localSampleFormat);

            if (localWidth != width || localHeight != height || localSamples != 1
                || localBits != bitDepth || localSampleFormat != sampleFormat) {
                throw std::runtime_error("Inconsistent TIFF stack pages");
            }

            for (uint32_t row = 0; row < height; ++row) {
                if (TIFFReadScanline(tiff, scanline.data(), row, 0) != 1) {
                    throw std::runtime_error("Failed to read TIFF scanline");
                }

                std::memcpy(bytes.data() + directory * planeBytes + static_cast<qsizetype>(row) * width * bytesPerElement,
                            scanline.constData(),
                            static_cast<size_t>(width) * bytesPerElement);
            }
        }

        QVector<int> shape;
        if (directoryCount > 1) {
            shape << directoryCount;
        }
        shape << static_cast<int>(height) << static_cast<int>(width);
        return NdImage(shape, dataType, ChannelMode::Scalar, bytes, baseNameForPath(path));
    }

    TIFFSetDirectory(tiff, 0);
    std::unique_ptr<uint32_t[]> raster(new uint32_t[static_cast<size_t>(width) * height]);
    if (TIFFReadRGBAImageOriented(tiff, width, height, raster.get(), ORIENTATION_TOPLEFT, 0) == 0) {
        throw std::runtime_error("Failed to decode TIFF color image");
    }

    QByteArray bytes(static_cast<qsizetype>(width) * height * 4, Qt::Uninitialized);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint32_t pixel = raster[static_cast<size_t>(y) * width + x];
            const qsizetype offset = (static_cast<qsizetype>(y) * width + x) * 4;
            bytes[offset + 0] = static_cast<char>(TIFFGetR(pixel));
            bytes[offset + 1] = static_cast<char>(TIFFGetG(pixel));
            bytes[offset + 2] = static_cast<char>(TIFFGetB(pixel));
            bytes[offset + 3] = static_cast<char>(TIFFGetA(pixel));
        }
    }

    return NdImage({static_cast<int>(height), static_cast<int>(width)},
                   DataType::UInt8,
                   ChannelMode::RGBA,
                   bytes,
                   baseNameForPath(path));
}

std::unique_ptr<Layer> makeLayer(NdImage image, const OpenLayerKind kind)
{
    switch (kind) {
    case OpenLayerKind::Image:
        return std::make_unique<ImageLayer>(std::move(image));
    case OpenLayerKind::Labels:
        if (image.isColor()) {
            throw std::runtime_error("Labels layers must be opened from grayscale integer data");
        }
        if (!image.isInteger()) {
            throw std::runtime_error("Labels layers require integer image data");
        }
        return std::make_unique<LabelsLayer>(std::move(image));
    }

    throw std::runtime_error("Unsupported layer kind");
}

}  // namespace

std::vector<std::unique_ptr<Layer>> ImageReader::read(const QString &path,
                                                      const OpenLayerKind kind,
                                                      QString *errorMessage)
{
    try {
        if (errorMessage != nullptr) {
            errorMessage->clear();
        }
        NdImage image;
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix == QStringLiteral("tif") || suffix == QStringLiteral("tiff")) {
            image = readTiff(path);
        } else {
            image = readStandardImage(path);
        }

        std::vector<std::unique_ptr<Layer>> layers;
        layers.push_back(makeLayer(std::move(image), kind));
        return layers;
    } catch (const std::exception &error) {
        if (errorMessage != nullptr) {
            *errorMessage = QString::fromUtf8(error.what());
        }
        return {};
    }
}

}  // namespace napari_cpp
