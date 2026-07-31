/*
 * IssueItemDelegate.h -- draws one issue row.
 *
 * Two stacked lines with a leading type icon and a trailing priority indicator:
 *
 *     [icon]  Login button does nothing                    (bold, elided)
 *             #007                                         (small, dim)   (o)
 *
 * Group headers and the empty-group placeholders are left to
 * QStyledItemDelegate, so they pick up the style's own look and the model's
 * font/foreground hints.
 */
#ifndef IHAVEISSUES_ISSUE_ITEM_DELEGATE_H
#define IHAVEISSUES_ISSUE_ITEM_DELEGATE_H

#include <QStyledItemDelegate>

namespace ihaveissues
{

class IssueItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit IssueItemDelegate(QObject *parent = nullptr);
    ~IssueItemDelegate() override;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_ITEM_DELEGATE_H
