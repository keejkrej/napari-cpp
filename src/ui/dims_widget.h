#pragma once

#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace napari_cpp {

class DimsModel;

class DimsWidget : public QWidget {
    Q_OBJECT

public:
    explicit DimsWidget(DimsModel *dims, QWidget *parent = nullptr);

private:
    struct RowWidgets {
        QWidget *container = nullptr;
        QLabel *label = nullptr;
        QSlider *slider = nullptr;
        QSpinBox *spinBox = nullptr;
        int axis = -1;
    };

    void rebuild();
    void syncValues();

    DimsModel *dims_ = nullptr;
    QVBoxLayout *layout_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QVector<RowWidgets> rows_;
};

}  // namespace napari_cpp
