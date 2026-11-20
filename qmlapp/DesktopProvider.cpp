// DesktopProvider.cpp - see DesktopProvider.h.
#include "DesktopProvider.h"

namespace bosskey {

namespace {

// One black pixel: the shader must never sample an unbound texture, and a
// black snapshot degrades to a plain shadow instead of garbage.
QImage placeholder()
{
    QImage image(1, 1, QImage::Format_RGB32);
    image.fill(Qt::black);
    return image;
}

} // namespace

DesktopProvider::DesktopProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Image)
{
}

QImage DesktopProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    Q_UNUSED(requestedSize)
    const QStringList parts = id.split('/');
    const int screen = parts.isEmpty() ? -1 : parts.last().toInt();
    QMutexLocker lock(&m_mutex);
    const QImage image = m_snapshots.value(screen, placeholder());
    if (size)
        *size = image.size();
    return image;
}

void DesktopProvider::setSnapshot(int screenIndex, const QImage& image)
{
    if (image.isNull())
        return;
    QMutexLocker lock(&m_mutex);
    m_snapshots.insert(screenIndex, image);
}

} // namespace bosskey
