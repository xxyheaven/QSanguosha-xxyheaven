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

#include "carditem.h"
#include "engine.h"
#include "skill.h"
#include "clientplayer.h"
#include "settings.h"
#include "roomscene.h"

#include <cmath>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QFocusEvent>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>

void CardItem::_initialize()
{
    setFlag(QGraphicsItem::ItemIsMovable);
    m_card = NULL;
    m_opacityAtHome = 1.0;
    m_currentAnimation = NULL;
    _m_width = G_COMMON_LAYOUT.m_cardNormalWidth;
    _m_height = G_COMMON_LAYOUT.m_cardNormalHeight;
    _m_showFootnote = true;
    m_isSelected = false;
    _m_isUnknownGeneral = false;
    auto_back = true;
    frozen = false;
    resetTransform();
    setTransform(QTransform::fromTranslate(-_m_width / 2, -_m_height / 2), true);
	outerGlowEffectEnabled = false;
    outerGlowEffect = NULL;
    outerGlowColor = Qt::white;
    _m_validate_suit = Card::SuitToBeDecided;
    m_isChosen = false;
    m_isHovered = false;
    emotion_item = new Sprite(this);
}

CardItem::CardItem(const Card *card)
{
    _initialize();
    setCard(card);
    setAcceptHoverEvents(true);
}

CardItem::CardItem(const QString &general_name)
{
    m_cardId = Card::S_UNKNOWN_CARD_ID;
    _initialize();
    changeGeneral(general_name);
    m_currentAnimation = NULL;
    m_opacityAtHome = 1.0;
}

QRectF CardItem::boundingRect() const
{
    return G_COMMON_LAYOUT.m_cardFrameArea;
}

void CardItem::setCard(const Card *card)
{
    if (card != NULL) {
        if (card->isVirtualCard()) {
            m_card = card;
            m_cardId = Card::S_UNKNOWN_CARD_ID;
            _m_validate_suit = card->getSuit();
            setObjectName(card->objectName());
            setToolTip(card->getDescription());
        } else {
            m_cardId = card->getId();
            const Card *engineCard = Sanguosha->getEngineCard(m_cardId);
            Q_ASSERT(engineCard != NULL);
            m_card = engineCard;
            setObjectName(engineCard->objectName());
            QString description = engineCard->getDescription();
            setToolTip(description);
        }
    } else {
        m_cardId = Card::S_UNKNOWN_CARD_ID;
        setObjectName("unknown");
    }
}

void CardItem::setEnabled(bool enabled)
{
    QSanSelectableItem::setEnabled(enabled);
}

CardItem::~CardItem()
{
    m_animationMutex.lock();
    if (m_currentAnimation != NULL) {
        m_currentAnimation->stop();
        delete m_currentAnimation;
        m_currentAnimation = NULL;
    }
    m_animationMutex.unlock();
    if (emotion_item) {
        delete emotion_item;
        emotion_item = NULL;
    }
}

void CardItem::changeGeneral(const QString &general_name)
{
    setObjectName(general_name);
    const General *general = Sanguosha->getGeneral(general_name);
    if (general) {
        _m_isUnknownGeneral = false;
        setToolTip(general->getSkillDescription(true));
    } else {
        _m_isUnknownGeneral = true;
        setToolTip(QString());
    }
}

const Card *CardItem::getCard() const
{
    if (m_cardId != Card::S_UNKNOWN_CARD_ID)
        return Sanguosha->getCard(m_cardId);
    if (m_card)
        return m_card;
    return Sanguosha->getCard(m_cardId);
}

void CardItem::setHomePos(QPointF home_pos)
{
    this->home_pos = home_pos;
}

QPointF CardItem::homePos() const
{
    return home_pos;
}

void CardItem::goBack(bool playAnimation, bool doFade, int animation_type, int duration)
{
    if (playAnimation) {
        getGoBackAnimation(doFade, animation_type, true, duration);
        if (m_currentAnimation != NULL)
            m_currentAnimation->start();
    } else {
        m_animationMutex.lock();
        if (m_currentAnimation != NULL) {
            m_currentAnimation->stop();
            delete m_currentAnimation;
            m_currentAnimation = NULL;
        }
        setPos(homePos());
        m_animationMutex.unlock();
    }
}

QAbstractAnimation *CardItem::getGoBackAnimation(bool doFade, int animation_type, bool smoothTransition, int duration)
{
    m_animationMutex.lock();
    if (m_currentAnimation != NULL) {
        m_currentAnimation->stop();
        delete m_currentAnimation;
        m_currentAnimation = NULL;
    }
    QPropertyAnimation *goback = new QPropertyAnimation(this, "pos");

    if (animation_type != -1){
        QPointF newPos = home_pos;
        newPos.setX(animation_type);
        goback->setStartValue(newPos);
    }

    goback->setEndValue(home_pos);

    if (smoothTransition)
        goback->setEasingCurve(QEasingCurve::OutQuad);
    else
        goback->setEasingCurve(QEasingCurve::OutQuint);
    goback->setDuration(duration);

    if (doFade) {
        QParallelAnimationGroup *group = new QParallelAnimationGroup;
        QPropertyAnimation *disappear = new QPropertyAnimation(this, "opacity");
        double middleOpacity = qMax(opacity(), m_opacityAtHome);
        if (middleOpacity == 0) middleOpacity = 1.0;
        disappear->setEndValue(m_opacityAtHome);

        disappear->setKeyValueAt(0.2, middleOpacity);
        disappear->setKeyValueAt(0.8, middleOpacity);
        disappear->setDuration(duration);

        group->addAnimation(goback);
        group->addAnimation(disappear);

        m_currentAnimation = group;
    } else {
        m_currentAnimation = goback;
    }
    m_animationMutex.unlock();
    connect(m_currentAnimation, SIGNAL(finished()), this, SIGNAL(movement_animation_finished()));
    connect(m_currentAnimation, SIGNAL(destroyed()), this, SLOT(currentAnimationDestroyed()));
    return m_currentAnimation;
}

void CardItem::currentAnimationDestroyed()
{
    QObject *ca = sender();
    if (m_currentAnimation == ca)
        m_currentAnimation = NULL;
}

void CardItem::showAvatar(const General *general)
{
    _m_avatarName = general->objectName();
}

void CardItem::hideAvatar()
{
    _m_avatarName = QString();
}

void CardItem::showSmallCard(const QString &card_name)
{
    _m_smallCardName = card_name;;
}

void CardItem::hideSmallCard()
{
    _m_smallCardName = QString();
}

void CardItem::setAutoBack(bool auto_back)
{
    this->auto_back = auto_back;
}

bool CardItem::isEquipped() const
{
    const Card *card = getCard();
    Q_ASSERT(card);
    return Self->hasEquip(card);
}

void CardItem::setFrozen(bool is_frozen)
{
    frozen = is_frozen;
}

CardItem *CardItem::FindItem(const QList<CardItem *> &items, int card_id)
{
    foreach (CardItem *item, items) {
        if (item->getCard() == NULL) {
            if (card_id == Card::S_UNKNOWN_CARD_ID)
                return item;
            else
                continue;
        }
        if (item->getCard()->getId() == card_id)
            return item;
    }

    return NULL;
}

void CardItem::setOuterGlowEffectEnabled(const bool &willPlay)
{
    if (outerGlowEffectEnabled == willPlay) return;
    if (willPlay) {
        if (outerGlowEffect == NULL) {
            outerGlowEffect = new QGraphicsDropShadowEffect(this);
            outerGlowEffect->setOffset(0);
            outerGlowEffect->setBlurRadius(18);
            outerGlowEffect->setColor(outerGlowColor);
            outerGlowEffect->setEnabled(false);
            setGraphicsEffect(outerGlowEffect);
        }
        connect(this, &CardItem::hoverChanged, outerGlowEffect, &QGraphicsDropShadowEffect::setEnabled);
    } else {
        if (outerGlowEffect != NULL) {
            disconnect(this, &CardItem::hoverChanged, outerGlowEffect, &QGraphicsDropShadowEffect::setEnabled);
            outerGlowEffect->setEnabled(false);
        }
    }
    outerGlowEffectEnabled = willPlay;
}

bool CardItem::isOuterGlowEffectEnabled() const
{
    return outerGlowEffectEnabled;
}

void CardItem::setOuterGlowColor(const QColor &color)
{
    if (!outerGlowEffect || outerGlowColor == color) return;
    outerGlowColor = color;
    outerGlowEffect->setColor(color);
}

QColor CardItem::getOuterGlowColor() const
{
    return outerGlowColor;
}

const int CardItem::_S_CLICK_JITTER_TOLERANCE = 1600;
const int CardItem::_S_MOVE_JITTER_TOLERANCE = 200;

void CardItem::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    if (frozen) return;
    _m_lastMousePressScenePos = mapToParent(mouseEvent->pos());
}

void CardItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    if (frozen) return;

    QPointF totalMove = mapToParent(mouseEvent->pos()) - _m_lastMousePressScenePos;
    if (totalMove.x() * totalMove.x() + totalMove.y() * totalMove.y() < _S_MOVE_JITTER_TOLERANCE)
        emit clicked();
    else
        emit released();

    if (auto_back) {
        goBack(true, false);
    }
    update();
}

void CardItem::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    if (!(flags() & QGraphicsItem::ItemIsMovable)) return;
    QPointF newPos = mapToParent(mouseEvent->pos());
    QPointF totalMove = newPos - _m_lastMousePressScenePos;
    if (totalMove.x() * totalMove.x() + totalMove.y() * totalMove.y() >= _S_CLICK_JITTER_TOLERANCE) {
        QPointF down_pos = mouseEvent->buttonDownPos(Qt::LeftButton);
        setPos(newPos - this->transform().map(down_pos));
    }
}

void CardItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (frozen) return;

    if (hasFocus()) {
        event->accept();
        emit double_clicked();
    } else
        emit toggle_discards();
}

void CardItem::hoverEnterEvent(QGraphicsSceneHoverEvent *)
{
    m_isHovered = true;
    emit enter_hover();
    emit hoverChanged(true);
}

void CardItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    m_isHovered = false;
    emit leave_hover();
    emit hoverChanged(false);
}


void CardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    if (!isEnabled()) {
        painter->fillRect(G_COMMON_LAYOUT.m_cardMainArea, QColor(100, 100, 100, 255 * opacity()));
        painter->setOpacity(0.7 * opacity());
    }

    if (!_m_isUnknownGeneral)
        painter->drawPixmap(G_COMMON_LAYOUT.m_cardMainArea, G_ROOM_SKIN.getCardMainPixmap(objectName()));
    else
        painter->drawPixmap(G_COMMON_LAYOUT.m_cardMainArea, G_ROOM_SKIN.getPixmap("generalCardBack", QString()));
    if (m_card) {
        const Card *card = Sanguosha->getEngineCard(m_cardId);
        if (card == NULL)
            painter->drawPixmap(G_COMMON_LAYOUT.m_validateSuitArea, G_ROOM_SKIN.getCardSuitPixmap(_m_validate_suit));
        else {
            painter->drawPixmap(G_COMMON_LAYOUT.m_cardSuitArea, G_ROOM_SKIN.getCardSuitPixmap(card->getSuit()));
            painter->drawPixmap(G_COMMON_LAYOUT.m_cardNumberArea, G_ROOM_SKIN.getCardNumberPixmap(card->getNumber(), card->isBlack()));
        }
    }

    QRect rect = G_COMMON_LAYOUT.m_cardFootnoteArea;
    // Deal with stupid QT...
    if (_m_showFootnote) painter->drawImage(rect, _m_footnoteImage);

    if (!_m_avatarName.isEmpty())
        painter->drawPixmap(G_COMMON_LAYOUT.m_cardAvatarArea, G_ROOM_SKIN.getCardAvatarPixmap(_m_avatarName));

    if (!_m_smallCardName.isEmpty()) {
		QPixmap chartlet = G_ROOM_SKIN.getPixmap(QSanRoomSkin::S_SKIN_KEY_CARD_ITEM_SMALL_CARDS, _m_smallCardName);
		painter->drawPixmap(boundingRect().center().x() - chartlet.width() / 2 + 2, boundingRect().center().y() - chartlet.height() / 2, chartlet);
	}

    if (m_isChosen) {
        QPixmap chartlet = G_ROOM_SKIN.getPixmap(QSanRoomSkin::S_SKIN_KEY_CARD_ITEM_CHECK);
		painter->drawPixmap(boundingRect().center().x() - chartlet.width() / 2, boundingRect().bottom() - chartlet.height() - 10, chartlet);
	}
}

void CardItem::setFootnote(const QString &desc)
{
    const IQSanComponentSkin::QSanShadowTextFont &font = G_COMMON_LAYOUT.m_cardFootnoteFont;
    QRect rect = G_COMMON_LAYOUT.m_cardFootnoteArea;
    rect.moveTopLeft(QPoint(0, 0));
    _m_footnoteImage = QImage(rect.size(), QImage::Format_ARGB32);
    _m_footnoteImage.fill(Qt::transparent);
    QPainter painter(&_m_footnoteImage);
    font.paintText(&painter, QRect(QPoint(0, 0), rect.size()),
        (Qt::AlignmentFlag)((int)Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWrapAnywhere), desc);
}

bool CardItem::setEmotion(const QString &emotion, bool permanent)
{
    if (emotion == ".") {
        hideEmotion();
        return false;
    }

    QString path = QString("image/system/emotion/goldencard/%1.png").arg(emotion);
    if (QFile::exists(path)) {
        QPixmap pixmap = QPixmap(path);
        emotion_item->setPixmap(pixmap);
        emotion_item->setPos((G_PHOTO_LAYOUT.m_normalWidth - pixmap.width()) / 2,
            (G_PHOTO_LAYOUT.m_normalHeight - pixmap.height()) / 2);

        QPropertyAnimation *appear = new QPropertyAnimation(emotion_item, "opacity");
        appear->setStartValue(0.0);
        if (permanent) {
            appear->setEndValue(1.0);
            appear->setDuration(500);
        } else {
            appear->setKeyValueAt(0.25, 1.0);
            appear->setKeyValueAt(0.75, 1.0);
            appear->setEndValue(0.0);
            appear->setDuration(2000);
        }
        appear->start(QAbstractAnimation::DeleteWhenStopped);
        return true;
    } else {
        QString wholemotion = QString("goldencard/") + emotion;
        PixmapAnimation *pma = PixmapAnimation::GetPixmapAnimation(this, wholemotion, 150);
        return pma->valid();
    }
}

void CardItem::hideEmotion()
{
    QPropertyAnimation *disappear = new QPropertyAnimation(emotion_item, "opacity");
    disappear->setStartValue(1.0);
    disappear->setEndValue(0.0);
    disappear->setDuration(500);
    disappear->start(QAbstractAnimation::DeleteWhenStopped);
}
