/*
 * IssueListModel.h -- the list pane's model.
 *
 * A two-level tree: two fixed group rows, "Open" and "Resolved", each holding
 * the issues of that group in DOCUMENT ORDER. An empty group holds exactly one
 * unselectable placeholder row instead.
 *
 * This is a real QAbstractItemModel rather than a pile of widgets in a layout
 * because that is what a QTreeView expects, and because it keeps identity in one
 * place: rows are addressed by uuid through indexForUuid()/uuidAt(), never by
 * array offset. Deleting an issue therefore cannot silently shift the selection
 * onto a different one -- the Apple detail view's issue #3, avoided by
 * construction here.
 *
 * There is deliberately NO search, filter or sort: the Apple app has none, and
 * adding them would be new product, not a port.
 */
#ifndef IHAVEISSUES_ISSUE_LIST_MODEL_H
#define IHAVEISSUES_ISSUE_LIST_MODEL_H

#include <vector>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QString>
#include <QVariant>

#include <issueskit/IssueModel.h>

namespace ihaveissues
{

class IssueListModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Role {
        //! QString, the issue uuid. Empty for group and placeholder rows.
        UuidRole = Qt::UserRole + 1,
        //! QString, the display title ("Untitled" when the title is blank).
        TitleRole,
        //! QString, "#NNN".
        NumberRole,
        //! int, an issueskit::IssueType.
        TypeRole,
        //! int, an issueskit::IssuePriority.
        PriorityRole,
        //! bool, true for the two group header rows.
        IsGroupRole,
        //! bool, true for the "No open issues" style rows.
        IsPlaceholderRole,
        //! bool, true only for rows that stand for a real issue.
        IsIssueRole
    };

    explicit IssueListModel(QObject *parent = nullptr);
    ~IssueListModel() override;

    //! Replaces the contents with \a model's open and resolved issues.
    void setDocument(const issueskit::IssuesDocumentModel &model);

    //! The index of the issue with \a uuid, or an invalid index.
    QModelIndex indexForUuid(const QString &uuid) const;

    //! The uuid at \a index, or an empty string for non-issue rows.
    QString uuidAt(const QModelIndex &index) const;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    enum Group {
        OpenGroup = 0,
        ResolvedGroup = 1,
        GroupCount = 2
    };

    /*! internalId of a group row.
     *
     *  Issue rows carry their parent group's row number (0 or 1) as their
     *  internalId; group rows carry this sentinel, which no group number can
     *  collide with. That is the whole index encoding.
     */
    static constexpr quintptr kGroupRowId = static_cast<quintptr>(-1);

    const std::vector<issueskit::Issue> &groupIssues(int group) const;

    std::vector<issueskit::Issue> m_open;
    std::vector<issueskit::Issue> m_resolved;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_LIST_MODEL_H
