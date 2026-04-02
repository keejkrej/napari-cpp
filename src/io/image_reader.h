#pragma once

#include <memory>
#include <vector>

#include <QString>

namespace napari_cpp {

class Layer;

enum class OpenLayerKind {
    Image,
    Labels,
};

class ImageReader {
public:
    static std::vector<std::unique_ptr<Layer>> read(const QString &path,
                                                    OpenLayerKind kind = OpenLayerKind::Image,
                                                    QString *errorMessage = nullptr);
};

}  // namespace napari_cpp
