/*
 * IssueListModel.cpp
 */
#include "IssueListModel.h"

#include <QBrush>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>

#include <KLocalizedString>

using issueskit::Issue;
using issueskit::IssuesDocumentModel;

namespace ihaveissues
{

namespace
{

const std::vector<Issue> &emptyIssues()
{
    static const std::vector<Issue> empty;
    return empty;
}

} // unnamed namespace

IssueListModel::IssueListModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

IssueListModel::~IssueListModel() = default;

void IssueListModel::setDocument(const IssuesDocumentModel &model)
{
    beginResetModel();
    m_open = model.OpenIssues();
    m_resolved = model.ResolvedIssues();
    endResetModel();
}

const std::vector<Issue> &IssueListModel::groupIssues(int group) const
{
    if (group == OpenGroup) {
        return m_open;
    }
    if (group == ResolvedGroup) {
        return m_resolved;
    }
    return emptyIssues();
}

QModelIndex IssueListModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0) {
        return QModelIndex();
    }

    if (!parent.isValid()) {
        if (row >= GroupCount) {
            return QModelIndex();
        }
        return createIndex(row, column, kGroupRowId);
    }

    // Issue rows have no children.
    if (parent.internalId() != kGroupRowId) {
        return QModelIndex();
    }

    const int group = parent.row();
    if (group < 0 || group >= GroupCount) {
        return QModelIndex();
    }
    if (row >= rowCount(parent)) {
        return QModelIndex();
    }
    return createIndex(row, column, static_cast<quintptr>(group));
}

QModelIndex IssueListModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }
    const quintptr id = child.internalId();
    if (id == kGroupRowId) {
        return QModelIndex();
    }
    return createIndex(static_cast<int>(id), 0, kGroupRowId);
}

int IssueListModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return GroupCount;
    }
    if (parent.internalId() != kGroupRowId) {
        return 0;
    }
    const std::vector<Issue> &issues = groupIssues(parent.row());
    // An empty group still gets one row: the muted placeholder.
    return issues.empty() ? 1 : static_cast<int>(issues.size());
}

int IssueListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant IssueListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    // Group header row.
    if (index.internalId() == kGroupRowId) {
        switch (role) {
        case Qt::DisplayRole:
            return index.row() == OpenGroup ? i18n("Open") : i18n("Resolved");
        case Qt::FontRole: {
            QFont font = QGuiApplication::font();
            font.setBold(true);
            return font;
        }
        case IsGroupRole:
            return true;
        case IsPlaceholderRole:
        case IsIssueRole:
            return false;
        default:
            return QVariant();
        }
    }

    const int group = static_cast<int>(index.internalId());
    const std::vector<Issue> &issues = groupIssues(group);

    // Placeholder row for an empty group.
    if (issues.empty()) {
        switch (role) {
        case Qt::DisplayRole:
            return group == OpenGroup ? i18n("No open issues") : i18n("No resolved issues");
        case Qt::ForegroundRole:
            return QBrush(QGuiApplication::palette().color(QPalette::Disabled, QPalette::Text));
        case IsPlaceholderRole:
            return true;
        case IsGroupRole:
        case IsIssueRole:
            return false;
        default:
            return QVariant();
        }
    }

    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(issues.size())) {
        return QVariant();
    }
    const Issue &issue = issues[static_cast<size_t>(row)];

    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return issue.title.empty() ? i18n("Untitled") : QString::fromStdString(issue.title);
    case Qt::ToolTipRole:
        return QStringLiteral("%1  %2")
            .arg(QString::fromStdString(issue.DisplayNumber()),
                 issue.title.empty() ? i18n("Untitled") : QString::fromStdString(issue.title));
    case UuidRole:
        return QString::fromStdString(issue.uuid);
    case NumberRole:
        return QString::fromStdString(issue.DisplayNumber());
    case TypeRole:
        return static_cast<int>(issue.type);
    case PriorityRole:
        return static_cast<int>(issue.priority);
    case IsIssueRole:
        return true;
    case IsGroupRole:
    case IsPlaceholderRole:
        return false;
    default:
        return QVariant();
    }
}

Qt::ItemFlags IssueListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    // Groups are expandable but not selectable: selecting one would mean
    // "no issue", which the detail pane already says better.
    if (index.internalId() == kGroupRowId) {
        return Qt::ItemIsEnabled;
    }
    if (groupIssues(static_cast<int>(index.internalId())).empty()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex IssueListModel::indexForUuid(const QString &uuid) const
{
    if (uuid.isEmpty()) {
        return QModelIndex();
    }
    const std::string needle = uuid.toStdString();
    for (int group = 0; group < GroupCount; group++) {
        const std::vector<Issue> &issues = groupIssues(group);
        for (size_t i = 0; i < issues.size(); i++) {
            if (issues[i].uuid == needle) {
                return createIndex(static_cast<int>(i), 0, static_cast<quintptr>(group));
            }
        }
    }
    return QModelIndex();
}

QString IssueListModel::uuidAt(const QModelIndex &index) const
{
    if (!index.isValid() || index.internalId() == kGroupRowId) {
        return QString();
    }
    const std::vector<Issue> &issues = groupIssues(static_cast<int>(index.internalId()));
    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(issues.size())) {
        return QString();
    }
    return QString::fromStdString(issues[static_cast<size_t>(row)].uuid);
}

} // namespace ihaveissues
