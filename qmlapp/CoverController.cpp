// CoverController.cpp - see CoverController.h.
#include "CoverController.h"

#include "BossQml.h"
#include "../logic.h"
#include "../qrcode.h"

#include <QDateTime>

namespace bosskey {

CoverController::CoverController(QObject* parent)
    : QObject(parent)
    , m_gcc(new GccModel(this))
{
    // The blue screen's QR code is a real one, encoded once: the URL printed
    // right below it, so a scan lands on the same page.
    const qr::Code code = qr::encode("https://www.windows.com/stopcode");
    if (code.ok)
        m_qrRows = QString::fromStdString(code.rows).split(u'\n', Qt::SkipEmptyParts);

    m_percentTimer.setInterval(250);
    connect(&m_percentTimer, &QTimer::timeout, this, &CoverController::tick);
    m_autoHideTimer.setSingleShot(true);
    connect(&m_autoHideTimer, &QTimer::timeout, this, &CoverController::exitRequested);
}

void CoverController::applySpec(const CoverSpec& spec)
{
    const QString scene = QString::fromStdString(spec.scene);
    const QString exit = QString::fromStdString(spec.exitSequence);
    const QString project = QString::fromStdString(spec.gccProject);
    if (scene != m_scene) {
        m_scene = scene;
        emit sceneChanged();
    }
    if (exit != m_exitSequence) {
        m_exitSequence = exit;
        emit exitSequenceChanged();
    }
    if (project != m_project) {
        m_project = project;
        emit projectChanged();
    }
    if (spec.zh != m_zh) {
        m_zh = spec.zh;
        emit zhChanged();
    }
    m_autoHideSec = spec.autoHideSec;

    m_buffer.clear();
    m_percent = 0;
    emit percentChanged();

    if (m_scene == QLatin1String(kSceneGcc))
        m_gcc->resetLog(m_project,
                        static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch() & 0x7fffffffu) | 1u);
}

void CoverController::startCover()
{
    m_elapsed.start();
    m_shown = true;
    emit shownChanged();
    m_percentTimer.start();
    if (m_autoHideSec > 0)
        m_autoHideTimer.start(m_autoHideSec * 1000);
    else
        m_autoHideTimer.stop();
}

void CoverController::stopCover()
{
    m_percentTimer.stop();
    m_autoHideTimer.stop();
    if (m_shown) {
        m_shown = false;
        emit shownChanged();
    }
}

void CoverController::bumpDesktopEpoch()
{
    ++m_desktopEpoch;
    emit desktopEpochChanged();
}

bool CoverController::keyPressed(const QString& text, int key)
{
    (void)key; // the sequence is letters and digits only; their e.text carries them
    if (!m_shown)
        return false;
    const std::string sequence = m_exitSequence.toStdString();
    for (const QChar ch : text) {
        if (!ch.isLetterOrNumber())
            continue;
        char c = ch.toLower().toLatin1();
        if (c == '\0')
            continue;
        auto [next, hit] = pushKey(std::move(m_buffer), sequence, c);
        m_buffer = std::move(next);
        if (hit) {
            m_buffer.clear();
            emit exitRequested();
            return true;
        }
    }
    return false;
}

void CoverController::tick()
{
    const int ms = static_cast<int>(m_elapsed.elapsed());
    const int percent = coverPercent(ms, m_scene.toStdString());
    if (percent != m_percent) {
        m_percent = percent;
        emit percentChanged();
    }
}

} // namespace bosskey
