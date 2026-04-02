#include "ui/main_window.h"

#include <QDockWidget>
#include <QFileDialog>
#include <QListView>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include "core/viewer_model.h"
#include "render/canvas_widget.h"
#include "ui/dims_widget.h"
#include "ui/layer_controls_widget.h"
#include "ui/layer_list_model.h"

namespace napari_cpp {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      viewer_(new ViewerModel(this)),
      canvas_(new CanvasWidget(viewer_, this)),
      dimsWidget_(new DimsWidget(viewer_->dimsModel(), this)),
      layerControls_(new LayerControlsWidget(viewer_, this)),
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
    auto *dockSplitter = new QSplitter(Qt::Vertical, layerDock);
    dockSplitter->addWidget(layerView_);
    dockSplitter->addWidget(layerControls_);
    dockSplitter->setStretchFactor(0, 3);
    dockSplitter->setStretchFactor(1, 4);
    layerDock->setWidget(dockSplitter);
    addDockWidget(Qt::RightDockWidgetArea, layerDock);

    createActions();
    createMenus();
    createToolbar();

    statusBar()->showMessage(QStringLiteral("Ready"));

    connect(viewer_, &ViewerModel::statusTextChanged, this, [this](const QString &text) {
        statusBar()->showMessage(text);
    });
    connect(viewer_, &ViewerModel::activeLayerChanged, this, [this](Layer *) { syncSelectionToViewer(); });
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
    openPathsImpl(paths, OpenLayerKind::Image);
}

void MainWindow::openPathsAsLabels(const QStringList &paths)
{
    openPathsImpl(paths, OpenLayerKind::Labels);
}

void MainWindow::openPathsImpl(const QStringList &paths, const OpenLayerKind kind)
{
    bool openedAnything = false;
    for (const QString &path : paths) {
        QString error;
        std::vector<std::unique_ptr<Layer>> layers = ImageReader::read(path, kind, &error);
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
    openAction_ = new QAction(QStringLiteral("Open"), this);
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this,
            QStringLiteral("Open images"),
            QString(),
            QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All Files (*)"));
        if (!paths.isEmpty()) {
            openPaths(paths);
        }
    });

    openLabelsAction_ = new QAction(QStringLiteral("Open as Labels"), this);
    connect(openLabelsAction_, &QAction::triggered, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this,
            QStringLiteral("Open labels"),
            QString(),
            QStringLiteral("Images (*.png *.bmp *.tif *.tiff);;All Files (*)"));
        if (!paths.isEmpty()) {
            openPathsAsLabels(paths);
        }
    });

    newLabelsAction_ = new QAction(QStringLiteral("New Labels"), this);
    connect(newLabelsAction_, &QAction::triggered, this, [this]() {
        viewer_->newLabels();
        canvas_->scheduleAutoFit();
    });

    exportAction_ = new QAction(QStringLiteral("Export Screenshot"), this);
    connect(exportAction_, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save screenshot"),
            QStringLiteral("napari-cpp.png"),
            QStringLiteral("PNG (*.png)"));
        if (!path.isEmpty()) {
            canvas_->saveScreenshot(path);
        }
    });

    quitAction_ = new QAction(QStringLiteral("Quit"), this);
    quitAction_->setShortcut(QKeySequence::Quit);
    connect(quitAction_, &QAction::triggered, this, &QWidget::close);

    fitAction_ = new QAction(QStringLiteral("Fit"), this);
    connect(fitAction_, &QAction::triggered, canvas_, &CanvasWidget::fitToView);

    zoomInAction_ = new QAction(QStringLiteral("Zoom In"), this);
    connect(zoomInAction_, &QAction::triggered, this, [this]() { canvas_->zoomByFactor(1.2); });

    zoomOutAction_ = new QAction(QStringLiteral("Zoom Out"), this);
    connect(zoomOutAction_, &QAction::triggered, this, [this]() { canvas_->zoomByFactor(1.0 / 1.2); });

    removeAction_ = new QAction(QStringLiteral("Remove Layer"), this);
    connect(removeAction_, &QAction::triggered, viewer_, &ViewerModel::removeActiveLayer);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(openAction_);
    fileMenu->addAction(openLabelsAction_);
    fileMenu->addAction(newLabelsAction_);
    fileMenu->addAction(exportAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction_);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(fitAction_);
    viewMenu->addAction(zoomInAction_);
    viewMenu->addAction(zoomOutAction_);

    QMenu *layerMenu = menuBar()->addMenu(QStringLiteral("&Layer"));
    layerMenu->addAction(newLabelsAction_);
    layerMenu->addAction(removeAction_);
}

void MainWindow::createToolbar()
{
    QToolBar *toolbar = addToolBar(QStringLiteral("Main"));
    toolbar->addAction(openAction_);
    toolbar->addAction(openLabelsAction_);
    toolbar->addAction(newLabelsAction_);
    toolbar->addSeparator();
    toolbar->addAction(fitAction_);
    toolbar->addAction(zoomInAction_);
    toolbar->addAction(zoomOutAction_);
    toolbar->addSeparator();
    toolbar->addAction(removeAction_);
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
