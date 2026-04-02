#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QVector>

namespace napari_cpp {

class Layer : public QObject {
    Q_OBJECT

public:
    enum class Kind {
        Image,
        Labels,
    };
    Q_ENUM(Kind)

    explicit Layer(QString name = {}, QObject *parent = nullptr);
    ~Layer() override = default;

    [[nodiscard]] QString name() const;
    [[nodiscard]] bool visible() const;
    [[nodiscard]] double opacity() const;

    void setName(const QString &name);
    void setVisible(bool visible);
    void setOpacity(double opacity);

    [[nodiscard]] virtual Kind kind() const = 0;
    [[nodiscard]] virtual QVector<int> shape() const = 0;
    [[nodiscard]] virtual QSize planeSize() const = 0;
    [[nodiscard]] QString kindName() const;

signals:
    void changed();
    void nameChanged(const QString &name);
    void visibilityChanged(bool visible);
    void opacityChanged(double opacity);

private:
    QString name_;
    bool visible_ = true;
    double opacity_ = 1.0;
};

}  // namespace napari_cpp
