#include "ui/main_window.h"

#include <QDockWidget>
#include <QFileDialog>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include "core/viewer_model.h"
#include "io/image_reader.h"
#include "render/canvas_widget.h"
#include "ui/dims_widget.h"
#include "ui/layer_list_model.h"

namespace napari_cpp {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      viewer_(new ViewerModel(this)),
      canvas_(new CanvasWidget(viewer_, this)),
      dimsWidget_(new DimsWidget(viewer_->dimsModel(), this)),
      layerModel_(new LayerListModel(viewer_, this)),
      layerView_(new QListView(this))
{
    setWindowTitle(QStringLiteral("napari-cpp"));
    resize(1280, 820);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(canvas_, 1);
    layout->addWidget(dimsWidget_);
    setCentralWidget(central);

    layerView_->setModel(layerModel_);
    layerView_->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *layerDock = new QDockWidget(QStringLiteral("Layers"), this);
    layerDock->setWidget(layerView_);
    addDockWidget(Qt::RightDockWidgetArea, layerDock);

    createActions();
    createMenus();
    createToolbar();

    statusBar()->showMessage(QStringLiteral("Ready"));

    connect(viewer_, &ViewerModel::statusTextChanged, this, [this](const QString &text) {
        statusBar()->showMessage(text);
    });
    connect(viewer_, &ViewerModel::activeLayerChanged, this, [this](ImageLayer *) { syncSelectionToViewer(); });
    connect(viewer_, &ViewerModel::layersChanged, this, [this]() { syncSelectionToViewer(); });
    connect(layerView_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &current) {
        viewer_->setActiveLayerIndex(current.isValid() ? current.row() : -1);
    });
}

ViewerModel *MainWindow::viewerModel() const
{
    return viewer_;
}

CanvasWidget *MainWindow::canvasWidget() const
{
    return canvas_;
}

QListView *MainWindow::layerListView() const
{
    return layerView_;
}

void MainWindow::openPaths(const QStringList &paths)
{
    bool openedAnything = false;
    for (const QString &path : paths) {
        QString error;
        std::vector<std::unique_ptr<ImageLayer>> layers = ImageReader::read(path, &error);
        if (layers.empty()) {
            if (!error.isEmpty()) {
                QMessageBox::warning(this,
                                     QStringLiteral("Open failed"),
                                     QStringLiteral("Failed to open %1\n%2").arg(path, error));
            }
            continue;
        }

        viewer_->addLayers(std::move(layers));
        openedAnything = true;
    }

    if (openedAnything) {
        canvas_->scheduleAutoFit();
    }
}

void MainWindow::createActions()
{
    auto *openAction = new QAction(QStringLiteral("Open"), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this,
            QStringLiteral("Open images"),
            QString(),
            QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*)"));
        if (!paths.isEmpty()) {
            openPaths(paths);
        }
    });
    addAction(openAction);

    auto *exportAction = new QAction(QStringLiteral("Export Screenshot"), this);
    connect(exportAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save screenshot"),
            QStringLiteral("napari-cpp.png"),
            QStringLiteral("PNG (*.png)"));
        if (!path.isEmpty()) {
            canvas_->saveScreenshot(path);
        }
    });
    addAction(exportAction);

    auto *quitAction = new QAction(QStringLiteral("Quit"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    addAction(quitAction);

    auto *fitAction = new QAction(QStringLiteral("Fit"), this);
    connect(fitAction, &QAction::triggered, canvas_, &CanvasWidget::fitToView);
    addAction(fitAction);

    auto *zoomInAction = new QAction(QStringLiteral("Zoom In"), this);
    connect(zoomInAction, &QAction::triggered, this, [this]() { canvas_->zoomByFactor(1.2); });
    addAction(zoomInAction);

    auto *zoomOutAction = new QAction(QStringLiteral("Zoom Out"), this);
    connect(zoomOutAction, &QAction::triggered, this, [this]() { canvas_->zoomByFactor(1.0 / 1.2); });
    addAction(zoomOutAction);

    auto *removeAction = new QAction(QStringLiteral("Remove Layer"), this);
    connect(removeAction, &QAction::triggered, viewer_, &ViewerModel::removeActiveLayer);
    addAction(removeAction);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(actions().at(0));
    fileMenu->addAction(actions().at(1));
    fileMenu->addSeparator();
    fileMenu->addAction(actions().at(2));

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(actions().at(3));
    viewMenu->addAction(actions().at(4));
    viewMenu->addAction(actions().at(5));

    QMenu *layerMenu = menuBar()->addMenu(QStringLiteral("&Layer"));
    layerMenu->addAction(actions().at(6));
}

void MainWindow::createToolbar()
{
    QToolBar *toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->addAction(actions().at(0));
    toolbar->addSeparator();
    toolbar->addAction(actions().at(3));
    toolbar->addAction(actions().at(4));
    toolbar->addAction(actions().at(5));
    toolbar->addSeparator();
    toolbar->addAction(actions().at(6));
}

void MainWindow::syncSelectionToViewer()
{
    if (layerView_->selectionModel() == nullptr) {
        return;
    }

    const int index = viewer_->activeLayerIndex();
    if (index < 0) {
        layerView_->selectionModel()->clearSelection();
        return;
    }

    layerView_->setCurrentIndex(layerModel_->index(index));
}

}  // namespace napari_cpp
