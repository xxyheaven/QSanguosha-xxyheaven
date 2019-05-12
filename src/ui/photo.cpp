#include "photo.h"
#include "clientplayer.h"
#include "settings.h"
#include "carditem.h"
#include "engine.h"
#include "standard.h"
#include "client.h"
#include "rolecombobox.h"
#include "skin-bank.h"

#include <QPainter>
#include <QDrag>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QMessageBox>
#include <QGraphicsProxyWidget>
#include <QTimer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QMenu>
#include <QFile>

#include "pixmapanimation.h"

using namespace QSanProtocol;

// skins that remain to be extracted:
// equips
// mark
// emotions
// hp
// seatNumber
// death logo
// kingdom mask and kingdom icon (decouple from player)
// make layers (drawing order) configurable

Photo::Photo() : PlayerCardContainer()
{
    _m_mainFrame = NULL;
    m_player = NULL;
    _m_focusFrame = NULL;
    _m_onlineStatusItem = NULL;
    _m_layout = &G_PHOTO_LAYOUT;
    _m_frameType = S_FRAME_NO_FRAME;
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setTransform(QTransform::fromTranslate(-G_PHOTO_LAYOUT.m_normalWidth / 2, -G_PHOTO_LAYOUT.m_normalHeight / 2), true);

    _m_skillNameLabel = new QLabel;
    _m_skillNameLabel->setStyleSheet("QLabel { background-color: transparent; }");
    _m_skillNameRegion = new QGraphicsProxyWidget();
    _m_skillNameRegion->setWidget(_m_skillNameLabel);
    _m_skillNameRegion->setPos(G_PHOTO_LAYOUT.m_skillNameArea.topLeft());
    _m_skillNameRegion->setParentItem(this);
    _m_skillNameRegion->hide();
    _m_skillNameAnim = new QParallelAnimationGroup(this);

    emotion_item = new Sprite(_m_groupMain);

    _m_duanchangMask = new QGraphicsRectItem(_m_groupMain);
    _m_duanchangMask->setRect(boundingRect());
    _m_duanchangMask->setZValue(32767.0);
    _m_duanchangMask->setOpacity(0.4);
    _m_duanchangMask->hide();
    QBrush duanchang_brush(G_PHOTO_LAYOUT.m_duanchangMaskColor);
    _m_duanchangMask->setBrush(duanchang_brush);

    _createControls();
}

Photo::~Photo()
{
    if (emotion_item) {
        delete emotion_item;
        emotion_item = NULL;
    }
}

void Photo::refresh(bool killed)
{
    PlayerCardContainer::refresh(killed);
    if (!m_player) return;
    QString state_str = m_player->getState();
    if (!state_str.isEmpty() && state_str != "online") {
        QRect rect = G_PHOTO_LAYOUT.m_onlineStatusArea;
        QImage image(rect.size(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.fillRect(QRect(0, 0, rect.width(), rect.height()), G_PHOTO_LAYOUT.m_onlineStatusBgColor);
        G_PHOTO_LAYOUT.m_onlineStatusFont.paintText(&painter, QRect(QPoint(0, 0), rect.size()),
            Qt::AlignCenter,
            Sanguosha->translate(state_str));
        QPixmap pixmap = QPixmap::fromImage(image);
        _paintPixmap(_m_onlineStatusItem, rect, pixmap, _m_groupMain);
        _layBetween(_m_onlineStatusItem, _m_mainFrame, _m_chainIcon);
        if (!_m_onlineStatusItem->isVisible()) _m_onlineStatusItem->show();
    } else if (_m_onlineStatusItem != NULL && state_str == "online")
        _m_onlineStatusItem->hide();

}

QRectF Photo::boundingRect() const
{
    return QRect(0, 0, G_PHOTO_LAYOUT.m_normalWidth, G_PHOTO_LAYOUT.m_normalHeight);
}

void Photo::repaintAll()
{
    resetTransform();
    setTransform(QTransform::fromTranslate(-G_PHOTO_LAYOUT.m_normalWidth / 2, -G_PHOTO_LAYOUT.m_normalHeight / 2), true);
    _paintPixmap(_m_mainFrame, G_PHOTO_LAYOUT.m_mainFrameArea, QSanRoomSkin::S_SKIN_KEY_MAINFRAME);
    setFrame(_m_frameType);
    hideSkillName(); // @todo: currently we don't adjust skillName's position for simplicity,
    // consider repainting it instead of hiding it in the future.
    PlayerCardContainer::repaintAll();
    refresh();
}

void Photo::_adjustComponentZValues(bool killed)
{
    PlayerCardContainer::_adjustComponentZValues(killed);
    _layBetween(_m_mainFrame, _m_faceTurnedIcon, _m_equipRegions[3]);
    _layBetween(emotion_item, _m_chainIcon, _m_roleComboBox);
    _layBetween(_m_skillNameRegion, _m_chainIcon, _m_roleComboBox);
    _m_skillNameRegion->setZValue(10000);
    _m_progressBarItem->setZValue(_m_groupMain->zValue() + 1);
}

void Photo::setEmotion(const QString &emotion, bool permanent)
{
    if (emotion == ".") {
        hideEmotion();
        return;
    }

    QString path = QString("image/system/emotion/%1.png").arg(emotion);
    if (QFile::exists(path)) {
        QPixmap pixmap = QPixmap(path);
        emotion_item->setPixmap(pixmap);
        emotion_item->setPos((G_PHOTO_LAYOUT.m_normalWidth - pixmap.width()) / 2,
            (G_PHOTO_LAYOUT.m_normalHeight - pixmap.height()) / 2);
        _layBetween(emotion_item, _m_chainIcon, _m_roleComboBox);

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
    } else {
        PixmapAnimation::GetPixmapAnimation(this, emotion);
    }
}

void Photo::tremble()
{
    QPropertyAnimation *vibrate = new QPropertyAnimation(this, "x");
    static qreal offset = 20;

    vibrate->setKeyValueAt(0.5, x() - offset);
    vibrate->setEndValue(x());

    vibrate->setEasingCurve(QEasingCurve::OutInBounce);

    vibrate->start(QAbstractAnimation::DeleteWhenStopped);
}

void Photo::showSkillName(const QString &skill_name)
{
    QRect rect = G_PHOTO_LAYOUT.m_skillNameArea;
    QPixmap bg = _getPixmap(QSanRoomSkin::S_SKIN_KEY_SKILL_NAME_BG);
    QPixmap pixmap(rect.size());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.drawPixmap((rect.width()-bg.size().width())/2, rect.height()-bg.size().height(), bg.size().width(), bg.size().height(), bg);

    G_PHOTO_LAYOUT.m_skillNameFont.paintText(&painter, QRect(0,0,rect.width(),rect.height()), Qt::AlignCenter, Sanguosha->translate(skill_name));

    _m_skillNameRegion->setWidget(NULL);
    _m_skillNameLabel = new QLabel;
    _m_skillNameLabel->setStyleSheet("QLabel { background-color: transparent; }");
    _m_skillNameLabel->setPixmap(pixmap);
    _m_skillNameRegion->setWidget(_m_skillNameLabel);

    _mutexSkillNameAnim.lock();
    _m_skillNameRegion->setPos(rect.topLeft() - QPoint(50, 0));
    _m_skillNameRegion->show();
    _m_skillNameRegion->setOpacity(0);
    _m_skillNameAnim->stop();
    _m_skillNameAnim->clear();
    QPropertyAnimation *anim = new QPropertyAnimation(_m_skillNameRegion, "pos");
    anim->setEndValue(rect.topLeft());
    anim->setDuration(200);
    _m_skillNameAnim->addAnimation(anim);
    connect(anim, SIGNAL(finished()), anim, SLOT(deleteLater()));
    anim = new QPropertyAnimation(_m_skillNameRegion, "opacity");
    anim->setEndValue(255);
    anim->setDuration(200);
    _m_skillNameAnim->addAnimation(anim);
    connect(anim, SIGNAL(finished()), anim, SLOT(deleteLater()));
    _m_skillNameAnim->start();
    _mutexSkillNameAnim.unlock();

    skill_names << skill_name;
    QTimer::singleShot(2000, this, SLOT(hideSkillName()));
}

void Photo::hideSkillName()
{
    if (!skill_names.isEmpty())
        skill_names.removeFirst();
    if (skill_names.isEmpty()) {
        _mutexSkillNameAnim.lock();
        _m_skillNameAnim->stop();
        _m_skillNameAnim->clear();
        QPropertyAnimation *anim = new QPropertyAnimation(_m_skillNameRegion, "pos");
        anim->setEndValue(G_PHOTO_LAYOUT.m_skillNameArea.topLeft() + QPoint(50, 0));
        anim->setDuration(200);
        _m_skillNameAnim->addAnimation(anim);
        connect(anim, SIGNAL(finished()), anim, SLOT(deleteLater()));
        anim = new QPropertyAnimation(_m_skillNameRegion, "opacity");
        anim->setEndValue(0);
        anim->setDuration(200);
        _m_skillNameAnim->addAnimation(anim);
        connect(anim, SIGNAL(finished()), anim, SLOT(deleteLater()));
        _m_skillNameAnim->start();
        _mutexSkillNameAnim.unlock();
    }
}

void Photo::hideEmotion()
{
    QPropertyAnimation *disappear = new QPropertyAnimation(emotion_item, "opacity");
    disappear->setStartValue(1.0);
    disappear->setEndValue(0.0);
    disappear->setDuration(500);
    disappear->start(QAbstractAnimation::DeleteWhenStopped);
}

void Photo::updateDuanchang()
{
    if (!m_player) return;
    _m_duanchangMask->setVisible(m_player->getMark("@duanchang") > 0);
}

const ClientPlayer *Photo::getPlayer() const
{
    return m_player;
}

void Photo::speak(const QString &)
{
}

QList<CardItem *> Photo::removeCardItems(const QList<int> &card_ids, const CardsMoveStruct &moveInfo)
{
    Player::Place place = moveInfo.from_place;
    Player::Place to_place = moveInfo.to_place;
    QList<CardItem *> result;
    if (place == Player::PlaceHand || place == Player::PlaceSpecial) {
        if (to_place != Player::PlaceSpecial)
            result = _createCards(card_ids);
        updateHandcardNum();
    } else if (place == Player::PlaceEquip) {
        result = removeEquips(card_ids);
    } else if (place == Player::PlaceDelayedTrick) {
        result = removeDelayedTricks(card_ids);
    }
    if (to_place == Player::PlaceSpecial) {
        foreach (CardItem *card_item, result) {
            Q_ASSERT(card_item);
            if (card_item) {
                result.removeOne(card_item);
                delete card_item;
            }
        }
        return QList<CardItem *>();
    }
    _disperseCards(result, G_PHOTO_LAYOUT.m_cardMoveRegion.center(), false);

    update();
    return result;
}

bool Photo::_addCardItems(QList<CardItem *> &card_items, const CardsMoveStruct &moveInfo)
{
    _disperseCards(card_items, G_PHOTO_LAYOUT.m_cardMoveRegion.center(), true);
    double homeOpacity = 0.0;
    Player::Place place = moveInfo.to_place;

    foreach(CardItem *card_item, card_items)
        card_item->setHomeOpacity(homeOpacity);

    if (place == Player::PlaceEquip) {
        addEquips(card_items);
        return false;
    } else if (place == Player::PlaceDelayedTrick) {
        addDelayedTricks(card_items);
        return false;
    } else if (place == Player::PlaceHand)
        updateHandcardNum();

    return true;
}

void Photo::setFrame(FrameType type)
{
    _m_frameType = type;
    if (type == S_FRAME_NO_FRAME) {
        if (_m_focusFrame) {
            if (_m_saveMeIcon && _m_saveMeIcon->isVisible())
                setFrame(S_FRAME_SOS);
            else if (m_player->getPhase() != Player::NotActive)
                setFrame(S_FRAME_PLAYING);
            else
                _m_focusFrame->hide();
        }
    } else {
        _paintPixmap(_m_focusFrame, G_PHOTO_LAYOUT.m_focusFrameArea,
            _getPixmap(QSanRoomSkin::S_SKIN_KEY_FOCUS_FRAME, QString::number(type)),
            _m_groupMain);
        _layBetween(_m_focusFrame, _m_avatarArea, _m_mainFrame);
        _m_focusFrame->show();
    }
    update();
}

void Photo::updatePhase()
{
    PlayerCardContainer::updatePhase();
    if (m_player->getPhase() != Player::NotActive)
        setFrame(S_FRAME_PLAYING);
    else
        setFrame(S_FRAME_NO_FRAME);
}

void Photo::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
}

QGraphicsItem *Photo::getMouseClickReceiver()
{
    return this;
}

