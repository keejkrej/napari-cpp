#pragma once

#include <QListView>
#include <QMainWindow>
#include <QStringList>

namespace napari_cpp {

class CanvasWidget;
class DimsWidget;
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

private:
    void createActions();
    void createMenus();
    void createToolbar();
    void syncSelectionToViewer();

    ViewerModel *viewer_ = nullptr;
    CanvasWidget *canvas_ = nullptr;
    DimsWidget *dimsWidget_ = nullptr;
    LayerListModel *layerModel_ = nullptr;
    QListView *layerView_ = nullptr;
};

}  // namespace napari_cpp
