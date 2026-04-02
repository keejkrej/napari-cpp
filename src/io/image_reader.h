#pragma once

#include <memory>
#include <vector>

#include <QString>

#include "core/image_layer.h"

namespace napari_cpp {

class ImageReader {
public:
    static std::vector<std::unique_ptr<ImageLayer>> read(const QString &path, QString *errorMessage = nullptr);
};

}  // namespace napari_cpp
