#pragma once

#include <QImage>

namespace napari_cpp {

class DimsModel;
class ImageLayer;

struct SliceResult {
    QImage image;
};

class SliceExtractor {
public:
    static SliceResult extractRgba(const ImageLayer &layer, const DimsModel &dims);
};

}  // namespace napari_cpp
