#include "SubtitleOcrRoiService.h"

namespace LAStudio {

SubtitleOcrRoiService::SubtitleOcrRoiService(QObject *parent)
    : QObject(parent)
{
}

void SubtitleOcrRoiService::setRoi(const QRectF &rect)
{
    if (m_roi != rect) {
        m_roi = rect;
        emit roiChanged(m_roi);
    }
}

void SubtitleOcrRoiService::setRoiCoordinates(double x, double y, double width, double height)
{
    setRoi(QRectF(x, y, width, height));
}

QImage SubtitleOcrRoiService::cropAndPreprocessRoi(const QImage &sourceFrame) const
{
    if (sourceFrame.isNull()) return QImage();

    const int imgW = sourceFrame.width();
    const int imgH = sourceFrame.height();

    const int cropX = qBound(0, static_cast<int>(m_roi.x() * imgW), imgW);
    const int cropY = qBound(0, static_cast<int>(m_roi.y() * imgH), imgH);
    const int cropW = qBound(1, static_cast<int>(m_roi.width() * imgW), imgW - cropX);
    const int cropH = qBound(1, static_cast<int>(m_roi.height() * imgH), imgH - cropY);

    return sourceFrame.copy(cropX, cropY, cropW, cropH);
}

} // namespace LAStudio
