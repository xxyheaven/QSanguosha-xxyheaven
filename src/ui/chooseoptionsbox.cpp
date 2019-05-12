/********************************************************************
    Copyright (c) 2013-2015 - Mogara

    This file is part of QSanguosha-Hegemony.

    This game is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3.0
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    See the LICENSE file for more details.

    Mogara
    *********************************************************************/

#include "chooseoptionsbox.h"
#include "button.h"
#include "engine.h"
#include "qsanbutton.h"
#include "client.h"
#include "clientstruct.h"
#include "roomscene.h"
#include "stylehelper.h"

#include <QGraphicsProxyWidget>

ChooseOptionsBox::ChooseOptionsBox()
{
}

QRectF ChooseOptionsBox::boundingRect() const
{
    int n = options.length();
    int allbuttonswidth = 0;
    foreach (const QString &card_name, options) {
        int buttonwidth = getButtonWidth(card_name);
        allbuttonswidth += buttonwidth;
    }
    return QRectF(0, 0, (allbuttonswidth + (n+1)*interval), defaultButtonHeight);
}

void ChooseOptionsBox::chooseOption(const QStringList &options, const QStringList &all_options)
{
    //repaint background
    this->options = all_options;
    prepareGeometryChange();

    foreach (const QString &choice, all_options) {
        QSanButton *button = new QSanButton(this, getButtonWidth(choice), translate(choice));
        button->setObjectName(choice);
        button->setEnabled(options.contains(choice));
        buttons << button;

        QString original_tooltip = QString(":%1").arg(title);
        QString tooltip = Sanguosha->translate(original_tooltip);
        if (tooltip == original_tooltip) {
            original_tooltip = QString(":%1").arg(choice);
            tooltip = Sanguosha->translate(original_tooltip);
        }
        connect(button, &QSanButton::clicked, this, &ChooseOptionsBox::reply);
        if (tooltip != original_tooltip)
            button->setToolTip(tooltip);
    }

    const QRectF rect = boundingRect();
    setPos(RoomSceneInstance->tableCenterPos().x() - rect.width() / 2, RoomSceneInstance->tableCenterPos().y()*2 - 230);

    setFlag(QGraphicsItem::ItemIsMovable, false);
    show();
    int x = interval;

    //foreach (const QString &card_name, all_options) {
    for (int i = 0; i < buttons.length(); ++i) {
        QSanButton *button = buttons.at(i);
        QPointF apos;
        apos.setX(x);
        x += (interval + getButtonWidth(button->objectName()));
        apos.setY(0);
        button->setPos(apos);
    }
}

void ChooseOptionsBox::reply()
{
    QString choice = sender()->objectName();
    if (choice.isEmpty())
        choice = options.first();
    ClientInstance->onPlayerMakeChoice(choice);
}

int ChooseOptionsBox::getButtonWidth(const QString &card_name) const
{
    QFontMetrics fontMetrics(Button::defaultFont());
    int width = fontMetrics.width(translate(card_name));
    // Otherwise it would look compact
    width += 30;
    return width;
}

QString ChooseOptionsBox::translate(const QString &option) const
{
    if (skillName == "GameRule_TriggerOrder") {
        int time = 1;
        QString str = option;
        if (str.contains("*")) {
            time = str.split("*").last().toInt();
            str = str.split("*").first();
        }
        QString text = Sanguosha->translate(str);
        if (time > 1)
            text += QString("[%1]").arg(time);

        return text;
    }


    QString title = QString("%1:%2").arg(skillName).arg(option);
    QString translated = Sanguosha->translate(title);
    if (translated == title)
        translated = Sanguosha->translate(option);
    return translated;
}

void ChooseOptionsBox::clear()
{
    foreach (QSanButton *button, buttons)
        button->deleteLater();

    buttons.clear();

    disappear();
}
