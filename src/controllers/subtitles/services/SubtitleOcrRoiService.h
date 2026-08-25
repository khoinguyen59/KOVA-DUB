#pragma once

#include <QObject>
#include <QRectF>
#include <QImage>

namespace LAStudio {

class SubtitleOcrRoiService : public QObject
{
    Q_OBJECT

public:
    explicit SubtitleOcrRoiService(QObject *parent = nullptr);
    ~SubtitleOcrRoiService() override = default;

    QRectF roi() const { return m_roi; }
    void setRoi(const QRectF &rect);
    void setRoiCoordinates(double x, double y, double width, double height);

    QImage cropAndPreprocessRoi(const QImage &sourceFrame) const;

signals:
    void roiChanged(const QRectF &roi);

private:
    QRectF m_roi{0.1, 0.75, 0.8, 0.2}; // Default lower third subtitle region
};

} // namespace LAStudio
