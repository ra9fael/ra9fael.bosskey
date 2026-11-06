// GccModel.cpp - see GccModel.h.
#include "GccModel.h"

namespace bosskey {

GccModel::GccModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int GccModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_shown;
}

QVariant GccModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_shown || index.column() != 0)
        return {};
    if (role == LineRole)
        return m_lines.at(index.row());
    return {};
}

QHash<int, QByteArray> GccModel::roleNames() const
{
    return {{LineRole, "line"}};
}

void GccModel::resetLog(const QStringList& lines)
{
    beginResetModel();
    m_lines = lines;
    m_shown = 0;
    endResetModel();
    emit finishedChanged();
}

int GccModel::revealMore(int count)
{
    if (count <= 0 || m_shown >= m_lines.size())
        return 0;
    const int add = qMin(count, static_cast<int>(m_lines.size()) - m_shown);
    beginInsertRows({}, m_shown, m_shown + add - 1);
    m_shown += add;
    endInsertRows();
    if (m_shown == m_lines.size())
        emit finishedChanged();
    return add;
}

bool GccModel::finished() const
{
    return m_shown >= m_lines.size();
}

} // namespace bosskey
