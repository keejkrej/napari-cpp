#include <QApplication>
#include <cstring>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>

#include <tiffio.h>

#include "core/dims_model.h"
#include "core/image_layer.h"
#include "core/labels_layer.h"
#include "core/layer.h"
#include "core/slice_extractor.h"
#include "core/viewer_model.h"
#include "io/image_reader.h"
#include "render/canvas_widget.h"
#include "ui/main_window.h"

using namespace napari_cpp;

namespace {

QByteArray scalarBytesU8(const QVector<int> &shape)
{
    qsizetype count = 1;
    for (const int extent : shape) {
        count *= extent;
    }
    QByteArray bytes(count, Qt::Uninitialized);
    for (qsizetype i = 0; i < count; ++i) {
        bytes[i] = static_cast<char>(i % 255);
    }
    return bytes;
}

QByteArray scalarBytesU16(const QVector<quint16> &values)
{
    QByteArray bytes(static_cast<qsizetype>(values.size() * sizeof(quint16)), Qt::Uninitialized);
    std::memcpy(bytes.data(), values.constData(), static_cast<size_t>(bytes.size()));
    return bytes;
}

QString writePng(const QTemporaryDir &directory)
{
    QImage image(12, 10, QImage::Format_Grayscale8);
    image.fill(0);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixel(x, y, qRgb(x * 10, x * 10, x * 10));
        }
    }
    const QString path = directory.path() + QStringLiteral("/test.png");
    image.save(path);
    return path;
}

QString writeColorPng(const QTemporaryDir &directory)
{
    QImage image(8, 6, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y, QColor(255, x * 20, y * 20, 255));
        }
    }
    const QString path = directory.path() + QStringLiteral("/color.png");
    image.save(path);
    return path;
}

QString writeTiff(const QTemporaryDir &directory, const int pages)
{
    const QString path = directory.path() + QStringLiteral("/stack.tiff");
    TIFF *tiff = TIFFOpen(path.toUtf8().constData(), "w");
    if (tiff == nullptr) {
        return {};
    }

    for (int page = 0; page < pages; ++page) {
        TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, 6);
        TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, 4);
        TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

        QByteArray row(6, Qt::Uninitialized);
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 6; ++x) {
                row[x] = static_cast<char>(page * 20 + y * 6 + x);
            }
            TIFFWriteScanline(tiff, row.data(), static_cast<uint32_t>(y), 0);
        }
        TIFFWriteDirectory(tiff);
    }

    TIFFClose(tiff);
    return path;
}

class DimsModelTests : public QObject {
    Q_OBJECT

private slots:
    void clampsIndicesAndTracksDisplayedAxes()
    {
        DimsModel dims;
        dims.setShape({5, 9, 32, 64});
        QCOMPARE(dims.ndim(), 4);
        QCOMPARE(dims.displayedAxes(), QVector<int>({2, 3}));
        QCOMPARE(dims.nonDisplayedAxes(), QVector<int>({0, 1}));

        dims.setCurrentIndex(0, 99);
        dims.setCurrentIndex(1, 3);
        QCOMPARE(dims.currentIndex(0), 4);
        QCOMPARE(dims.currentIndex(1), 3);
    }
};

class SliceExtractorTests : public QObject {
    Q_OBJECT

private slots:
    void slicesScalarImageCorrectly()
    {
        ImageLayer layer(NdImage({2, 3, 4}, DataType::UInt8, ChannelMode::Scalar, scalarBytesU8({2, 3, 4}), QStringLiteral("stack")));
        layer.setContrastLimits(0, 23);
        layer.setColormapName(QStringLiteral("green"));

        DimsModel dims;
        dims.setShape({2, 3, 4});
        dims.setCurrentIndex(0, 1);

        const SliceResult result = SliceExtractor::extractRgba(layer, dims);
        QCOMPARE(result.image.size(), QSize(4, 3));
        const QColor firstPixel(result.image.pixel(0, 0));
        QVERIFY(firstPixel.green() > 120);
    }

    void rendersLabelsWithTransparentBackground()
    {
        LabelsLayer layer(NdImage({2, 2}, DataType::UInt16, ChannelMode::Scalar, scalarBytesU16({0, 1, 2, 1}), QStringLiteral("labels")));
        DimsModel dims;
        dims.setShape({2, 2});

        SliceResult result = SliceExtractor::extractRgba(layer, dims);
        QCOMPARE(result.image.pixelColor(0, 0).alpha(), 0);
        QVERIFY(result.image.pixelColor(1, 0).alpha() > 0);

        layer.setSelectedLabel(2);
        layer.setShowSelectedLabel(true);
        result = SliceExtractor::extractRgba(layer, dims);
        QCOMPARE(result.image.pixelColor(1, 0).alpha(), 0);
        QVERIFY(result.image.pixelColor(0, 1).alpha() > 0);
    }
};

class LabelsLayerTests : public QObject {
    Q_OBJECT

private slots:
    void paintEraseFillAndPickOperateOnCurrentSlice()
    {
        LabelsLayer layer(NdImage::zeros({2, 10, 10}, DataType::UInt16, QStringLiteral("labels")));
        DimsModel dims;
        dims.setShape({2, 10, 10});
        dims.setCurrentIndex(0, 1);

        layer.setBrushSize(1);
        layer.setSelectedLabel(7);
        QVERIFY(layer.paint(QPoint(5, 5), dims));
        QCOMPARE(layer.pickLabel(QPoint(5, 5), dims), 7u);

        dims.setCurrentIndex(0, 0);
        QCOMPARE(layer.pickLabel(QPoint(5, 5), dims), 0u);

        dims.setCurrentIndex(0, 1);
        QVERIFY(layer.erase(QPoint(5, 5), dims));
        QCOMPARE(layer.pickLabel(QPoint(5, 5), dims), 0u);

        layer.setSelectedLabel(3);
        layer.setContiguous(true);
        QVERIFY(layer.paint(QPoint(1, 1), dims));
        QVERIFY(layer.paint(QPoint(1, 2), dims));
        QVERIFY(layer.paint(QPoint(8, 8), dims));
        layer.setSelectedLabel(9);
        QVERIFY(layer.fill(QPoint(1, 1), dims));
        QCOMPARE(layer.pickLabel(QPoint(1, 1), dims), 9u);
        QCOMPARE(layer.pickLabel(QPoint(1, 2), dims), 9u);
        QCOMPARE(layer.pickLabel(QPoint(8, 8), dims), 3u);

        layer.setContiguous(false);
        QVERIFY(layer.fill(QPoint(8, 8), dims));
        QCOMPARE(layer.pickLabel(QPoint(8, 8), dims), 9u);
    }
};

class ViewerModelTests : public QObject {
    Q_OBJECT

private slots:
    void mixedLayersUpdateDimsAndSelection()
    {
        ViewerModel viewer;
        auto *image = viewer.addImage(NdImage({8, 9}, DataType::UInt8, ChannelMode::Scalar, scalarBytesU8({8, 9}), QStringLiteral("image")));
        auto *labels = viewer.addLabels(NdImage::zeros({4, 8, 9}, DataType::UInt16, QStringLiteral("labels")));

        QCOMPARE(viewer.layerCount(), 2);
        QCOMPARE(viewer.dimsModel()->shape(), QVector<int>({4, 8, 9}));
        QCOMPARE(viewer.activeLayer(), static_cast<Layer *>(labels));
        viewer.setActiveLayerIndex(0);
        QCOMPARE(viewer.activeLayer(), static_cast<Layer *>(image));
    }

    void newLabelsUsesActiveImageSizeAndFits()
    {
        ViewerModel viewer;
        viewer.addImage(NdImage({100, 200}, DataType::UInt8, ChannelMode::Scalar, scalarBytesU8({100, 200}), QStringLiteral("fit")));
        LabelsLayer *labels = viewer.newLabels();
        QCOMPARE(labels->shape(), QVector<int>({100, 200}));

        viewer.setActiveLayerIndex(1);
        viewer.fitToActiveOrVisible(QSize(400, 200));
        QVERIFY(qAbs(viewer.camera()->zoom() - 2.0) < 0.0001);
    }
};

class ImageReaderTests : public QObject {
    Q_OBJECT

private slots:
    void readsImagesAndLabelsExplicitly()
    {
        QTemporaryDir directory;
        const QString path = writePng(directory);
        QString error;

        auto imageLayers = ImageReader::read(path, OpenLayerKind::Image, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(static_cast<int>(imageLayers.size()), 1);
        QCOMPARE(imageLayers.front()->kind(), Layer::Kind::Image);

        auto labelLayers = ImageReader::read(path, OpenLayerKind::Labels, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(static_cast<int>(labelLayers.size()), 1);
        QCOMPARE(labelLayers.front()->kind(), Layer::Kind::Labels);
    }

    void rejectsColorImagesAsLabelsAndReadsTiffStacks()
    {
        QTemporaryDir directory;
        QString error;

        auto rejected = ImageReader::read(writeColorPng(directory), OpenLayerKind::Labels, &error);
        QVERIFY(rejected.empty());
        QVERIFY(!error.isEmpty());

        auto stack = ImageReader::read(writeTiff(directory, 3), OpenLayerKind::Labels, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(static_cast<int>(stack.size()), 1);
        QCOMPARE(stack.front()->kind(), Layer::Kind::Labels);
        QCOMPARE(stack.front()->shape(), QVector<int>({3, 4, 6}));
    }
};

class MainWindowTests : public QObject {
    Q_OBJECT

private slots:
    void opensFilesSwapsControlsAndPaintsLabels()
    {
        QTemporaryDir directory;
        const QString imagePath = writePng(directory);

        MainWindow window;
        window.show();
        QTest::qWait(100);

        window.openPaths({imagePath});
        QCOMPARE(window.viewerModel()->layerCount(), 1);
        QCOMPARE(window.viewerModel()->activeLayer()->kind(), Layer::Kind::Image);
        auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("layerControlsStack"));
        QVERIFY(stack != nullptr);
        QCOMPARE(stack->currentWidget()->objectName(), QStringLiteral("imageControlsPage"));

        window.viewerModel()->newLabels();
        window.canvasWidget()->fitToView();
        QTRY_COMPARE(window.viewerModel()->activeLayer()->kind(), Layer::Kind::Labels);
        QCOMPARE(stack->currentWidget()->objectName(), QStringLiteral("labelsControlsPage"));

        auto *labels = window.viewerModel()->activeLabelsLayer();
        QVERIFY(labels != nullptr);
        labels->setBrushSize(1);
        labels->setSelectedLabel(7);
        labels->setMode(LabelsLayer::Mode::Paint);

        const QPoint imageCenter(labels->planeSize().width() / 2, labels->planeSize().height() / 2);
        const QPointF mappedCenter = window.viewerModel()->camera()->imageToScreen(QPointF(imageCenter), window.canvasWidget()->size());
        const QPoint center(qRound(mappedCenter.x()), qRound(mappedCenter.y()));
        QTest::mousePress(window.canvasWidget(), Qt::LeftButton, Qt::NoModifier, center);
        QTest::mouseRelease(window.canvasWidget(), Qt::LeftButton, Qt::NoModifier, center);
        QTRY_COMPARE(labels->pickLabel(imageCenter, *window.viewerModel()->dimsModel()), 7u);

        labels->setSelectedLabel(1);
        labels->setMode(LabelsLayer::Mode::Pick);
        QTest::mouseClick(window.canvasWidget(), Qt::LeftButton, Qt::NoModifier, center);
        QTRY_COMPARE(labels->selectedLabel(), 7u);

        const QString screenshotPath = directory.path() + QStringLiteral("/shot.png");
        QVERIFY(window.canvasWidget()->saveScreenshot(screenshotPath));
        QVERIFY(QFileInfo::exists(screenshotPath));
        QVERIFY(QFileInfo(screenshotPath).size() > 0);
    }

    void opensPathsAsLabelsExplicitly()
    {
        QTemporaryDir directory;
        MainWindow window;
        window.show();
        QTest::qWait(50);

        window.openPathsAsLabels({writePng(directory)});
        QCOMPARE(window.viewerModel()->layerCount(), 1);
        QCOMPARE(window.viewerModel()->activeLayer()->kind(), Layer::Kind::Labels);
    }
};

}  // namespace

int runAllTests(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    int status = 0;
    DimsModelTests dimsModelTests;
    status |= QTest::qExec(&dimsModelTests, argc, argv);
    SliceExtractorTests sliceExtractorTests;
    status |= QTest::qExec(&sliceExtractorTests, argc, argv);
    LabelsLayerTests labelsLayerTests;
    status |= QTest::qExec(&labelsLayerTests, argc, argv);
    ViewerModelTests viewerModelTests;
    status |= QTest::qExec(&viewerModelTests, argc, argv);
    ImageReaderTests imageReaderTests;
    status |= QTest::qExec(&imageReaderTests, argc, argv);
    MainWindowTests mainWindowTests;
    status |= QTest::qExec(&mainWindowTests, argc, argv);
    return status;
}

#include "test_suite.moc"
