#pragma once

#include <QImage>

namespace napari_cpp {

class DimsModel;
class ImageLayer;
class LabelsLayer;
class Layer;

struct SliceResult {
    QImage image;
};

class SliceExtractor {
public:
    static SliceResult extractRgba(const Layer &layer, const DimsModel &dims);
};

}  // namespace napari_cpp
