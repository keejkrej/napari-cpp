#pragma once

#include <QAction>
#include <QListView>
#include <QMainWindow>
#include <QStringList>

#include "io/image_reader.h"

namespace napari_cpp {

class CanvasWidget;
class DimsWidget;
class LayerControlsWidget;
class LayerListModel;
class ViewerModel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    ViewerModel *viewerModel() const;
    CanvasWidget *canvasWidget() const;
    QListView *layerListView() const;

public slots:
    void openPaths(const QStringList &paths);
    void openPathsAsLabels(const QStringList &paths);

private:
    void createActions();
    void createMenus();
    void createToolbar();
    void syncSelectionToViewer();
    void openPathsImpl(const QStringList &paths, OpenLayerKind kind);

    QAction *openAction_ = nullptr;
    QAction *openLabelsAction_ = nullptr;
    QAction *newLabelsAction_ = nullptr;
    QAction *exportAction_ = nullptr;
    QAction *quitAction_ = nullptr;
    QAction *fitAction_ = nullptr;
    QAction *zoomInAction_ = nullptr;
    QAction *zoomOutAction_ = nullptr;
    QAction *removeAction_ = nullptr;

    ViewerModel *viewer_ = nullptr;
    CanvasWidget *canvas_ = nullptr;
    DimsWidget *dimsWidget_ = nullptr;
    LayerControlsWidget *layerControls_ = nullptr;
    LayerListModel *layerModel_ = nullptr;
    QListView *layerView_ = nullptr;
};

}  // namespace napari_cpp
