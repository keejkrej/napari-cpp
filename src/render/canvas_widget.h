#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPoint>

namespace napari_cpp {

class ViewerModel;

class CanvasWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit CanvasWidget(ViewerModel *viewer, QWidget *parent = nullptr);

    [[nodiscard]] ViewerModel *viewerModel() const;

    void fitToView();
    void scheduleAutoFit();
    void zoomByFactor(double factor);
    bool saveScreenshot(const QString &path);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void ensureShaderProgram();
    void drawLayerTexture(const QImage &image, double opacity);
    void applyPendingAutoFit();

    ViewerModel *viewer_ = nullptr;
    QOpenGLShaderProgram program_;
    bool dragging_ = false;
    QPoint lastDragPosition_;
    bool autoFitPending_ = false;
};

}  // namespace napari_cpp
