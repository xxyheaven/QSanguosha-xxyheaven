#ifndef GUHUOBOX
#define GUHUOBOX

#include "graphicsbox.h"
#include "title.h"
#include "qsanbutton.h"

class GuhuoBox : public GraphicsBox
{
    Q_OBJECT

public:
    GuhuoBox(const QString& skill_name, const QString& flags);
    QString getSkillName()const
    {
        return skill_name;
    }

signals:
    void onButtonClick();

public slots:
    void popup();
    void reply();
    void clear();

protected:
    virtual QRectF boundingRect() const;

    bool isButtonEnable(const QString &card) const;

    QString translate(const QString &option) const;

    QString flags;
    QString skill_name;

    QMap<QString, QStringList> card_list;

    QMap<QString, QSanButton *> buttons;

    QMap<QString, Title*> titles;

    static const int defaultButtonWidth;
    static const int defaultButtonHeight;
    static const int topBlankWidth;
    static const int bottomBlankWidth;
    static const int interval;
    static const int outerBlankWidth;
    static const int eachBottomWidth;

    static const int titleWidth;
};

#endif // GUHUOBOX

