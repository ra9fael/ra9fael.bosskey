// GccModel.h - the gcc scene's log lines, revealed a few at a time by a QML
// timer. The log is endless: logic.h's gccLog produces one build pass at a time
// and this model appends the next one whenever the visible part catches up, so
// the build streams for as long as the cover is up and never finishes.
#ifndef BOSSKEY_GCCMODEL_H
#define BOSSKEY_GCCMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QStringList>

namespace bosskey {

class GccModel final : public QAbstractListModel {
    Q_OBJECT
public:
    explicit GccModel(QObject* parent = nullptr);

    enum Role { LineRole = Qt::UserRole + 1 };

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Starts a fresh log for `project`; a screenful of lines is visible at once.
    Q_INVOKABLE void resetLog(const QString& project, unsigned seed);
    // Reveals up to `count` more lines, appending another build pass when the
    // visible part catches up. Returns how many were revealed.
    Q_INVOKABLE int revealMore(int count = 1);

private:
    // Appends one build pass, revealing `reveal` of its lines at once (the rest
    // waits for revealMore).
    void appendPass(int reveal);

    QString m_project;
    unsigned m_seed = 1;
    int m_pass = 0;
    QStringList m_lines;
    int m_shown = 0;
};

} // namespace bosskey
#endif // BOSSKEY_GCCMODEL_H
