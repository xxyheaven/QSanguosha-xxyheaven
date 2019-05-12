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

#include "pindianbox.h"
#include "roomscene.h"
#include "engine.h"
#include "client.h"
#include "graphicsbox.h"
#include <QMessageBox>

PindianBox::PindianBox()
    : CardContainer()
{
}

void PindianBox::doPindian(const QString &requestor, const QString &reason, const QStringList &targets)
{
    clear();
    _m_mutex_pindian.lock();
    zhuge = requestor;
    this->targets = targets;
    this->reason = reason;
    finished = false;
    upItems.clear();
    downItems.clear();
    scene_width = RoomSceneInstance->sceneRect().width();

    for (int i = 1; i <= targets.length(); i++) {
        card_ids << -1;
        CardItem *cardItem = new CardItem(Sanguosha->getCard(-1));
        cardItem->setFlag(QGraphicsItem::ItemIsFocusable);
        cardItem->setFlag(ItemIsMovable, false);
        cardItem->setAutoBack(false);
        cardItem->setFootnote(ClientInstance->getPlayerName(targets.at(i - 1)) + isYou(targets.at(i - 1)));
        upItems << cardItem;
        cardItem->setParentItem(this);
    }

    CardItem *card = new CardItem(Sanguosha->getCard(-1));
    card->setFlag(QGraphicsItem::ItemIsFocusable);
    card->setFlag(ItemIsMovable, false);
    card->setAutoBack(false);
    card->setFootnote(QString("%1%2%3").arg(ClientInstance->getPlayerName(zhuge)).arg(isYou(zhuge)).arg(tr("request")));
    card->setParentItem(this);
    downItems << card;

    itemCount = upItems.length();

    int cardWidth = G_COMMON_LAYOUT.m_cardNormalWidth;
    int cardHeight = G_COMMON_LAYOUT.m_cardNormalHeight;
    int count = (itemCount >= 2) ? itemCount : 2;
    int width = (cardWidth + cardInterval) * count - cardInterval + 50;
    this->width = width;

    if (upItems.length() == 1) {
        QPointF pos(25, 45);
        card->resetTransform();
        card->setOuterGlowEffectEnabled(false);
        card->setPos(pos);
        card->setHomePos(pos);
        card->goBack(true);
        card->hide();

        CardItem *cardItem = upItems.first();
        cardItem->resetTransform();
        cardItem->setOuterGlowEffectEnabled(false);
        pos = QPointF(25 + cardWidth + cardInterval, 45);
        cardItem->setPos(pos);
        cardItem->setHomePos(pos);
        cardItem->goBack(true);
        cardItem->hide();
    } else {
        for (int i = 0; i < upItems.length(); i++) {
            CardItem *cardItem = upItems.at(i);

            QPointF pos;
            int X, Y;

            X = 25 + (cardWidth + cardInterval) * i;
            Y = 45;

            pos.setX(X);
            pos.setY(Y);
            cardItem->resetTransform();
            cardItem->setOuterGlowEffectEnabled(false);
            cardItem->setPos(pos);
            cardItem->setHomePos(pos);
            cardItem->goBack(true);
            cardItem->hide();
        }
        QPointF pos;
        pos.setX((width - cardWidth) / 2);
        pos.setY(45 + cardHeight + cardInterval);
        card->resetTransform();
        card->setOuterGlowEffectEnabled(false);
        card->setPos(pos);
        card->setHomePos(pos);
        card->goBack(true);
        card->hide();
    }

    prepareGeometryChange();
    GraphicsBox::moveToCenter(this);
    show();
    _m_mutex_pindian.unlock();
}

void PindianBox::onReply(const QString &who, int card_id)
{
    _m_mutex_pindian.lock();
    if (who == zhuge) {
        this->card_id = card_id;
        downItems.first()->show();
    } else {
        for (int i = 0; i < targets.length(); i++) {
            if (who == targets.at(i)) {
                upItems.at(i)->show();
                card_ids[i] = card_id;
            }
        }
    }
    update();
    _m_mutex_pindian.unlock();
}

void PindianBox::doPindianAnimation(const QString &who)
{
    _m_mutex_pindian.lock();
    downItems.first()->setCard(Sanguosha->getCard(card_id));
    for (int i = 0; i < targets.length(); i++) {
        if (who == targets.at(i)) {
            upItems.at(i)->setCard(Sanguosha->getCard(card_ids.at(i)));
            break;
        } else
            upItems.at(i)->setEnabled(false);
    }
    update();
    _m_mutex_pindian.unlock();
}

void PindianBox::playSuccess(int type, int index)
{
    _m_mutex_pindian.lock();
    PixmapAnimation::GetPixmapAnimation(downItems.first(), type == 1 ? "success" : "no-success");
    _m_mutex_pindian.unlock();
    if (index == targets.length()) {
        finished = true;
        QTimer::singleShot(1500, this, SLOT(clear()));
    }
}

void PindianBox::clear()
{
    if (!finished) return;
    _m_mutex_pindian.lock();
    foreach(CardItem *card_item, upItems)
        card_item->deleteLater();
    foreach(CardItem *card_item, downItems)
        card_item->deleteLater();

    upItems.clear();
    downItems.clear();

    prepareGeometryChange();
    hide();
    _m_mutex_pindian.unlock();
}

QString PindianBox::isYou(QString player_name)
{
    return Self == ClientInstance->getPlayer(player_name) ? tr("(you)") : "";
}

QRectF PindianBox::boundingRect() const
{
    const int card_height = G_COMMON_LAYOUT.m_cardNormalHeight;
    int height = cardInterval + card_height * (targets.length() > 1 ? 2 : 1);
    height += 70;

    return QRectF(0, 0, width, height);
}

void PindianBox::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QString title = QString("%1: %2").arg(tr("pindian")).arg(Sanguosha->translate(reason));
    GraphicsBox::paintGraphicsBoxStyle(painter, title, boundingRect());

    const int card_width = G_COMMON_LAYOUT.m_cardNormalWidth;
    const int card_height = G_COMMON_LAYOUT.m_cardNormalHeight;
    IQSanComponentSkin::QSanSimpleTextFont font = G_COMMON_LAYOUT.m_cardFootnoteFont;
    Qt::Alignment align = (Qt::AlignmentFlag)((int)Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWrapAnywhere);

    if (targets.length() == 1) {
        QRect bottom_rect(25, 45, card_width, card_height);
        painter->drawPixmap(bottom_rect, G_ROOM_SKIN.getPixmap(QSanRoomSkin::S_SKIN_KEY_CHOOSE_GENERAL_BOX_DEST_SEAT));
        QRect fn_rect2(QPoint(bottom_rect.x()+1, bottom_rect.y()-6), bottom_rect.size());
        font.paintText(painter, fn_rect2, align, QString("%1%2%3").arg(ClientInstance->getPlayerName(zhuge)).arg(isYou(zhuge)).arg(tr("request")));

        QRect top_rect(25 + card_width + cardInterval, 45, card_width, card_height);
        painter->drawPixmap(top_rect, G_ROOM_SKIN.getPixmap(QSanRoomSkin::S_SKIN_KEY_CHOOSE_GENERAL_BOX_DEST_SEAT));
        QRect fn_rect1(QPoint(top_rect.x()+1, top_rect.y()-6), top_rect.size());
        font.paintText(painter, fn_rect1, align, ClientInstance->getPlayerName(targets.first()) + isYou(targets.first()));
    } else {
        for (int i = 0; i < targets.length(); ++i) {
            QRect top_rect(25 + (card_width + cardInterval) * i, 45, card_width, card_height);
            painter->drawPixmap(top_rect, G_ROOM_SKIN.getPixmap(QSanRoomSkin::S_SKIN_KEY_CHOOSE_GENERAL_BOX_DEST_SEAT));
            QRect fn_rect1(QPoint(top_rect.x()+1, top_rect.y()-6), top_rect.size());
            font.paintText(painter, fn_rect1, align, ClientInstance->getPlayerName(targets.at(i)) + isYou(targets.at(i)));
        }

        QRect bottom_rect((width - card_width) / 2, 45 + card_height + cardInterval, card_width, card_height);
        painter->drawPixmap(bottom_rect, G_ROOM_SKIN.getPixmap(QSanRoomSkin::S_SKIN_KEY_CHOOSE_GENERAL_BOX_DEST_SEAT));
        QRect fn_rect2(QPoint(bottom_rect.x()+1, bottom_rect.y()-6), bottom_rect.size());
        font.paintText(painter, fn_rect2, align, QString("%1%2%3").arg(ClientInstance->getPlayerName(zhuge)).arg(isYou(zhuge)).arg(tr("request")));
    }
}
