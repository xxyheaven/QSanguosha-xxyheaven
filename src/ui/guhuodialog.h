#ifndef GUHUODIALOG_H
#define GUHUODIALOG_H

#include "package.h"
#include "card.h"

#include <QDialog>
#include <QAbstractButton>
#include <QGroupBox>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QCommandLinkButton>

class GuhuoDialog : public QDialog
{
    Q_OBJECT

public:
    static GuhuoDialog *getInstance(const QString &object, bool left = true, bool right = true,
        bool play_only = true, bool slash_combined = false, bool delayed_tricks = false);

public slots:
    void popup();
    void selectCard(QAbstractButton *button);

protected:
    explicit GuhuoDialog(const QString &object, bool left = true, bool right = true,
        bool play_only = true, bool slash_combined = false, bool delayed_tricks = false);
    virtual bool isButtonEnabled(const QString &button_name) const;
    QAbstractButton *createButton(const Card *card);

    QHash<QString, const Card *> map;

private:
    QGroupBox *createLeft();
    QGroupBox *createRight();
    QButtonGroup *group;

    QString object_name;
    bool play_only; // whether the dialog will pop only during the Play phase
    bool slash_combined; // create one 'Slash' button instead of 'Slash', 'Fire Slash', 'Thunder Slash'
    bool delayed_tricks; // whether buttons of Delayed Tricks will be created

signals:
    void onButtonClick();
};

#endif // GUHUODIALOG_H
