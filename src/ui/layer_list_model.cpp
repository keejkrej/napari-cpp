#include "ui/layer_list_model.h"

#include "core/image_layer.h"
#include "core/viewer_model.h"

namespace napari_cpp {

LayerListModel::LayerListModel(ViewerModel *viewer, QObject *parent)
    : QAbstractListModel(parent), viewer_(viewer)
{
    connect(viewer_, &ViewerModel::layersChanged, this, [this]() {
        beginResetModel();
        endResetModel();
    });
}

int LayerListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return viewer_->layerCount();
}

QVariant LayerListModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid()) {
        return {};
    }

    ImageLayer *layer = viewer_->layerAt(index.row());
    if (layer == nullptr) {
        return {};
    }

    if (role == Qt::DisplayRole) {
        return layer->name();
    }
    if (role == Qt::CheckStateRole) {
        return layer->visible() ? Qt::Checked : Qt::Unchecked;
    }
    if (role == Qt::ToolTipRole) {
        const QSize size = layer->image().planeSize();
        return QStringLiteral("%1 x %2").arg(size.width()).arg(size.height());
    }

    return {};
}

bool LayerListModel::setData(const QModelIndex &index, const QVariant &value, const int role)
{
    if (!index.isValid() || role != Qt::CheckStateRole) {
        return false;
    }

    ImageLayer *layer = viewer_->layerAt(index.row());
    if (layer == nullptr) {
        return false;
    }

    layer->setVisible(value.toInt() == Qt::Checked);
    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

Qt::ItemFlags LayerListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
}

}  // namespace napari_cpp
