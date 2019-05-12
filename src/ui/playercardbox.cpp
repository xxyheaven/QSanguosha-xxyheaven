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

#include "playercardbox.h"
#include "clientplayer.h"
#include "skin-bank.h"
#include "engine.h"
#include "carditem.h"
#include "client.h"
#include "button.h"
#include "timed-progressbar.h"

#include <QGraphicsProxyWidget>

static QChar handcardFlag('h');
static QChar equipmentFlag('e');
static QChar judgingFlag('j');

const int PlayerCardBox::maxCardNumberInOneRow = 20;

const int PlayerCardBox::verticalBlankWidth = 37;
const int PlayerCardBox::placeNameAreaWidth = 15;
const int PlayerCardBox::intervalBetweenNameAndCard = 20;
const int PlayerCardBox::topBlankWidth = 42;
const int PlayerCardBox::bottomBlankWidth = 25;
const int PlayerCardBox::intervalBetweenAreas = 10;
const int PlayerCardBox::intervalBetweenRows = 5;
const int PlayerCardBox::intervalBetweenCards = 3;

PlayerCardBox::PlayerCardBox()
    : player(NULL), confirm(new Button(tr("confirm"), 0.6)), progressBar(NULL),
    rowCount(0), intervalsBetweenAreas(-1), intervalsBetweenRows(0), maxCardsInOneRow(0)
{
	confirm->setEnabled(ClientInstance->getReplayer());
    confirm->setParentItem(this);
    connect(confirm, &Button::clicked, this, &PlayerCardBox::reply2);
}

void PlayerCardBox::chooseCard(const QString &reason, const ClientPlayer *player,
    const QString &flags, bool handcardVisible,
    Card::HandlingMethod method, const QList<int> &disabledIds, const QList<int> &handcards, int min_num, int max_num)
{
    nameRects.clear();
    rowCount = 0;
    intervalsBetweenAreas = -1;
    intervalsBetweenRows = 0;
    maxCardsInOneRow = 0;

	this->single_result = false;
	if (max_num == 0) {
		min_num = 1;
		max_num = 1;
		this->single_result = true;
	}

	this->reason = reason;
    this->handcards = handcards;
    this->player = player;
	QString reason_tr = Sanguosha->translate(reason);
    if (reason == "zhidao" || reason == "fenglve1")
		this->title = tr("%1: please choose %2's card each area").arg(reason_tr).arg(ClientInstance->getPlayerName(player->objectName()));
	else if (single_result)
		this->title = tr("%1: please choose %2's card").arg(reason_tr).arg(ClientInstance->getPlayerName(player->objectName()));
	else if (max_num == min_num)
		this->title = tr("%1: please choose %2's %3 cards").arg(reason_tr).arg(ClientInstance->getPlayerName(player->objectName())).arg(max_num);
	else
		this->title = tr("%1: please choose %2's %3 to %4 cards").arg(reason_tr).arg(ClientInstance->getPlayerName(player->objectName())).arg(min_num).arg(max_num);
	
    this->flags = flags;
	this->min_num = min_num;
	this->max_num = max_num;
    bool handcard = false;
    bool equip = false;
    bool judging = false;

    if (flags.contains(handcardFlag) && !player->isKongcheng()) {
        updateNumbers(player->getHandcardNum());
        handcard = true;
    }

    if (flags.contains(equipmentFlag) && player->hasEquip()) {
        updateNumbers(player->getEquips().length());
        equip = true;
    }

    if (flags.contains(judgingFlag) && !player->getJudgingArea().isEmpty()) {
        updateNumbers(player->getJudgingArea().length());
        judging = true;
    }

    int max = maxCardsInOneRow;
    int maxNumber = maxCardNumberInOneRow;
    maxCardsInOneRow = qMin(max, maxNumber);

    prepareGeometryChange();

    moveToCenter();
    show();

    this->handcardVisible = handcardVisible;
    this->method = method;
    this->disabledIds = disabledIds;

    const int startX = verticalBlankWidth + placeNameAreaWidth + intervalBetweenNameAndCard;
    int index = 0;

    if (handcard) {
        QList<const Card *> cards;
        for (int i = 0; i < handcards.length(); ++i)
            cards << Sanguosha->getCard(handcards.at(i));
        arrangeCards(cards, QPoint(startX, nameRects.at(index).y()));

        ++index;
    }

    if (equip) {
        arrangeCards(player->getEquips(), QPoint(startX, nameRects.at(index).y()));

        ++index;
    }

    if (judging)
        arrangeCards(player->getJudgingArea(), QPoint(startX, nameRects.at(index).y()));

    if (min_num == max_num) {
        confirm->hide();
    } else {
        confirm->setPos(boundingRect().center().x() - confirm->boundingRect().width() / 2, boundingRect().height() - ((ServerInfo.OperationTimeout == 0) ? 40 : 60));
        confirm->show();
		confirm->setEnabled(false);
    }

    if (ServerInfo.OperationTimeout != 0) {
        if (!progressBar) {
            progressBar = new QSanCommandProgressBar();
            progressBar->setMaximumWidth(qMin(boundingRect().width() - 16, (qreal)150));
            progressBar->setMaximumHeight(12);
            progressBar->setTimerEnabled(true);
            progressBarItem = new QGraphicsProxyWidget(this);
            progressBarItem->setWidget(progressBar);
            progressBarItem->setPos(boundingRect().center().x() - progressBarItem->boundingRect().width() / 2, boundingRect().height() - 20);
            if (single_result)
				connect(progressBar, SIGNAL(timedOut()), this, SLOT(reply()));
			else
				connect(progressBar, SIGNAL(timedOut()), this, SLOT(reply2()));
        }
        progressBar->setCountdown(QSanProtocol::S_COMMAND_CHOOSE_CARD);
        progressBar->show();
    }
}

QRectF PlayerCardBox::boundingRect() const
{
    if (player == NULL)
        return QRectF();

    if (rowCount == 0)
        return QRectF();

    const int cardWidth = G_COMMON_LAYOUT.m_cardNormalWidth;
    const int cardHeight = G_COMMON_LAYOUT.m_cardNormalHeight;

    int width = verticalBlankWidth * 2 + placeNameAreaWidth + intervalBetweenNameAndCard;

    if (maxCardsInOneRow > maxCardNumberInOneRow / 2) {
        width += cardWidth * maxCardNumberInOneRow / 2
            + intervalBetweenCards * (maxCardNumberInOneRow / 2 - 1);
    } else {
        width += cardWidth * maxCardsInOneRow
            + intervalBetweenCards * (maxCardsInOneRow - 1);
    }

    int areaInterval = intervalBetweenAreas;
    int height = topBlankWidth + bottomBlankWidth + cardHeight * rowCount
        + intervalsBetweenAreas * qMax(areaInterval, 0)
        + intervalsBetweenRows * intervalBetweenRows;

    if (ServerInfo.OperationTimeout != 0)
        height += 12;

    if (ServerInfo.OperationTimeout != 0)
        height += 12;

	if (max_num > min_num)
		height += 30;

    return QRectF(0, 0, width, height);
}

void PlayerCardBox::paintLayout(QPainter *painter)
{
    if (nameRects.isEmpty())
        return;

    foreach(const QRect &rect, nameRects)
        painter->drawRoundedRect(rect, 3, 3);

    int index = 0;

    if (flags.contains(handcardFlag) && !player->isKongcheng()) {
        G_COMMON_LAYOUT.playerCardBoxPlaceNameText.paintText(painter, nameRects.at(index),
            Qt::AlignCenter,
            tr("Handcard area"));
        ++index;
    }
    if (flags.contains(equipmentFlag) && player->hasEquip()) {
        G_COMMON_LAYOUT.playerCardBoxPlaceNameText.paintText(painter, nameRects.at(index),
            Qt::AlignCenter,
            tr("Equip area"));
        ++index;
    }
    if (flags.contains(judgingFlag) && !player->getJudgingArea().isEmpty()) {
        G_COMMON_LAYOUT.playerCardBoxPlaceNameText.paintText(painter, nameRects.at(index),
            Qt::AlignCenter,
            tr("Judging area"));
    }
}

void PlayerCardBox::clear()
{
    if (progressBar != NULL) {
        progressBar->hide();
        progressBar->deleteLater();
        progressBar = NULL;

        progressBarItem->deleteLater();
    }

    foreach(CardItem *item, items)
        item->deleteLater();
    items.clear();
	selected.clear();

    disappear();
}

int PlayerCardBox::getRowCount(const int &cardNumber) const
{
    return (cardNumber + maxCardNumberInOneRow - 1) / maxCardNumberInOneRow;
}

void PlayerCardBox::updateNumbers(const int &cardNumber)
{
    ++intervalsBetweenAreas;
    if (cardNumber > maxCardsInOneRow)
        maxCardsInOneRow = cardNumber;

    const int cardHeight = G_COMMON_LAYOUT.m_cardNormalHeight;
    const int y = topBlankWidth + rowCount * cardHeight
        + intervalsBetweenAreas * intervalBetweenAreas
        + intervalsBetweenRows * intervalBetweenRows;

    const int count = getRowCount(cardNumber);
    rowCount += count;
    intervalsBetweenRows += count - 1;

    const int height = count * cardHeight
        + (count - 1) * intervalsBetweenRows;

    nameRects << QRect(verticalBlankWidth, y, placeNameAreaWidth, height);
}

void PlayerCardBox::arrangeCards(const CardList &cards, const QPoint &topLeft)
{
    QList<CardItem *> areaItems;

    foreach (const Card *card, cards) {
        CardItem *item = new CardItem(card);
        if (handcards.contains(card->getId()) && !handcardVisible && Self != player)
            item = new CardItem(NULL);
        item->setAutoBack(false);
        item->setOuterGlowEffectEnabled(true);
        item->resetTransform();
        item->setParentItem(this);
        item->setFlag(ItemIsMovable, false);
        item->setEnabled(!disabledIds.contains(card->getEffectiveId())
            && (method != Card::MethodDiscard || Self->canDiscard(player, card->getEffectiveId())));
		if (single_result)
			connect(item, SIGNAL(clicked()), this, SLOT(reply()));
		else
			connect(item, SIGNAL(clicked()), this, SLOT(selectCardItem()));
        items << item;
        areaItems << item;
    }

    int n = items.size();
    if (n == 0)
        return;

    const int rows = (n + maxCardNumberInOneRow - 1) / maxCardNumberInOneRow;
    const int cardWidth = G_COMMON_LAYOUT.m_cardNormalWidth;
    const int cardHeight = G_COMMON_LAYOUT.m_cardNormalHeight;
    const int min = qMin(maxCardsInOneRow, maxCardNumberInOneRow / 2);
    const int maxWidth = min * cardWidth + intervalBetweenCards * (min - 1);
    for (int row = 0; row < rows; ++row) {
        int count = qMin(maxCardNumberInOneRow, areaItems.size());
        double step = 0;
        if (count > 1) {
            step = qMin((double)cardWidth + intervalBetweenCards,
                (double)(maxWidth - cardWidth) / qMax(count - 1, 0));
        }
        for (int i = 0; i < count; ++i) {
            CardItem *item = areaItems.takeFirst();
            const double x = topLeft.x() + step * i;
            const double y = topLeft.y() + (cardHeight + intervalBetweenRows) * row;
            item->setPos(x, y);
        }
    }
}

void PlayerCardBox::selectCardItem()
{
    CardItem *item = qobject_cast<CardItem *>(sender());
	
	if (selected.contains(item)) {
		item->setChosen(false);
		selected.removeOne(item);
	} else {
		item->setChosen(true);
        if (reason == "zhidao" || reason == "fenglve1") {
			int id1 = item->getId();
			foreach(CardItem *item2, selected) {
				int id2 = item2->getId();
				if (((id1 == Card::S_UNKNOWN_CARD_ID || handcards.contains(id1)) && (id2 == Card::S_UNKNOWN_CARD_ID || handcards.contains(id2)))
					|| (player->getEquips().contains(Sanguosha->getCard(id1)) && player->getEquips().contains(Sanguosha->getCard(id2)))
					|| (player->getJudgingArea().contains(Sanguosha->getCard(id1)) && player->getJudgingArea().contains(Sanguosha->getCard(id2)))) {
					item2->setChosen(false);
					selected.removeOne(item2);
					break;
				}
			}
		}
		selected << item;
		if (selected.length() == max_num)
			reply2();
    }
	confirm->setEnabled(selected.length() >= min_num);
}

void PlayerCardBox::reply()
{
    CardItem *item = qobject_cast<CardItem *>(sender());

    int index = items.indexOf(item);

    int card_id = -2;

    if (item)
        card_id = item->getId();

    if (card_id == Card::S_UNKNOWN_CARD_ID && index != -1)
        card_id = handcards.at(index);

    ClientInstance->onPlayerChooseCard(card_id);
}

void PlayerCardBox::reply2()
{
    QList<int> card_ids;
	foreach(CardItem *item, selected) {
		int index = items.indexOf(item);
		int card_id = -2;
		if (item)
			card_id = item->getId();
		if (card_id == Card::S_UNKNOWN_CARD_ID)
			card_id = handcards.at(index);
        card_ids << card_id;
	}
	ClientInstance->onPlayerChooseCards(card_ids);
}
