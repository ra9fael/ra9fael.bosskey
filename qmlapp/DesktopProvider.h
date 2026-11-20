// DesktopProvider.h - the desktop snapshots the black hole scene lenses.
//
// That scene's cover window is transparent, so the shader has to know what is
// behind it: a snapshot of the rectangle the window covers. QML reaches the
// snapshots through image://desktop/<epoch>/<screen index>; the epoch only
// exists to make the URL change when a new snapshot arrives, so the Image
// reloads instead of keeping the first (empty) one.
#ifndef BOSSKEY_DESKTOPPROVIDER_H
#define BOSSKEY_DESKTOPPROVIDER_H

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

namespace bosskey {

class DesktopProvider final : public QQuickImageProvider {
public:
    DesktopProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

    // Stores the snapshot for a screen (index as QGuiApplication::screens() has it).
    void setSnapshot(int screenIndex, const QImage& image);

private:
    QMutex m_mutex;
    QHash<int, QImage> m_snapshots;
};

} // namespace bosskey
#endif // BOSSKEY_DESKTOPPROVIDER_H
