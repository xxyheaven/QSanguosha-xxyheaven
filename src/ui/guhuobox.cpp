#include "guhuobox.h"
#include "button.h"
#include "engine.h"
#include "standard.h"
#include "clientplayer.h"
#include "skin-bank.h"
#include "roomscene.h"
#include "button.h"

#include <QPropertyAnimation>
#include <QGraphicsSceneMouseEvent>

const int GuhuoBox::defaultButtonWidth = 94;
const int GuhuoBox::defaultButtonHeight = 25;
const int GuhuoBox::topBlankWidth = 30; //42
const int GuhuoBox::bottomBlankWidth = 55; //85
const int GuhuoBox::interval = 10; //15
const int GuhuoBox::outerBlankWidth = 25; //37
const int GuhuoBox::eachBottomWidth = 10; //85
const int GuhuoBox::titleWidth = 15; // 20

GuhuoBox::GuhuoBox(const QString &skillname, const QString &flag)
{
    this->skill_name = skillname;
    this->flags = flag;
    title = QString("%1 %2").arg(Sanguosha->translate(skill_name)).arg(tr("Please choose:"));;
    QList<int> modecard = Sanguosha->getRandomCards();
    //collect Cards' objectNames
    if (flags.contains("b")) {
        QList<const BasicCard*> basics = Sanguosha->findChildren<const BasicCard*>();
        foreach (const BasicCard *card, basics) {
            if (!card_list["BasicCard"].contains(card->objectName()) && modecard.contains(card->getId())
                && !(flags.contains("s") && card_list["BasicCard"].contains("slash") && card->objectName().contains("slash")))
                card_list["BasicCard"].append(card->objectName());
        }
    }
    if (flags.contains("t")) {
        QList<const TrickCard*> tricks = Sanguosha->findChildren<const TrickCard*>();
        foreach (const TrickCard *card, tricks) {
            if (modecard.contains(card->getId()) && card->isNDTrick()) {
                if (card_list["SingleTargetTrick"].contains(card->objectName()) || card_list["MultiTargetTrick"].contains(card->objectName()))
                    continue;
                if (card->inherits("SingleTargetTrick") && !card_list["SingleTargetTrick"].contains(card->objectName()))
                    card_list["SingleTargetTrick"].append(card->objectName());
                else
                    card_list["MultiTargetTrick"].append(card->objectName());

            }
        }
    }
    if (flags.contains("d")) {
        QList<const DelayedTrick*> delays = Sanguosha->findChildren<const DelayedTrick*>();
        foreach (const DelayedTrick *card, delays) {
            if (!card_list["DelayedTrick"].contains(card->objectName())
                && modecard.contains(card->getId()))
                card_list["DelayedTrick"].append(card->objectName());
        }
    }
}

QRectF GuhuoBox::boundingRect() const
{
    const int width = defaultButtonWidth * 4 + outerBlankWidth * 2 + eachBottomWidth * 3; // 4 buttons each line

    int height = topBlankWidth
        + ((card_list["BasicCard"].length() + 3) / 4) * defaultButtonHeight
        + (((card_list["BasicCard"].length() + 3) / 4) - 1) * interval
        + ((card_list["SingleTargetTrick"].length() + 3) / 4) * defaultButtonHeight
        + (((card_list["SingleTargetTrick"].length() + 3) / 4) - 1) * interval
        + ((card_list["MultiTargetTrick"].length() + 3) / 4) * defaultButtonHeight
        + (((card_list["MultiTargetTrick"].length() + 3) / 4) - 1) * interval
        + ((card_list["DelayedTrick"].length() + 3) / 4) * defaultButtonHeight
        + (((card_list["DelayedTrick"].length() + 3) / 4) - 1) * interval
        + card_list.keys().length()*titleWidth * 2 //add some titles......
        + bottomBlankWidth;

    return QRectF(0, 0, width, height);
}

bool GuhuoBox::isButtonEnable(const QString &card_name) const
{
    const Skill *skill = Sanguosha->getSkill(skill_name);
    if (skill == NULL)
        return false;
    return skill->buttonEnabled(card_name);
}

void GuhuoBox::popup()
{
    //if (play_only && Sanguosha->currentRoomState()->getCurrentCardUseReason() != CardUseStruct::CARD_USE_REASON_PLAY) {
        //emit onButtonClick();
        //return;
    //}
    //RoomSceneInstance->getDasboard()->disableAllCards();
    RoomSceneInstance->setOkButton(false);
    RoomSceneInstance->current_guhuo_box = this;
    foreach (const QString &key, card_list.keys()) {
        foreach (const QString &card_name, card_list.value(key)) {
            //Button *button = new Button(translate(card_name), QSizeF(buttonWidth,
              //  defaultButtonHeight));
            QSanButton *button = new QSanButton("guhuo", card_name, this);
            buttons[card_name] = button;

            button->setEnabled(isButtonEnable(card_name));

            QString original_tooltip = QString(":%1").arg(title);
            QString tooltip = Sanguosha->translate(original_tooltip);
            if (tooltip == original_tooltip) {
                original_tooltip = QString(":%1").arg(card_name);
                tooltip = Sanguosha->translate(original_tooltip);
            }
            connect(button, &QSanButton::clicked, this, &GuhuoBox::reply);
            if (tooltip != original_tooltip)
                button->setToolTip(tooltip);
        }

        titles[key] = new Title(this, translate(key), IQSanComponentSkin::QSanSimpleTextFont::_m_fontBank.key(G_COMMON_LAYOUT.graphicsBoxTitleFont.m_fontFace), Config.TinyFont.pixelSize()); //undefined reference to "GuhuoBox::titleWidth" 666666
        titles[key]->setParentItem(this);
    }
    moveToCenter();
    show();
    int x = 0;
    int y = 0;
    int titles_num = 0;
    foreach (const QString &key, card_list.keys()) {
        QPointF titlepos;
        titlepos.setX(interval);
        titlepos.setY(topBlankWidth + defaultButtonHeight*y + interval*(y - 1) + titleWidth*titles_num * 2 - 2 * titles[key]->y());
        titles[key]->setPos(titlepos);
        ++titles_num;
        foreach (const QString &card_name, card_list.value(key)) {
            QPointF apos;
            apos.setX(outerBlankWidth + x*(defaultButtonWidth + eachBottomWidth));
            apos.setY(topBlankWidth + defaultButtonHeight*y + interval*(y - 1) + titleWidth*titles_num * 2);
            ++x;
            if (x == 4) {
                ++y;
                x = 0;
            }
            buttons[card_name]->setPos(apos);
        }
        if (x > 0)
            ++y;
        x = 0;
    }
}

void GuhuoBox::reply()
{
    Self->tag.remove(skill_name);
    QSanButton *button = qobject_cast<QSanButton *>(sender());
    if (button) {
        Self->tag[skill_name] = button->getButtonName();
        emit onButtonClick();
    }
    clear();
}

void GuhuoBox::clear()
{
    RoomSceneInstance->current_guhuo_box = NULL;

    if (!isVisible())
        return;

    foreach(QSanButton *button, buttons.values())
        button->deleteLater();

    buttons.values().clear();

    foreach (Title *title, titles.values())
        title->deleteLater();

    titles.values().clear();

    disappear();
}

QString GuhuoBox::translate(const QString &option) const
{
    QString title = QString("%1:%2").arg(skill_name).arg(option);
    QString translated = Sanguosha->translate(title);
    if (translated == title)
        translated = Sanguosha->translate(option);
    return translated;
}
