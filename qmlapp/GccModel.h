// GccModel.h - the gcc scene's log lines, revealed a few at a time by a QML
// timer. The full log comes from logic.h's gccLog (deterministic); this model
// only decides how much of it is visible yet.
#ifndef BOSSKEY_GCCMODEL_H
#define BOSSKEY_GCCMODEL_H

#include <QAbstractListModel>
#include <QStringList>

namespace bosskey {

class GccModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool finished READ finished NOTIFY finishedChanged)
public:
    explicit GccModel(QObject* parent = nullptr);

    enum Role { LineRole = Qt::UserRole + 1 };

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void resetLog(const QStringList& lines);
    // Reveals up to `count` more lines; returns how many were revealed (0:
    // the log is over).
    Q_INVOKABLE int revealMore(int count = 1);
    bool finished() const;

signals:
    void finishedChanged();

private:
    QStringList m_lines;
    int m_shown = 0;
};

} // namespace bosskey
#endif // BOSSKEY_GCCMODEL_H
