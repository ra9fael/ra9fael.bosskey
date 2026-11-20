// GccModel.cpp - see GccModel.h.
#include "GccModel.h"

#include "../logic.h"

namespace bosskey {

// Lines visible the moment the cover appears: the screen is already full of
// build output, and only then does it start streaming.
constexpr int kFirstScreen = 40;

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

void GccModel::resetLog(const QString& project, unsigned seed)
{
    beginResetModel();
    m_project = project;
    m_seed = seed ? seed : 1u;
    m_pass = 0;
    m_lines.clear();
    m_shown = 0;
    endResetModel();

    appendPass(kFirstScreen);
}

int GccModel::revealMore(int count)
{
    if (count <= 0)
        return 0;
    if (m_shown >= m_lines.size())
        appendPass(0); // the visible part caught up: the build keeps going
    const int add = qMin(count, static_cast<int>(m_lines.size()) - m_shown);
    if (add <= 0)
        return 0;
    beginInsertRows({}, m_shown, m_shown + add - 1);
    m_shown += add;
    endInsertRows();
    return add;
}

void GccModel::appendPass(int reveal)
{
    const std::vector<std::string> lines = gccLog(m_project.toStdString(), m_seed, ++m_pass);
    if (lines.empty())
        return;
    // One insert per batch of newly visible rows: the view must never be told
    // about rows rowCount() does not report yet.
    const int first = static_cast<int>(m_lines.size());
    const int shown = qMin(reveal, static_cast<int>(lines.size()));
    beginInsertRows({}, first, first + shown - 1);
    for (const std::string& line : lines)
        m_lines.push_back(QString::fromStdString(line));
    m_shown += shown;
    endInsertRows();
}

} // namespace bosskey
