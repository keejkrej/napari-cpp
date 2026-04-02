#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <tiffio.h>

#include "core/dims_model.h"
#include "core/image_layer.h"
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

    void preservesTrailingIndicesAcrossShapeChanges()
    {
        DimsModel dims;
        dims.setShape({4, 8, 16});
        dims.setCurrentIndex(0, 2);
        dims.setShape({10, 4, 8, 16});
        QCOMPARE(dims.currentIndex(1), 2);
        QCOMPARE(dims.currentIndex(0), 0);
    }
};

class SliceExtractorTests : public QObject {
    Q_OBJECT

private slots:
    void slicesScalarStackCorrectly()
    {
        NdImage image({2, 3, 4}, DataType::UInt8, ChannelMode::Scalar, scalarBytesU8({2, 3, 4}), QStringLiteral("stack"));
        ImageLayer layer(image);
        layer.setContrastLimits(0, 23);

        DimsModel dims;
        dims.setShape({2, 3, 4});
        dims.setCurrentIndex(0, 1);

        const SliceResult result = SliceExtractor::extractRgba(layer, dims);
        QCOMPARE(result.image.size(), QSize(4, 3));
        const QColor firstPixel(result.image.pixel(0, 0));
        QVERIFY(firstPixel.red() > 120);
    }

    void passesThroughColorImages()
    {
        QByteArray bytes(2 * 2 * 4, Qt::Uninitialized);
        bytes[0] = static_cast<char>(255);
        bytes[1] = 0;
        bytes[2] = 0;
        bytes[3] = static_cast<char>(255);
        bytes[4] = 0;
        bytes[5] = static_cast<char>(255);
        bytes[6] = 0;
        bytes[7] = static_cast<char>(255);
        bytes[8] = 0;
        bytes[9] = 0;
        bytes[10] = static_cast<char>(255);
        bytes[11] = static_cast<char>(255);
        bytes[12] = static_cast<char>(255);
        bytes[13] = static_cast<char>(255);
        bytes[14] = 0;
        bytes[15] = static_cast<char>(255);
        ImageLayer layer(NdImage({2, 2}, DataType::UInt8, ChannelMode::RGBA, bytes, QStringLiteral("rgba")));

        DimsModel dims;
        dims.setShape({2, 2});
        const SliceResult result = SliceExtractor::extractRgba(layer, dims);
        QCOMPARE(QColor(result.image.pixel(0, 0)), QColor(255, 0, 0, 255));
        QCOMPARE(QColor(result.image.pixel(1, 0)), QColor(0, 255, 0, 255));
    }
};

class ViewerModelTests : public QObject {
    Q_OBJECT

private slots:
    void appendRemoveMoveLayersAndEmitRepaint()
    {
        ViewerModel viewer;
        QSignalSpy repaintSpy(&viewer, &ViewerModel::repaintRequested);

        auto first = std::make_unique<ImageLayer>(
            NdImage({8, 9}, DataType::UInt8, ChannelMode::Scalar, scalarBytesU8({8, 9}), QStringLiteral("first")));
        auto second = std::make_unique<ImageLayer>(
            NdImage({4, 8, 9}, DataType::UInt8, ChannelMode::Scalar, scalarBytesU8({4, 8, 9}), QStringLiteral("second")));

        viewer.addLayer(std::move(first));
        viewer.addLayer(std::move(second));
        QCOMPARE(viewer.layerCount(), 2);
        QCOMPARE(viewer.dimsModel()->shape(), QVector<int>({4, 8, 9}));

        viewer.moveLayer(1, 0);
        QCOMPARE(viewer.layerAt(0)->name(), QStringLiteral("second"));

        viewer.layerAt(0)->setOpacity(0.5);
        QVERIFY(repaintSpy.count() > 0);

        viewer.removeActiveLayer();
        QCOMPARE(viewer.layerCount(), 1);
    }

    void fitsToVisibleLayer()
    {
        ViewerModel viewer;
        auto layer = std::make_unique<ImageLayer>(
            NdImage({100, 200}, DataType::UInt8, ChannelMode::Scalar, scalarBytesU8({100, 200}), QStringLiteral("fit")));
        viewer.addLayer(std::move(layer));
        viewer.fitToActiveOrVisible(QSize(400, 200));
        QVERIFY(qAbs(viewer.camera()->zoom() - 2.0) < 0.0001);
    }
};

class ImageReaderTests : public QObject {
    Q_OBJECT

private slots:
    void readsPngIntoSingleLayer()
    {
        QTemporaryDir directory;
        const QString path = writePng(directory);
        QString error;
        auto layers = ImageReader::read(path, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(static_cast<int>(layers.size()), 1);
        QCOMPARE(layers.front()->image().shape(), QVector<int>({10, 12}));
    }

    void readsSinglePageAndStackedTiff()
    {
        QTemporaryDir directory;
        QString error;

        auto onePage = ImageReader::read(writeTiff(directory, 1), &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(onePage.front()->image().shape(), QVector<int>({4, 6}));

        auto stack = ImageReader::read(writeTiff(directory, 3), &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(stack.front()->image().shape(), QVector<int>({3, 4, 6}));
    }
};

class MainWindowTests : public QObject {
    Q_OBJECT

private slots:
    void opensFilesAndExportsScreenshot()
    {
        QTemporaryDir directory;
        const QString imagePath = writePng(directory);

        MainWindow window;
        window.show();
        QTest::qWait(100);

        window.openPaths({imagePath});
        QCOMPARE(window.viewerModel()->layerCount(), 1);
        QVERIFY(window.layerListView()->model()->rowCount() == 1);
        QTRY_VERIFY(window.viewerModel()->camera()->zoom() > 1.0);

        const QString screenshotPath = directory.path() + QStringLiteral("/shot.png");
        QVERIFY(window.canvasWidget()->saveScreenshot(screenshotPath));
        QVERIFY(QFileInfo::exists(screenshotPath));
        QVERIFY(QFileInfo(screenshotPath).size() > 0);
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
    ViewerModelTests viewerModelTests;
    status |= QTest::qExec(&viewerModelTests, argc, argv);
    ImageReaderTests imageReaderTests;
    status |= QTest::qExec(&imageReaderTests, argc, argv);
    MainWindowTests mainWindowTests;
    status |= QTest::qExec(&mainWindowTests, argc, argv);
    return status;
}

#include "test_suite.moc"
