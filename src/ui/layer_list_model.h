#pragma once

#include <QAbstractListModel>

namespace napari_cpp {

class ViewerModel;

class LayerListModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit LayerListModel(ViewerModel *viewer, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    ViewerModel *viewer_ = nullptr;
};

}  // namespace napari_cpp
