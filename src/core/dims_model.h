#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

namespace napari_cpp {

class DimsModel : public QObject {
    Q_OBJECT

public:
    explicit DimsModel(QObject *parent = nullptr);

    [[nodiscard]] int ndim() const;
    [[nodiscard]] QVector<int> shape() const;
    [[nodiscard]] QStringList axisLabels() const;
    [[nodiscard]] QVector<int> currentIndices() const;
    [[nodiscard]] int currentIndex(int axis) const;
    [[nodiscard]] QVector<int> displayedAxes() const;
    [[nodiscard]] QVector<int> nonDisplayedAxes() const;
    [[nodiscard]] int axisExtent(int axis) const;

    void setShape(const QVector<int> &shape);
    void setAxisLabels(const QStringList &labels);
    void setCurrentIndex(int axis, int index);
    void resetIndices();

signals:
    void shapeChanged();
    void axisLabelsChanged();
    void currentIndexChanged(int axis, int value);
    void indicesChanged();

private:
    QStringList defaultLabels(int ndim) const;
    void clampIndices();

    QVector<int> shape_;
    QStringList axisLabels_;
    QVector<int> currentIndices_;
};

}  // namespace napari_cpp
