#include "render/canvas_widget.h"

#include <QMouseEvent>
#include <QOpenGLTexture>
#include <QTimer>
#include <QWheelEvent>

#include "core/slice_extractor.h"
#include "core/viewer_model.h"

namespace napari_cpp {

namespace {

constexpr auto kVertexShader = R"(
attribute vec2 a_position;
attribute vec2 a_texCoord;
uniform vec2 u_viewOrigin;
uniform vec2 u_viewSize;
varying vec2 v_texCoord;

void main() {
    vec2 normalized = (a_position - u_viewOrigin) / u_viewSize;
    vec2 ndc = vec2(normalized.x * 2.0 - 1.0, 1.0 - normalized.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_texCoord = a_texCoord;
}
)";

constexpr auto kFragmentShader = R"(
uniform sampler2D u_texture;
uniform float u_opacity;
varying vec2 v_texCoord;

void main() {
    vec4 color = texture2D(u_texture, v_texCoord);
    gl_FragColor = vec4(color.rgb, color.a * u_opacity);
}
)";

}  // namespace

CanvasWidget::CanvasWidget(ViewerModel *viewer, QWidget *parent)
    : QOpenGLWidget(parent), viewer_(viewer)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    connect(viewer_, &ViewerModel::repaintRequested, this, QOverload<>::of(&CanvasWidget::update));
}

ViewerModel *CanvasWidget::viewerModel() const
{
    return viewer_;
}

void CanvasWidget::fitToView()
{
    autoFitPending_ = false;
    viewer_->fitToActiveOrVisible(size());
}

void CanvasWidget::scheduleAutoFit()
{
    autoFitPending_ = true;
    QTimer::singleShot(0, this, [this]() { applyPendingAutoFit(); });
}

void CanvasWidget::zoomByFactor(const double factor)
{
    const QPointF anchor = viewer_->camera()->screenToImage(QPointF(width() / 2.0, height() / 2.0), size());
    viewer_->camera()->zoomBy(factor, anchor);
}

bool CanvasWidget::saveScreenshot(const QString &path)
{
    const QImage framebuffer = grabFramebuffer();
    if (!framebuffer.isNull()) {
        return framebuffer.save(path);
    }

    const QPixmap fallback = grab();
    if (!fallback.isNull()) {
        return fallback.toImage().save(path);
    }

    return false;
}

void CanvasWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ensureShaderProgram();
}

void CanvasWidget::resizeGL(int, int)
{
    applyPendingAutoFit();
}

void CanvasWidget::paintGL()
{
    const qreal devicePixelRatio = this->devicePixelRatioF();
    glViewport(0,
               0,
               static_cast<GLsizei>(width() * devicePixelRatio),
               static_cast<GLsizei>(height() * devicePixelRatio));
    glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (viewer_->layerCount() == 0) {
        return;
    }

    ensureShaderProgram();
    for (ImageLayer *layer : viewer_->layers()) {
        if (!layer->visible()) {
            continue;
        }
        const SliceResult slice = SliceExtractor::extractRgba(*layer, *viewer_->dimsModel());
        if (slice.image.isNull()) {
            continue;
        }
        drawLayerTexture(slice.image, layer->opacity());
    }
}

void CanvasWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        lastDragPosition_ = event->pos();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging_) {
        const QPoint delta = event->pos() - lastDragPosition_;
        lastDragPosition_ = event->pos();
        viewer_->camera()->pan(QPointF(-delta.x() / viewer_->camera()->zoom(),
                                       -delta.y() / viewer_->camera()->zoom()));
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void CanvasWidget::wheelEvent(QWheelEvent *event)
{
    const QPointF anchor = viewer_->camera()->screenToImage(event->position(), size());
    const double factor = event->angleDelta().y() >= 0 ? 1.15 : (1.0 / 1.15);
    viewer_->camera()->zoomBy(factor, anchor);
    QOpenGLWidget::wheelEvent(event);
}

void CanvasWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        fitToView();
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void CanvasWidget::ensureShaderProgram()
{
    if (program_.isLinked()) {
        return;
    }

    program_.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    program_.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    program_.link();
}

void CanvasWidget::applyPendingAutoFit()
{
    if (!autoFitPending_ || viewer_->layerCount() == 0 || width() <= 0 || height() <= 0) {
        return;
    }
    fitToView();
}

void CanvasWidget::drawLayerTexture(const QImage &image, const double opacity)
{
    const QImage glImage = image.convertToFormat(QImage::Format_RGBA8888).flipped(Qt::Vertical);
    QOpenGLTexture texture(glImage);
    texture.setMinificationFilter(QOpenGLTexture::Linear);
    texture.setMagnificationFilter(QOpenGLTexture::Linear);
    texture.setWrapMode(QOpenGLTexture::ClampToEdge);

    const QRectF viewRect = viewer_->camera()->viewRect(size());
    const float vertices[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        static_cast<float>(image.width()), 0.0f, 1.0f, 0.0f,
        0.0f, static_cast<float>(image.height()), 0.0f, 1.0f,
        static_cast<float>(image.width()), static_cast<float>(image.height()), 1.0f, 1.0f,
    };

    program_.bind();
    texture.bind(0);
    program_.setUniformValue("u_texture", 0);
    program_.setUniformValue("u_viewOrigin", QVector2D(viewRect.left(), viewRect.top()));
    program_.setUniformValue("u_viewSize",
                             QVector2D(static_cast<float>(viewRect.width()), static_cast<float>(viewRect.height())));
    program_.setUniformValue("u_opacity", static_cast<float>(opacity));
    program_.enableAttributeArray("a_position");
    program_.enableAttributeArray("a_texCoord");
    program_.setAttributeArray("a_position", GL_FLOAT, vertices, 2, 4 * sizeof(float));
    program_.setAttributeArray("a_texCoord", GL_FLOAT, vertices + 2, 2, 4 * sizeof(float));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    program_.disableAttributeArray("a_position");
    program_.disableAttributeArray("a_texCoord");
    texture.release();
    program_.release();
}

}  // namespace napari_cpp
