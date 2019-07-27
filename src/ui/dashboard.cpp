#include "dashboard.h"
#include "engine.h"
#include "settings.h"
#include "client.h"
#include "standard.h"
#include "roomscene.h"
#include "wind.h"
#include "heroskincontainer.h"
#include "graphicspixmaphoveritem.h"
#include "guhuobox.h"
#include "selectbox.h"

#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QParallelAnimationGroup>

using namespace QSanProtocol;

Dashboard::Dashboard(QGraphicsPixmapItem *widget)
    : button_widget(widget), selected(NULL), view_as_skill(NULL), filter(NULL),
    m_changeHeadHeroSkinButton(NULL), m_changeDeputyHeroSkinButton(NULL),
    m_headHeroSkinContainer(NULL), m_deputyHeroSkinContainer(NULL)
{
    Q_ASSERT(button_widget);
    _dlayout = &G_DASHBOARD_LAYOUT;
    _m_layout = _dlayout;
    m_player = Self;
    _m_leftFrame = _m_rightFrame = _m_middleFrame = _m_flowerFrame = _m_treeFrame = NULL;
    //_m_rightFrameBg = NULL;
    animations = new EffectAnimation();
    pending_card = NULL;
    _m_guhuo_expanded = QList<CardItem *>();
    _m_pile_expanded = QMap<QString, QList<int> >();
    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        _m_equipSkillBtns[i] = NULL;
        _m_isEquipsAnimOn[i] = false;
    }
    // At this stage, we cannot decide the dashboard size yet, the whole
    // point in creating them here is to allow PlayerCardContainer to
    // anchor all controls and widgets to the correct frame.
    //
    // Note that 20 is just a random plug-in so that we can proceed with
    // control creation, the actual width is updated when setWidth() is
    // called by its graphics parent.
    //
    _m_width = G_DASHBOARD_LAYOUT.m_leftWidth + G_DASHBOARD_LAYOUT.m_rightWidth + 20;

    _createLeft();
    _createMiddle();
    _createRight();

    // only do this after you create all frames.
    _createControls();
    _createExtraButtons();

    connect(_m_avatarIcon, &GraphicsPixmapHoverItem::hover_enter, this, &Dashboard::onAvatarHoverEnter);
    connect(_m_avatarIcon, &GraphicsPixmapHoverItem::hover_leave, this, &Dashboard::onAvatarHoverLeave);
    connect(_m_avatarIcon, &GraphicsPixmapHoverItem::skin_changing_start, this, &Dashboard::onSkinChangingStart);
    connect(_m_avatarIcon, &GraphicsPixmapHoverItem::skin_changing_finished, this, &Dashboard::onSkinChangingFinished);

    connect(_m_smallAvatarIcon, &GraphicsPixmapHoverItem::hover_enter, this, &Dashboard::onAvatarHoverEnter);
    connect(_m_smallAvatarIcon, &GraphicsPixmapHoverItem::hover_leave, this, &Dashboard::onAvatarHoverLeave);
    connect(_m_smallAvatarIcon, &GraphicsPixmapHoverItem::skin_changing_start, this, &Dashboard::onSkinChangingStart);
    connect(_m_smallAvatarIcon, &GraphicsPixmapHoverItem::skin_changing_finished, this, &Dashboard::onSkinChangingFinished);

    _m_sort_menu = new QMenu(RoomSceneInstance->mainWindow());
}

bool Dashboard::isAvatarUnderMouse()
{
    return _m_avatarArea->isUnderMouse();
}

void Dashboard::hideControlButtons()
{
    m_trustButton->hide();
    m_btnReverseSelection->hide();
    m_btnSortHandcard->hide();
}

void Dashboard::showControlButtons()
{
    m_trustButton->show();
    m_trustButton->setEnabled(true);
    m_btnReverseSelection->show();
    m_btnSortHandcard->show();
}

void Dashboard::showProgressBar(QSanProtocol::Countdown countdown, int max)
{
    _m_progressBar->setCountdown(countdown, max);
    connect(_m_progressBar, SIGNAL(timedOut()), this, SIGNAL(progressBarTimedOut()));
    _m_progressBar->show();
}

QGraphicsItem *Dashboard::getMouseClickReceiver()
{
    return _m_avatarIcon;
}

void Dashboard::_createLeft()
{
    _paintLeftFrame();

    _m_leftFrame->setZValue(-1000); // nobody should be under me.
    _createEquipBorderAnimations();
}

int Dashboard::getButtonWidgetWidth() const
{
    Q_ASSERT(button_widget);
    return button_widget->boundingRect().width();
}

void Dashboard::_createMiddle()
{
    _m_middleFrame = new QGraphicsPixmapItem(_m_groupMain);
    _m_middleFrame->setTransformationMode(Qt::SmoothTransformation);

    _m_middleFrame->setZValue(-1000); // nobody should be under me.

    _m_flowerFrame = new QGraphicsPixmapItem(_m_groupMain);
    _m_flowerFrame->setTransformationMode(Qt::SmoothTransformation);
    _m_flowerFrame->setZValue(-1000);

    _m_treeFrame = new QGraphicsPixmapItem(_m_groupMain);
    _m_treeFrame->setTransformationMode(Qt::SmoothTransformation);
    _m_treeFrame->setZValue(-1001);

    button_widget->setParentItem(_m_middleFrame);

    trusting_item = new QGraphicsPathItem(this);
    trusting_text = new QGraphicsSimpleTextItem(tr("Trusting ..."), this);

    QBrush trusting_brush(G_DASHBOARD_LAYOUT.m_trustEffectColor);
    trusting_item->setBrush(trusting_brush);
    trusting_item->setOpacity(0.36);
    trusting_item->setZValue(1002.0);

    trusting_text->setFont(Config.BigFont);
    trusting_text->setBrush(Qt::white);
    trusting_text->setZValue(1002.1);

    trusting_item->hide();
    trusting_text->hide();
}

void Dashboard::_adjustComponentZValues(bool killed)
{
    PlayerCardContainer::_adjustComponentZValues(killed);
    // make sure right frame is on top because we have a lot of stuffs
    // attached to it, such as the rolecomboBox, which should not be under
    // middle frame
    if (!Config.value("UseOldButtons", false).toBool())
    {
        _layUnder(_m_rightFrame);
        _layUnder(_m_leftFrame);
        _layUnder(_m_flowerFrame);
        _layUnder(_m_middleFrame);
        _layBetween(button_widget, _m_middleFrame, _m_roleComboBox);
        _layUnder(_m_treeFrame);
        _layUnder(_m_middleFrame);
    }
    else
    {
        _layUnder(_m_rightFrame);
        _layUnder(_m_leftFrame);
        _layUnder(_m_flowerFrame);
        _layUnder(_m_middleFrame);
        _layBetween(button_widget, _m_middleFrame, _m_roleComboBox);
    }
    //_layBetween(_m_rightFrameBg, _m_faceTurnedIcon, _m_equipRegions[4]);
}

int Dashboard::width()
{
    return this->_m_width;
}

void Dashboard::repaintAll()
{
    if (!Config.value("UseOldButtons", false).toBool())
        button_widget->setPixmap(G_ROOM_SKIN.getPixmap
                             (QSanRoomSkin::S_SKIN_KEY_DASHBOARD_BUTTON_BUTTOM)
                             .scaled(G_DASHBOARD_LAYOUT.m_buttonSetSize));
    else
        button_widget->setPixmap(G_ROOM_SKIN.getPixmap
                             (QSanRoomSkin::S_SKIN_KEY_DASHBOARD_BUTTON_BUTTOM_OLD)
                             .scaled(G_DASHBOARD_LAYOUT.m_buttonSetSizeOld));
    RoomSceneInstance->redrawDashboardButtons();

    _paintLeftFrame();
    _paintRightFrame();
    _m_skillDock->update();

    PlayerCardContainer::repaintAll();
    if (NULL != m_changeHeadHeroSkinButton) {
        m_changeHeadHeroSkinButton->setPos(_dlayout->m_changeHeadHeroSkinButtonPos);
    }
    if (NULL != m_changeDeputyHeroSkinButton) {
        m_changeDeputyHeroSkinButton->setPos(_dlayout->m_changeDeputyHeroSkinButtonPos);
    }
}

void Dashboard::_createRight()
{
    _m_rightFrame = new QGraphicsPixmapItem(_m_groupMain);
    _m_rightFrame->setTransformationMode(Qt::SmoothTransformation);

    _m_rightFrame->setZValue(-1000); // nobody should be under me.
    _m_skillDock = new QSanInvokeSkillDock(_m_rightFrame);

    m_changeHeadHeroSkinButton = new QSanButton("heroskin", "change", _m_rightFrame);
    m_changeHeadHeroSkinButton->hide();
    connect(m_changeHeadHeroSkinButton, &QSanButton::clicked, this, &Dashboard::showHeroSkinList);
    connect(m_changeHeadHeroSkinButton, &QSanButton::clicked_outside, this, &Dashboard::heroSkinButtonMouseOutsideClicked);

    m_changeDeputyHeroSkinButton = new QSanButton("heroskin", "change", _m_rightFrame);
    m_changeDeputyHeroSkinButton->hide();
    connect(m_changeDeputyHeroSkinButton, &QSanButton::clicked, this, &Dashboard::showHeroSkinList);
    connect(m_changeDeputyHeroSkinButton, &QSanButton::clicked_outside, this, &Dashboard::heroSkinButtonMouseOutsideClicked);
}

void Dashboard::_updateFrames()
{
    // Here is where we adjust all frames to actual width
    QRect rect = QRect(G_DASHBOARD_LAYOUT.m_leftWidth, 0,
                       this->width() - G_DASHBOARD_LAYOUT.m_rightWidth - G_DASHBOARD_LAYOUT.m_leftWidth, G_DASHBOARD_LAYOUT.m_normalHeight);

    _paintMiddleFrame(rect);

    const int fwidth = 1000, fheight = 149;
    QRect frect = QRect(G_DASHBOARD_LAYOUT.m_leftWidth + rect.width() / 2 - fwidth / 2, rect.height() - fheight, fwidth, fheight);
    if (Config.GeneralLevel > 4)
        _paintPixmap(_m_flowerFrame, frect, _getPixmap(QSanRoomSkin::S_SKIN_KEY_FLOWERFRAME), _m_groupMain);

    const int treewidth = 106, treeheight = 168;
    QRect treerect = QRect(G_DASHBOARD_LAYOUT.m_leftWidth + rect.width() - treewidth, 1, treewidth, treeheight);
    if (!Config.value("UseOldButtons", false).toBool())
        _paintPixmap(_m_treeFrame, treerect, _getPixmap(QSanRoomSkin::S_SKIN_KEY_DASHBOARD_BUTTON_SET_BG[Config.GeneralLevel]), _m_groupMain);
    else
        _paintPixmap(_m_treeFrame, treerect, _getPixmap(QSanRoomSkin::S_SKIN_KEY_DASHBOARD_BUTTON_SET_BG_OLD[Config.GeneralLevel]), _m_groupMain);

    _m_groupDeath->setPos(rect.x(), rect.y());
    _m_groupDeath->setPixmap(QPixmap(rect.size()));

    QRect rect2 = QRect(0, 0, this->width(), G_DASHBOARD_LAYOUT.m_normalHeight);
    trusting_item->setPos(0, 0);
    trusting_text->setPos((rect2.width() - Config.BigFont.pixelSize() * 4.5) / 2,
                          (rect2.height() - Config.BigFont.pixelSize()) / 2);

    Q_ASSERT(button_widget);
    if (!Config.value("UseOldButtons", false).toBool())
    {
        const double btnLoc = 0.58328327703595;
        double btnMove = btnLoc * double(this->width());
        button_widget->setX(rect.width() - btnMove);
        button_widget->setY(-24);

        QRectF btnWidgetRect = button_widget->mapRectToItem(this, button_widget->boundingRect());
        m_btnNoNullification->setPos(btnWidgetRect.x() + btnWidgetRect.width() + 75,
                                     btnWidgetRect.y() - m_btnNoNullification->boundingRect().height());
    }
    else
    {
        button_widget->setX(rect.width() - getButtonWidgetWidth());
        button_widget->setY(1);

        QRectF btnWidgetRect = button_widget->mapRectToItem(this, button_widget->boundingRect());
        m_btnNoNullification->setPos(btnWidgetRect.left() - m_btnNoNullification->boundingRect().width(),
                                     btnWidgetRect.y() - m_btnNoNullification->boundingRect().height());
    }

    _paintRightFrame();
    _m_rightFrame->setX(_m_width - G_DASHBOARD_LAYOUT.m_rightWidth);
    _m_rightFrame->moveBy(0, m_middleFrameAndRightFrameHeightDiff);

    QPainterPath kingdomColorMaskPath;
    QRectF kingdomColorMaskRect;
    if (_m_kingdomColorMaskIcon) {
        kingdomColorMaskRect = _m_kingdomColorMaskIcon->boundingRect();
        kingdomColorMaskRect = _m_kingdomColorMaskIcon->mapRectToItem(this, kingdomColorMaskRect);
    }
    else {
        kingdomColorMaskRect = _m_rightFrame->mapRectToItem(this, _dlayout->m_kingdomMaskArea);
    }
    kingdomColorMaskRect.adjust(0, -1, 0, -2);
    kingdomColorMaskPath.addRect(kingdomColorMaskRect);

    QRectF rightFrameRect = _m_rightFrame->boundingRect();
    rightFrameRect = _m_rightFrame->mapRectToItem(this, rightFrameRect);
    QPainterPath rightFramePath;
    rightFramePath.addRect(rightFrameRect);

    QRect leftFrameAndMiddleFrameRect = QRect(0, 0, this->width() - G_DASHBOARD_LAYOUT.m_rightWidth,
                                              G_DASHBOARD_LAYOUT.m_normalHeight);
    rightFramePath.addRect(leftFrameAndMiddleFrameRect);

    trusting_item->setPath(kingdomColorMaskPath.united(rightFramePath));
}

void Dashboard::_paintLeftFrame()
{
    QRect rect = QRect(0, 0, G_DASHBOARD_LAYOUT.m_leftWidth, G_DASHBOARD_LAYOUT.m_normalHeight);
    _paintPixmap(_m_leftFrame, rect, _getPixmap(QSanRoomSkin::S_SKIN_KEY_LEFTFRAME[Config.GeneralLevel]), _m_groupMain);
}

void Dashboard::_paintMiddleFrame(const QRect &rect)
{
    _paintPixmap(_m_middleFrame, rect, _getPixmap(QSanRoomSkin::S_SKIN_KEY_MIDDLEFRAME[Config.GeneralLevel]), _m_groupMain);
}

void Dashboard::_paintRightFrame()
{
    QPixmap rightFramePixmap = _getPixmap(QSanRoomSkin::S_SKIN_KEY_RIGHTFRAME[Config.GeneralLevel]);
    int middleFrameHeight = G_DASHBOARD_LAYOUT.m_normalHeight;
    int rightFrameHeight = rightFramePixmap.height();
    m_middleFrameAndRightFrameHeightDiff = middleFrameHeight - rightFrameHeight;

    int rightFrameWidth = G_DASHBOARD_LAYOUT.m_rightWidth;

    QRect rect = QRect(_m_width - rightFrameWidth,
                       m_middleFrameAndRightFrameHeightDiff,
                       rightFrameWidth,
                       rightFrameHeight);

    _paintPixmap(_m_rightFrame, QRect(0, 0, rect.width(), rect.height()), rightFramePixmap, _m_groupMain);

    _m_skillDock->setPos(G_DASHBOARD_LAYOUT.m_skillDockLeftMargin,
                         rightFrameHeight - G_DASHBOARD_LAYOUT.m_skillDockBottomMargin);
    _m_skillDock->setWidth(rightFrameWidth - G_DASHBOARD_LAYOUT.m_skillDockRightMargin);

}

void Dashboard::setTrust(bool trust)
{
    trusting_item->setVisible(trust);
    trusting_text->setVisible(trust);
}

void Dashboard::killPlayer()
{
    trusting_item->hide();
    trusting_text->hide();
    _m_roleComboBox->fix(m_player->getRole());
    _m_roleComboBox->setEnabled(false);
    _updateDeathIcon();
    _m_saveMeIcon->hide();
    if (_m_votesItem) _m_votesItem->hide();
    if (_m_distanceItem) _m_distanceItem->hide();

    QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect();
    effect->setColor(_m_layout->m_deathEffectColor);
    effect->setStrength(1.0);
    this->setGraphicsEffect(effect);
    refresh(true);
    _m_deathIcon->show();
    if (ServerInfo.GameMode == "04_1v3" && !Self->isLord()) {
        _m_votesGot = 4;
        updateVotes(false);
    }
}

void Dashboard::revivePlayer()
{
    _m_votesGot = 0;
    updateVotes(false);
    this->setGraphicsEffect(NULL);
    Q_ASSERT(_m_deathIcon);
    _m_deathIcon->hide();
    refresh();
}

bool Dashboard::_addCardItems(QList<CardItem *> &card_items, const CardsMoveStruct &moveInfo)
{
    Player::Place place = moveInfo.to_place;
    Player::Place from_place = moveInfo.from_place;

    if (place == Player::PlaceEquip)
        addEquips(card_items);
    else if (place == Player::PlaceDelayedTrick)
        addDelayedTricks(card_items);
    else if (place == Player::PlaceHand)
        addHandCards(card_items);

    int animation_type = -1;

    if (place == Player::PlaceHand) {
        if (from_place == Player::DrawPile || from_place == Player::DrawPileBottom || from_place == Player::DiscardPile
                || from_place == Player::PlaceUnknown || from_place == Player::PlaceWuGu)
            animation_type = width() - _m_rightFrame->boundingRect().width() - _m_leftFrame->boundingRect().width();
        else if (from_place == Player::PlaceSpecial && moveInfo.from == moveInfo.to)
            animation_type = (width() - _m_rightFrame->boundingRect().width() - _m_leftFrame->boundingRect().width()) / 2;
    }
    if (place == Player::PlaceSpecial) {//just for huashen
        QPointF center = mapFromItem(_getDeathIconParent(), _getDeathIconParent()->boundingRect().center());
        _disperseCards(card_items, center, true);
    }
    adjustCards(card_items, true, animation_type);
    return place == Player::PlaceSpecial;
}

void Dashboard::addHandCards(QList<CardItem *> &card_items)
{
    foreach(CardItem *card_item, card_items)
        _addHandCard(card_item);
    updateHandcardNum();
}

void Dashboard::_addHandCard(CardItem *card_item, int index, const QString &footnote)
{
    if (ClientInstance->getStatus() == Client::Playing)
        card_item->setEnabled(card_item->getCard()->isAvailable(Self));
    else
        card_item->setEnabled(false);

    card_item->setHomeOpacity(1.0);
    card_item->setRotation(0.0);
    card_item->setFlag(ItemIsFocusable);
    if (Config.EnableSuperDrag)
        card_item->setFlag(ItemIsMovable);
    card_item->setZValue(0.1);
    if (!footnote.isEmpty()) {
        card_item->setFootnote(footnote);
        card_item->showFootnote();
    }
    if (index == 0)
        m_middleCards.append(card_item);
    else if (index > 0)
        m_rightCards.append(card_item);
    else if (index < 0)
        m_leftCards.append(card_item);

    m_handCards = m_leftCards + m_middleCards + m_rightCards;
    connect(card_item, SIGNAL(clicked()), this, SLOT(onCardItemClicked()));
    connect(card_item, SIGNAL(double_clicked()), this, SLOT(onCardItemDoubleClicked()));
    connect(card_item, SIGNAL(thrown()), this, SLOT(onCardItemThrown()));
    connect(card_item, SIGNAL(enter_hover()), this, SLOT(onCardItemHover()));
    connect(card_item, SIGNAL(leave_hover()), this, SLOT(onCardItemLeaveHover()));

	card_item->setOuterGlowEffectEnabled(true);
}

void Dashboard::selectCard(const QString &pattern, bool forward, bool multiple)
{
    if (!multiple && selected && selected->isSelected())
        selected->clickItem();

    // find all cards that match the card type
    QList<CardItem *> matches;
    foreach (CardItem *card_item, m_handCards) {
        if (card_item->isEnabled() && (pattern == "." || card_item->getCard()->match(pattern)))
            matches << card_item;
    }

    if (matches.isEmpty()) {
        if (!multiple || !selected) {
            return;
        }
    }

    int index = matches.indexOf(selected), n = matches.length();
    index = (index + (forward ? 1 : n - 1)) % n;

    CardItem *to_select = matches[index];
    if (!to_select->isSelected())
        to_select->clickItem();
    else if (to_select->isSelected() && (!multiple || (multiple && to_select != selected)))
        to_select->clickItem();
    selected = to_select;

    _adjustCards();
    foreach(CardItem *card, m_handCards)
        card->goBack(true, true, -1, 200);
}

void Dashboard::selectEquip(int position)
{
    int i = position - 1;
    if (_m_equipCards[i] && _m_equipCards[i]->isMarkable()) {
        _m_equipCards[i]->mark(!_m_equipCards[i]->isMarked());
        update();
    }
}

void Dashboard::selectOnlyCard(bool need_only)
{
    if (selected && selected->isSelected())
        selected->clickItem();

    int count = 0;

    QList<CardItem *> items;
    foreach (CardItem *card_item, m_handCards) {
        if (card_item->isEnabled()) {
            items << card_item;
            count++;
            if (need_only && count > 1) {
                unselectAll();
                return;
            }
        }
    }

    QList<int> equip_pos;
    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (_m_equipCards[i] && _m_equipCards[i]->isMarkable()) {
            equip_pos << i;
            count++;
            if (need_only && count > 1) return;
        }
    }
    if (count == 0) return;
    if (!items.isEmpty()) {
        CardItem *item = items.first();
        item->clickItem();
        selected = item;
        adjustCards();
    } else if (!equip_pos.isEmpty()) {
        int pos = equip_pos.first();
        _m_equipCards[pos]->mark(!_m_equipCards[pos]->isMarked());
        update();
    }
}

const Card *Dashboard::getSelected() const
{
    if (view_as_skill) {
        const Card *pendingcard = pending_card;
        const SkillCard *skill_card = qobject_cast<const SkillCard *>(pendingcard);
        if (skill_card != NULL){
            SkillCard *card = const_cast<SkillCard *>(skill_card);
            if (card && card->getUserString().isEmpty()) {
                QString select_option = Self->tag[card->getSkillName()].toString();
                if (!select_option.isEmpty())
                    card->setUserString(select_option);
            }
            return card;
        }
        return pending_card;
    } else if (selected)
        return selected->getCard();
    else
        return NULL;
}

void Dashboard::selectCard(CardItem *item, bool isSelected)
{
    bool oldState = item->isSelected();
    if (oldState == isSelected) return;
    m_mutex.lock();

    item->setSelected(isSelected);
    QPointF oldPos = item->homePos();
    QPointF newPos = oldPos;
    newPos.setY(newPos.y() + (isSelected ? 1 : -1) * S_PENDING_OFFSET_Y);
    item->setHomePos(newPos);
    selected = item;

    m_mutex.unlock();
}

void Dashboard::unselectAll(const CardItem *except, bool update_skill)
{
    selected = NULL;

    foreach (CardItem *card_item, m_handCards) {
        if (card_item != except)
            selectCard(card_item, false);
    }

    adjustCards(true);
    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (_m_equipCards[i] && _m_equipCards[i] != except)
            _m_equipCards[i]->mark(false);
    }
    if (view_as_skill && update_skill) {
        pendings.clear();
        updatePending();
    }
}

QRectF Dashboard::boundingRect() const
{
    return QRectF(0, 0, _m_width, _m_layout->m_normalHeight);
}

void Dashboard::setWidth(int width)
{
    prepareGeometryChange();
    adjustCards(true);
    this->_m_width = width;
    _updateFrames();
    _updateDeathIcon();
}

QSanSkillButton *Dashboard::addSkillButton(const QString &skillName)
{
    // if it's a equip skill, add it to equip bar
    _mutexEquipAnim.lock();

    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (!_m_equipCards[i]) continue;
        const EquipCard *equip = qobject_cast<const EquipCard *>(_m_equipCards[i]->getCard()->getRealCard());
        Q_ASSERT(equip);
        // @todo: we must fix this in the server side - add a skill to the card itself instead
        // of getting it from the engine.
        const Skill *skill = Sanguosha->getSkill(equip);
        if (skill == NULL) continue;
        if (skill->objectName() == skillName) {
            // If there is already a button there, then we haven't removed the last skill before attaching
            // a new one. The server must have sent the requests out of order. So crash.
            Q_ASSERT(_m_equipSkillBtns[i] == NULL);
            _m_equipSkillBtns[i] = new QSanInvokeSkillButton(this);
            _m_equipSkillBtns[i]->setSkill(skill);
            _m_equipSkillBtns[i]->setVisible(false);
            connect(_m_equipSkillBtns[i], SIGNAL(clicked()), this, SLOT(_onEquipSelectChanged()));
            connect(_m_equipSkillBtns[i], SIGNAL(enable_changed()), this, SLOT(_onEquipSelectChanged()));
            QSanSkillButton *btn = _m_equipSkillBtns[i];
            _mutexEquipAnim.unlock();
            return btn;
        }
    }
    _mutexEquipAnim.unlock();
#ifndef QT_NO_DEBUG
    const Skill *skill = Sanguosha->getSkill(skillName);
    Q_ASSERT(skill && !skill->inherits("WeaponSkill") && !skill->inherits("ArmorSkill") && !skill->inherits("TreasureSkill"));
#endif
    return _m_skillDock->addSkillButtonByName(skillName);
}

QSanSkillButton *Dashboard::removeSkillButton(const QString &skillName)
{
    QSanSkillButton *btn = NULL;
    _mutexEquipAnim.lock();
    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (!_m_equipSkillBtns[i]) continue;
        const Skill *skill = _m_equipSkillBtns[i]->getSkill();
        Q_ASSERT(skill != NULL);
        if (skill->objectName() == skillName) {
            btn = _m_equipSkillBtns[i];
            _m_equipSkillBtns[i] = NULL;
            continue;
        }
    }
    _mutexEquipAnim.unlock();
    if (btn == NULL) {
        btn = _m_skillDock->removeSkillButtonByName(skillName);
    }
    return btn;
}

void Dashboard::highlightEquip(QString skillName, bool highlight)
{
    int i = 0;
    for (i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (!_m_equipCards[i]) continue;
        if (_m_equipCards[i]->getCard()->objectName() == skillName)
            break;
    }
    if (i != 5)
        _setEquipBorderAnimation(i, highlight);
}

void Dashboard::clearHighlightEquip()
{
    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++)
        if (!_m_equipCards[i])
        {
            _m_equipBorders[i]->hide();
            _m_equipBorders[i]->stop();
        }
}

void Dashboard::_createExtraButtons()
{
    m_trustButton = new QSanButton("handcard", "trust", this, true);
    m_trustButton->setStyle(QSanButton::S_STYLE_TOGGLE);
    m_btnReverseSelection = new QSanButton("handcard", "reverse-selection", this);
    m_btnSortHandcard = new QSanButton("handcard", "sort", this);
    m_btnNoNullification = new QSanButton("handcard", "nullification", this);
    m_btnNoNullification->setStyle(QSanButton::S_STYLE_TOGGLE);
    // @todo: auto hide.
    qreal pos = 0, height = 24-m_trustButton->boundingRect().height();
    m_trustButton->setPos(pos, height);
    pos += m_trustButton->boundingRect().right();
    m_btnReverseSelection->setPos(pos, height);
    pos += m_btnReverseSelection->boundingRect().right();
    m_btnSortHandcard->setPos(pos, height);

    m_btnNoNullification->hide();
    connect(m_trustButton, &QSanButton::clicked, RoomSceneInstance, &RoomScene::trust);
    connect(Self, &ClientPlayer::state_changed, this, &Dashboard::updateTrustButton);
    connect(m_btnReverseSelection, SIGNAL(clicked()), this, SLOT(reverseSelection()));
    connect(m_btnSortHandcard, SIGNAL(clicked()), this, SLOT(sortCards()));
    connect(m_btnNoNullification, SIGNAL(clicked()), this, SLOT(cancelNullification()));
    m_trustButton->setEnabled(false);
}

void Dashboard::updateSkillButton(const QString &skillName)
{
    _m_skillDock->updateSkillButtonByName(skillName);
}

void Dashboard::showSeat()
{
    const QRect region = G_DASHBOARD_LAYOUT.m_seatIconRegion;
    PixmapAnimation *pma = PixmapAnimation::GetPixmapAnimation(_m_rightFrame, "seat");
    if (pma) {
        pma->setTransform(QTransform::fromTranslate(-pma->boundingRect().width() / 2, -pma->boundingRect().height() / 2));
        pma->setPos(region.x() + region.width() / 2, region.y() + region.height() / 2);
    }
    _paintPixmap(_m_seatItem, region,
        _getPixmap(QSanRoomSkin::S_SKIN_KEY_SEAT_NUMBER, QString::number(m_player->getRealSeat())),
        _m_rightFrame);
    //save the seat number for later use
    m_player->setProperty("UI_Seat", m_player->getSeat());
    _m_seatItem->setZValue(1.1);
}

void Dashboard::skillButtonActivated()
{
    if (RoomSceneInstance->current_select_box != NULL)
        RoomSceneInstance->current_select_box->clear();
    if (RoomSceneInstance->current_guhuo_box != NULL)
        RoomSceneInstance->current_guhuo_box->clear();

    QSanSkillButton *button = qobject_cast<QSanSkillButton *>(sender());
    foreach (QSanSkillButton *btn, _m_skillDock->getAllSkillButtons()) {
        if (button == btn) continue;

        if (btn->getViewAsSkill() != NULL && btn->isDown())
            btn->setState(QSanButton::S_STATE_UP);
    }

    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (button == _m_equipSkillBtns[i]) continue;

        if (_m_equipSkillBtns[i] != NULL)
            _m_equipSkillBtns[i]->setEnabled(false);
    }
}

void Dashboard::skillButtonDeactivated()
{
    if (RoomSceneInstance->current_select_box != NULL)
        RoomSceneInstance->current_select_box->clear();
    if (RoomSceneInstance->current_guhuo_box != NULL)
        RoomSceneInstance->current_guhuo_box->clear();

    foreach (QSanSkillButton *btn, _m_skillDock->getAllSkillButtons()) {
        if (btn && btn->getViewAsSkill() != NULL && btn->isDown())
            btn->setState(QSanButton::S_STATE_UP);
    }

    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (_m_equipSkillBtns[i] != NULL) {
            _m_equipSkillBtns[i]->setEnabled(true);
            if (_m_equipSkillBtns[i]->isDown())
                _m_equipSkillBtns[i]->click();
        }
    }
}

void Dashboard::selectAll()
{
    foreach (const QString &pile, Self->getPileNames()) {
        if (pile.startsWith("&") || pile == "wooden_ox")
            retractPileCards(pile);
    }
    if (view_as_skill) {
        unselectAll();
        foreach (CardItem *card_item, m_handCards) {
            selectCard(card_item, true);
            pendings << card_item;
        }
        updatePending();
    }
    adjustCards(true);
}

void Dashboard::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
}

void Dashboard::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    PlayerCardContainer::mouseReleaseEvent(mouseEvent);

    CardItem *to_select = NULL;
    int i;
    for (i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (_m_equipRegions[i]->isUnderMouse()) {
            to_select = _m_equipCards[i];
            break;
        }
    }
    if (!to_select) return;
    if (_m_equipSkillBtns[i] != NULL && _m_equipSkillBtns[i]->isEnabled())
        _m_equipSkillBtns[i]->click();
    else if (to_select->isMarkable()) {
        // According to the game rule, you cannot select a weapon as a card when
        // you are invoking the skill of that equip. So something must be wrong.
        // Crash.
        Q_ASSERT(_m_equipSkillBtns[i] == NULL || !_m_equipSkillBtns[i]->isDown());
        to_select->mark(!to_select->isMarked());
        update();
    }
}

void Dashboard::_onEquipSelectChanged()
{
    QSanSkillButton *btn = qobject_cast<QSanSkillButton *>(sender());
    if (btn) {
        for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
            if (_m_equipSkillBtns[i] == btn) {
                _setEquipBorderAnimation(i, btn->isDown());
                break;
            }
        }
    } else {
        CardItem *equip = qobject_cast<CardItem *>(sender());
        // Do not remove this assertion. If equip is NULL here, some other
        // sources that could select equip has not been considered and must
        // be implemented.
        Q_ASSERT(equip);
        for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
            if (_m_equipCards[i] == equip) {
                _setEquipBorderAnimation(i, equip->isMarked());
                break;
            }
        }
    }
}

void Dashboard::_createEquipBorderAnimations()
{
    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        _m_equipBorders[i] = new PixmapAnimation();
        _m_equipBorders[i]->setParentItem(_getEquipParent());
        _m_equipBorders[i]->setPath("image/system/emotion/equipborder/");
        if (!_m_equipBorders[i]->valid()) {
            delete _m_equipBorders[i];
            _m_equipBorders[i] = NULL;
            continue;
        }
        _m_equipBorders[i]->setPos(_dlayout->m_equipBorderPos + _dlayout->m_equipSelectedOffset + _dlayout->m_equipAreas[i].topLeft());
        _m_equipBorders[i]->hide();
    }
}

void Dashboard::_setEquipBorderAnimation(int index, bool turnOn)
{
    _mutexEquipAnim.lock();
    if (_m_isEquipsAnimOn[index] == turnOn) {
        _mutexEquipAnim.unlock();
        return;
    }

    QRect rect = _m_layout->m_equipAreas[index];
    if (m_player && m_player->getTreasure() && (index == 2 || index == 3))
        rect = _m_layout->m_horseAreas[index-2];
    QPoint newPos;
    if (turnOn)
        newPos = _dlayout->m_equipSelectedOffset + rect.topLeft();
    else
        newPos = rect.topLeft();

    _m_equipAnim[index]->stop();
    _m_equipAnim[index]->clear();
    QPropertyAnimation *anim = new QPropertyAnimation(_m_equipRegions[index], "pos");
    anim->setEndValue(newPos);
    anim->setDuration(200);
    _m_equipAnim[index]->addAnimation(anim);
    connect(anim, SIGNAL(finished()), anim, SLOT(deleteLater()));
    anim = new QPropertyAnimation(_m_equipRegions[index], "opacity");
    anim->setEndValue(255);
    anim->setDuration(200);
    _m_equipAnim[index]->addAnimation(anim);
    connect(anim, SIGNAL(finished()), anim, SLOT(deleteLater()));
    _m_equipAnim[index]->start();

    Q_ASSERT(_m_equipBorders[index]);
    if (turnOn) {
        _m_equipBorders[index]->setPos(_dlayout->m_equipBorderPos + _dlayout->m_equipSelectedOffset + rect.topLeft());
        _m_equipBorders[index]->show();
        _m_equipBorders[index]->start();
    } else {
        _m_equipBorders[index]->hide();
        _m_equipBorders[index]->stop();
    }

    _m_isEquipsAnimOn[index] = turnOn;
    _mutexEquipAnim.unlock();
}

void Dashboard::showHeroSkinListHelper(const General *general,
    HeroSkinContainer * &heroSkinContainer)
{
    if (NULL == general) {
        return;
    }

    QString generalName = general->objectName();
    if (NULL == heroSkinContainer || heroSkinContainer->getGeneralName() != generalName) {
        heroSkinContainer = RoomSceneInstance->findHeroSkinContainer(generalName);

        if (NULL == heroSkinContainer) {
            heroSkinContainer = new HeroSkinContainer(generalName, general->getKingdom());

            RoomSceneInstance->addHeroSkinContainer(heroSkinContainer);
            RoomSceneInstance->addItem(heroSkinContainer);

            heroSkinContainer->setPos(getHeroSkinContainerPosition());
            RoomSceneInstance->bringToFront(heroSkinContainer);
        }
    }

    if (!heroSkinContainer->isVisible()) {
        heroSkinContainer->show();
    }

    heroSkinContainer->bringToTopMost();
}

QPointF Dashboard::getHeroSkinContainerPosition() const
{
    QRectF avatarParentRect = _m_rightFrame->sceneBoundingRect();
    QRectF heroSkinContainerRect = (m_headHeroSkinContainer != NULL)
        ? m_headHeroSkinContainer->boundingRect()
        : m_deputyHeroSkinContainer->boundingRect();;
    return QPointF(avatarParentRect.left() - heroSkinContainerRect.width() - 120,
        avatarParentRect.bottom() - heroSkinContainerRect.height() - 5);
}

void Dashboard::adjustCards(bool playAnimation)
{
    _adjustCards();
    foreach(CardItem *card, m_handCards)
        card->goBack(playAnimation, true, -1, 500);
}

void Dashboard::adjustCards(QList<CardItem *> &cards, bool playAnimation, int animation_type)
{
    _adjustCards();
	foreach(CardItem *card, m_handCards)
        card->goBack(playAnimation, true, cards.contains(card) ? animation_type : -1, 500);
}

void Dashboard::_adjustCards()
{
    int n = m_handCards.length();
    if (n == 0) return;

    QSanRoomSkin::DashboardLayout *layout = (QSanRoomSkin::DashboardLayout *)_m_layout;
    int leftWidth = layout->m_leftWidth;
    int cardHeight = G_COMMON_LAYOUT.m_cardNormalHeight;
    int middleWidth = _m_width - layout->m_leftWidth - layout->m_rightWidth - this->getButtonWidgetWidth();
    QRect rowRect = QRect(leftWidth, layout->m_normalHeight - cardHeight - 3, middleWidth, cardHeight);

    _m_highestZ = 0;

    int index = -1;
    for (int i = 0; i < n; i++) {
        CardItem *card = m_handCards[i];
        if (card->isHovered()) {
            index = i;
            break;
        }
    }

    _disperseCards(m_handCards, rowRect, Qt::AlignLeft, true, true, index);

    for (int i = 0; i < n; i++) {
        CardItem *card = m_handCards[i];
        if (card->isSelected()) {
            QPointF newPos = card->homePos();
            newPos.setY(newPos.y() + S_PENDING_OFFSET_Y);
            card->setHomePos(newPos);
        }
    }
}

int Dashboard::getMiddleWidth()
{
    return _m_width - G_DASHBOARD_LAYOUT.m_leftWidth - G_DASHBOARD_LAYOUT.m_rightWidth;
}

QList<CardItem *> Dashboard::cloneCardItems(QList<int> card_ids)
{
    QList<CardItem *> result;
    CardItem *card_item = NULL;
    CardItem *new_card = NULL;

    foreach (int card_id, card_ids) {
        card_item = CardItem::FindItem(m_handCards, card_id);
        new_card = _createCard(card_id);
        Q_ASSERT(card_item);
        if (card_item) {
            new_card->setPos(card_item->homePos());
            new_card->setHomePos(card_item->homePos());
        }
        result.append(new_card);
    }
    return result;
}

QList<CardItem *> Dashboard::removeHandCards(const QList<int> &card_ids)
{
    QList<CardItem *> result;
    CardItem *card_item = NULL;
    foreach (int card_id, card_ids) {
        card_item = CardItem::FindItem(m_handCards, card_id);
        if (card_item == selected) selected = NULL;
        Q_ASSERT(card_item);
        if (card_item) {
            animations->effectOut(card_item);
            m_leftCards.removeOne(card_item);
            m_middleCards.removeOne(card_item);
            m_rightCards.removeOne(card_item);
            m_handCards.removeOne(card_item);
            card_item->disconnect(this);
			card_item->setOuterGlowEffectEnabled(false);
            result.append(card_item);
        }
    }
    updateHandcardNum();
    return result;
}

QList<CardItem *> Dashboard::removeCardItems(const QList<int> &card_ids, const CardsMoveStruct &moveInfo)
{
    Player::Place place = moveInfo.from_place;
    Player::Place to_place = moveInfo.to_place;
    CardItem *card_item = NULL;
    QList<CardItem *> result;
    if (place == Player::PlaceHand) {
        result = removeHandCards(card_ids);
        adjustCards();
    }else if (place == Player::PlaceEquip)
        result = removeEquips(card_ids);
    else if (place == Player::PlaceDelayedTrick)
        result = removeDelayedTricks(card_ids);
    else if (place == Player::PlaceSpecial) {
        if (to_place == Player::PlaceSpecial)
            return QList<CardItem *>();
        foreach (int card_id, card_ids) {
            card_item = _createCard(card_id);
            card_item->setOpacity(0.0);
            result.push_back(card_item);
        }
    } else
        Q_ASSERT(false);

    Q_ASSERT(result.size() == card_ids.size());

    if (to_place == Player::PlaceSpecial) {
        foreach (CardItem *card_item, result) {
            Q_ASSERT(card_item);
            if (card_item) {
                result.removeOne(card_item);
                card_item->setVisible(false);
                QTimer::singleShot(2000, card_item, SLOT(deleteLater()));
            }
        }
        return QList<CardItem *>();
    }
    if (place == Player::PlaceSpecial || (to_place != Player::DiscardPile && to_place != Player::PlaceJudge
           && to_place != Player::DrawPile && to_place != Player::DrawPileBottom && to_place != Player::PlaceTable)) {
        QPointF center = mapFromItem(_getDeathIconParent(), _getDeathIconParent()->boundingRect().center());
        _disperseCards(result, center, false);
    }
    update();
    return result;
}

void Dashboard::updateAvatar()
{
    PlayerCardContainer::updateAvatar();
    if (m_headHeroSkinContainer)
        m_headHeroSkinContainer->hide();
    if (m_deputyHeroSkinContainer)
        m_deputyHeroSkinContainer->hide();
    _m_screenNameItem->hide();

    QPixmap rightFramePixmap = _getPixmap(QSanRoomSkin::S_SKIN_KEY_RIGHTFRAME[Config.GeneralLevel]);
    int rightFrameWidth = G_DASHBOARD_LAYOUT.m_rightWidth;
    int rightFrameHeight = rightFramePixmap.height();
    _paintPixmap(_m_backgroundFrame, QRect(0, 0, rightFrameWidth, rightFrameHeight), rightFramePixmap, _m_rightFrame);

    _m_skillDock->update();
}

static bool CompareByNumber(const CardItem *a, const CardItem *b)
{
    return Card::CompareByNumber(a->getCard(), b->getCard());
}

static bool CompareBySuit(const CardItem *a, const CardItem *b)
{
    return Card::CompareBySuit(a->getCard(), b->getCard());
}

static bool CompareByType(const CardItem *a, const CardItem *b)
{
    return Card::CompareByType(a->getCard(), b->getCard());
}

void Dashboard::sortCards()
{
    if (m_handCards.length() == 0) return;

    QMenu *menu = _m_sort_menu;
    menu->clear();
    menu->setTitle(tr("Sort handcards"));

    QAction *action1 = menu->addAction(tr("Sort by type"));
    action1->setData((int)ByType);

    QAction *action2 = menu->addAction(tr("Sort by suit"));
    action2->setData((int)BySuit);

    QAction *action3 = menu->addAction(tr("Sort by number"));
    action3->setData((int)ByNumber);

    connect(action1, SIGNAL(triggered()), this, SLOT(beginSorting()));
    connect(action2, SIGNAL(triggered()), this, SLOT(beginSorting()));
    connect(action3, SIGNAL(triggered()), this, SLOT(beginSorting()));

    QPointF posf = QCursor::pos();
    menu->popup(QPoint(posf.x(), posf.y()));
}

void Dashboard::beginSorting()
{
    QAction *action = qobject_cast<QAction *>(sender());
    SortType type = ByType;
    if (action)
        type = (SortType)(action->data().toInt());

    switch (type) {
    case ByType: qSort(m_middleCards.begin(), m_middleCards.end(), CompareByType); break;
    case BySuit: qSort(m_middleCards.begin(), m_middleCards.end(), CompareBySuit); break;
    case ByNumber: qSort(m_middleCards.begin(), m_middleCards.end(), CompareByNumber); break;
    default: Q_ASSERT(false);
    }

    m_handCards = m_leftCards + m_middleCards + m_rightCards;
    adjustCards();
}

void Dashboard::reverseSelection()
{
    if (!view_as_skill) return;

    QList<CardItem *> selected_items;
    foreach(CardItem *item, m_handCards)
        if (item->isSelected()) {
            item->clickItem();
            selected_items << item;
        }
    foreach(CardItem *item, m_handCards)
        if (item->isEnabled() && !selected_items.contains(item))
            item->clickItem();
    adjustCards();
}


void Dashboard::cancelNullification()
{
    ClientInstance->m_noNullificationThisTime = !ClientInstance->m_noNullificationThisTime;
    if (Sanguosha->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE
        && Sanguosha->getCurrentCardUsePattern() == "nullification"
        && RoomSceneInstance->isCancelButtonEnabled()) {
        RoomSceneInstance->doCancelButton();
    }
}

void Dashboard::controlNullificationButton(bool show)
{
    if (ClientInstance->getReplayer()) return;
    m_btnNoNullification->setState(QSanButton::S_STATE_UP);
    m_btnNoNullification->setVisible(show);
}

void Dashboard::disableAllCards()
{
    m_mutexEnableCards.lock();
    foreach(CardItem *card_item, m_handCards)
        card_item->setEnabled(false);
    m_mutexEnableCards.unlock();
}

void Dashboard::enableCards()
{
    m_mutexEnableCards.lock();
    foreach(CardItem *card_item, m_handCards)
        card_item->setEnabled(card_item->getCard()->isAvailable(Self));
    m_mutexEnableCards.unlock();
}

void Dashboard::enableAllCards()
{
    m_mutexEnableCards.lock();
    foreach(CardItem *card_item, m_handCards)
        card_item->setEnabled(true);
    m_mutexEnableCards.unlock();
}

void Dashboard::startPending(const ViewAsSkill *skill)
{
    m_mutexEnableCards.lock();
    view_as_skill = skill;
    pendings.clear();
    unselectAll();

    retractAllSkillPileCards();

    if (skill && !skill->getExpandPile().isEmpty()) {
        foreach(const QString &pile_name, skill->getExpandPile().split(","))
            expandPileCards(pile_name, false);
    }

    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        if (_m_equipCards[i] != NULL)
            connect(_m_equipCards[i], SIGNAL(mark_changed()), this, SLOT(onMarkChanged()));
    }

    QString guhuo_type = skill->getGuhuoType();
    expandGuhuoCards(guhuo_type);
    updatePending();
    m_mutexEnableCards.unlock();
}

void Dashboard::stopPending()
{
    m_mutexEnableCards.lock();

    if (view_as_skill && !view_as_skill->getExpandPile().isEmpty())
        retractPileCards(view_as_skill->getExpandPile());
    retractGuhuoCards();

    view_as_skill = NULL;
    pending_card = NULL;
    selected = NULL;

    emit card_selected(NULL);

    foreach (CardItem *item, m_handCards) {
        item->setEnabled(false);
        animations->effectOut(item);
    }

    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        CardItem *equip = _m_equipCards[i];
        if (equip != NULL) {
            equip->mark(false);
            equip->setMarkable(false);
            _m_equipRegions[i]->setOpacity(1.0);
            equip->setEnabled(false);
            disconnect(equip, SIGNAL(mark_changed()));
        }
    }
    pendings.clear();
    adjustCards(true);
    m_mutexEnableCards.unlock();
}

void Dashboard::expandGuhuoCards(const QString &guhuo_type)
{
    if (!_m_guhuo_expanded.isEmpty()) return;
    QStringList card_list;
    QList<int> modecard = Sanguosha->getRandomCards();
    if (guhuo_type.contains("b")) {
        QList<const BasicCard*> basics = Sanguosha->findChildren<const BasicCard*>();
        foreach (const BasicCard *card, basics) {
            if (!card_list.contains(card->objectName()) && modecard.contains(card->getId())
                && !(guhuo_type.contains("s") && card_list.contains("slash") && card->objectName().contains("slash")))
                card_list.append(card->objectName());
        }
    }
    if (guhuo_type.contains("t")) {
        QList<const TrickCard*> tricks = Sanguosha->findChildren<const TrickCard*>();
        foreach (const TrickCard *card, tricks) {
            if (modecard.contains(card->getId()) && card->isNDTrick()
                && !card_list.contains(card->objectName()))
                    card_list.append(card->objectName());
        }
    }
    if (guhuo_type.contains("d")) {
        QList<const DelayedTrick*> delays = Sanguosha->findChildren<const DelayedTrick*>();
        foreach (const DelayedTrick *card, delays) {
            if (!card_list.contains(card->objectName()) && modecard.contains(card->getId()))
                card_list.append(card->objectName());
        }
    }
    QList<CardItem *> card_items;
    foreach (QString card_name, card_list) {
        Card *card = Sanguosha->cloneCard(card_name, Card::NoSuit, 0);
        if (card) {
            CardItem *item = new CardItem(card);
            item->setOpacity(0.0);
            card_items.append(item);
        }
    }
    foreach (CardItem *card_item, card_items) {
        card_item->setPos(mapFromScene(card_item->scenePos()));
        card_item->setParentItem(this);
    }

    foreach(CardItem *card_item, card_items)
        _addHandCard(card_item, 1);

    int animation_type = width() - _m_rightFrame->boundingRect().width() - _m_leftFrame->boundingRect().width();
    adjustCards(card_items, true, animation_type);
    update();
    _m_guhuo_expanded = card_items;
}

void Dashboard::retractGuhuoCards()
{
    if (_m_guhuo_expanded.isEmpty())
        return;
    foreach (CardItem *card_item, _m_guhuo_expanded) {
        if (card_item == selected) selected = NULL;
        Q_ASSERT(card_item);
        if (card_item) {
            _m_guhuo_expanded.removeOne(card_item);
            m_rightCards.removeOne(card_item);
            m_handCards.removeOne(card_item);
            card_item->disconnect(this);
            card_item->setOuterGlowEffectEnabled(false);
            delete card_item;
        }
    }
    adjustCards();
    update();
}

void Dashboard::expandPileCards(const QString &pile_name, bool prepend)
{
    if (_m_pile_expanded.contains(pile_name)) return;
    QString new_name = pile_name;
    QList<int> pile;
    if (new_name.startsWith("%")) {
        new_name = new_name.mid(1);
        foreach(const Player *p, Self->getAliveSiblings())
            pile += p->getPile(new_name);
    } else {
        pile = Self->getPile(new_name);
    }
    if (pile.isEmpty()) return;
    QList<CardItem *> card_items = _createCards(pile);
    foreach (CardItem *card_item, card_items) {
        card_item->setPos(mapFromScene(card_item->scenePos()));
        card_item->setParentItem(this);
    }

    foreach(CardItem *card_item, card_items)
        _addHandCard(card_item, 1, Sanguosha->translate(new_name));

    int animation_type = -1;
    if (!prepend)
        animation_type = width() - _m_rightFrame->boundingRect().width() - _m_leftFrame->boundingRect().width();

    adjustCards(card_items, true, animation_type);
    update();
	_m_pile_expanded[pile_name] = pile;
}

void Dashboard::retractPileCards(const QString &pile_name)
{
    if (!_m_pile_expanded.contains(pile_name)) return;
    QString new_name = pile_name;
    QList<int> pile = _m_pile_expanded.value(new_name);
	_m_pile_expanded.remove(pile_name);
    if (pile.isEmpty()) return;
    CardItem *card_item;
    foreach (int card_id, pile) {
        card_item = CardItem::FindItem(m_handCards, card_id);
        if (card_item == selected) selected = NULL;
        Q_ASSERT(card_item);
        if (card_item) {
            m_leftCards.removeOne(card_item);
            m_middleCards.removeOne(card_item);
            m_rightCards.removeOne(card_item);
            m_handCards.removeOne(card_item);
            card_item->disconnect(this);
            card_item->setOuterGlowEffectEnabled(false);
            delete card_item;
            card_item = NULL;
        }
    }
    adjustCards();
    update();
}

void Dashboard::updateHandPile(const QString &pile_name, bool add, QList<int> card_ids)
{
	QList<int> pile;
	if (_m_pile_expanded.contains(pile_name))
		pile = _m_pile_expanded.value(pile_name);
	if (add) {
		QList<int> to_append;
		foreach (int card_id, card_ids) {
			if (pile.contains(card_id)) continue;
			pile.append(card_id);
			to_append.append(card_id);
		}
		QList<CardItem *> card_items = _createCards(to_append);
		foreach (CardItem *card_item, card_items) {
			card_item->setPos(mapFromScene(card_item->scenePos()));
			card_item->setParentItem(this);
		}
        foreach(CardItem *card_item, card_items)
            _addHandCard(card_item, -1, Sanguosha->translate(pile_name));
        adjustCards(card_items, true);
	} else {
		CardItem *card_item;
		foreach (int card_id, card_ids) {
			if (!pile.contains(card_id)) continue;
			pile.removeOne(card_id);
			card_item = CardItem::FindItem(m_handCards, card_id);
			if (card_item == selected) selected = NULL;
			Q_ASSERT(card_item);
			if (card_item) {
                m_leftCards.removeOne(card_item);
                m_middleCards.removeOne(card_item);
                m_rightCards.removeOne(card_item);
				m_handCards.removeOne(card_item);
				card_item->disconnect(this);
				delete card_item;
				card_item = NULL;
			}
		}
		adjustCards();
	}
    update();
	if (pile.isEmpty())
		_m_pile_expanded.remove(pile_name);
	else
		_m_pile_expanded[pile_name] = pile;
}

void Dashboard::retractAllSkillPileCards()
{
    foreach (const QString &pileName, _m_pile_expanded.keys()) {
        if (!(pileName.startsWith("&") || pileName == "wooden_ox"))
            retractPileCards(pileName);
    }
    retractGuhuoCards();
}

void Dashboard::onCardItemClicked()
{
    CardItem *card_item = qobject_cast<CardItem *>(sender());
    if (!card_item) return;

    if (view_as_skill) {
        if (card_item->isSelected()) {
            selectCard(card_item, false);
            pendings.removeOne(card_item);
        } else {
            if (view_as_skill->inherits("OneCardViewAsSkill"))
                unselectAll();
            selectCard(card_item, true);
            pendings << card_item;
        }

        updatePending();
    } else {
        if (card_item->isSelected()) {
            unselectAll();
            emit card_selected(NULL);
        } else {
            unselectAll();
            selectCard(card_item, true);
            selected = card_item;

            emit card_selected(selected->getCard());
        }
    }
}

void Dashboard::updatePending()
{
    if (!view_as_skill) return;
    QList<const Card *> cards;
    foreach(CardItem *item, pendings)
        cards.append(item->getCard());

    bool expand = view_as_skill->isResponseOrUse();
    if (!expand && view_as_skill->inherits("ResponseSkill")) {
        const ResponseSkill *resp_skill = qobject_cast<const ResponseSkill *>(view_as_skill);
        if (resp_skill && (resp_skill->getRequest() == Card::MethodResponse || resp_skill->getRequest() == Card::MethodUse))
            expand = true;
    }

    QList<const Card *> pended;
    if (!view_as_skill->inherits("OneCardViewAsSkill"))
        pended = cards;
    foreach (CardItem *item, m_handCards) {
        if (!item->isSelected() || pendings.isEmpty()){
			bool not_handcard_pile = true;
			foreach (const QString &pileName, _m_pile_expanded.keys()) {
                if ((pileName.startsWith("&") || pileName == "wooden_ox")){
					QList<int> pile = _m_pile_expanded.value(pileName);
                    if (pile.contains(item->getId())){
                        not_handcard_pile = false;
                        break;
                    }
                }
			}
			
			item->setEnabled((expand || not_handcard_pile) && view_as_skill->viewFilter(pended, item->getCard()));
		}
        if (!item->isEnabled())
            animations->effectOut(item);
    }

    for (int i = 0; i < S_EQUIP_AREA_LENGTH; i++) {
        CardItem *equip = _m_equipCards[i];
        if (equip && !equip->isMarked())
            equip->setMarkable(view_as_skill->viewFilter(pended, equip->getCard()));
        if (equip) {
            if (!equip->isMarkable() && (!_m_equipSkillBtns[i] || !_m_equipSkillBtns[i]->isEnabled()))
                _m_equipRegions[i]->setOpacity(0.7);
            else
                _m_equipRegions[i]->setOpacity(1.0);
        }
    }

    const Card *new_pending_card = view_as_skill->viewAs(cards);
    if (pending_card != new_pending_card) {
        if (pending_card && !pending_card->parent() && pending_card->isVirtualCard()) {
            delete pending_card;
            pending_card = NULL;
        }

        pending_card = new_pending_card;

        QString skill_name = view_as_skill->objectName();
        const Skill *skill = Sanguosha->getSkill(skill_name);
        QMap<QString, GuhuoBox *> guhuo_items = RoomSceneInstance->getGuhuoItems();
        GuhuoBox *guhuo = guhuo_items[skill_name];
        if (guhuo != NULL && skill != NULL && skill->buttonEnabled()) {
            Self->tag.remove(skill_name);
            if (new_pending_card == NULL)
                guhuo->clear();
            else {
                guhuo->popup();
                return;
            }
        }
        emit card_selected(pending_card);
    }
}

void Dashboard::clearPendings()
{
    selected = NULL;
    foreach(CardItem *item, m_handCards)
        selectCard(item, false);
    pendings.clear();
}

void Dashboard::onCardItemDoubleClicked()
{
    if (!Config.EnableDoubleClick) return;
    CardItem *card_item = qobject_cast<CardItem *>(sender());
    if (card_item) {
        if (!view_as_skill) selected = card_item;
        animations->effectOut(card_item);
        emit card_to_use();
    }
}

void Dashboard::onCardItemThrown()
{
    CardItem *card_item = qobject_cast<CardItem *>(sender());
    if (card_item) {
        if (!view_as_skill) selected = card_item;
        emit card_to_use();
    }
}

void Dashboard::onCardItemHover()
{
    _adjustCards();
    foreach(CardItem *card, m_handCards)
        card->goBack(true, true, -1, 300);
}

void Dashboard::onCardItemLeaveHover()
{
    _adjustCards();
    foreach(CardItem *card, m_handCards)
        card->goBack(true, true, -1, 300);
}

void Dashboard::onMarkChanged()
{
    CardItem *card_item = qobject_cast<CardItem *>(sender());

    Q_ASSERT(card_item->isEquipped());

    if (card_item) {
        if (card_item->isMarked()) {
            if (!pendings.contains(card_item)) {
                if (view_as_skill && view_as_skill->inherits("OneCardViewAsSkill"))
                    unselectAll(card_item);
                pendings.append(card_item);
            }
        } else
            pendings.removeOne(card_item);

        updatePending();
    }
}

void Dashboard::updateTrustButton()
{
    if (!ClientInstance->getReplayer()) {
        bool trusting = Self->getState() == "trust";
        m_trustButton->update();
        setTrust(trusting);
    }
}

void Dashboard::showHeroSkinList()
{
    if (NULL != m_player) {
        if (sender() == m_changeHeadHeroSkinButton) {
            showHeroSkinListHelper(m_player->getGeneral(),
                m_headHeroSkinContainer);
        } else {
            showHeroSkinListHelper(m_player->getGeneral2(),
                m_deputyHeroSkinContainer);
        }
    }
}

void Dashboard::heroSkinButtonMouseOutsideClicked()
{
    if (NULL != m_player) {
        QSanButton *heroSKinBtn = NULL;
        if (sender() == m_changeHeadHeroSkinButton) {
            heroSKinBtn = m_changeHeadHeroSkinButton;
        } else {
            heroSKinBtn = m_changeDeputyHeroSkinButton;
        }

        QGraphicsItem *parent = heroSKinBtn->parentItem();
        if (NULL != parent && !parent->isUnderMouse())
            heroSKinBtn->hide();
    }
}

void Dashboard::onAvatarHoverEnter()
{
    if (NULL != m_player) {
        QObject *senderObj = sender();

        const General *general = NULL;
        GraphicsPixmapHoverItem *avatarItem = NULL;
        QSanButton *heroSKinBtn = NULL;

        if (senderObj == _m_avatarIcon) {
            general = m_player->getGeneral();
            avatarItem = _m_avatarIcon;
            heroSKinBtn = m_changeHeadHeroSkinButton;

            m_changeDeputyHeroSkinButton->hide();
        } else if (senderObj == _m_smallAvatarIcon) {
            general = m_player->getGeneral2();
            avatarItem = _m_smallAvatarIcon;
            heroSKinBtn = m_changeDeputyHeroSkinButton;

            m_changeHeadHeroSkinButton->hide();
        }

        if (NULL != general && HeroSkinContainer::hasSkin(general->objectName())
            && avatarItem->isSkinChangingFinished()) {
            heroSKinBtn->show();
        }
    }
}

void Dashboard::onAvatarHoverLeave()
{
    if (NULL != m_player) {
        QObject *senderObj = sender();

        QSanButton *heroSKinBtn = NULL;

        if (senderObj == _m_avatarIcon)
            heroSKinBtn = m_changeHeadHeroSkinButton;
        else if (senderObj == _m_smallAvatarIcon)
            heroSKinBtn = m_changeDeputyHeroSkinButton;

        if ((NULL != heroSKinBtn) && (!heroSKinBtn->isMouseInside())) {
            heroSKinBtn->hide();
            //doAvatarHoverLeave();
        }
    }
}

void Dashboard::onSkinChangingStart()
{
    QSanButton *heroSKinBtn = NULL;
    QString generalName;

    if (sender() == _m_avatarIcon) {
        heroSKinBtn = m_changeHeadHeroSkinButton;
        generalName = m_player->getGeneralName();
    } else {
        heroSKinBtn = m_changeDeputyHeroSkinButton;
        generalName = m_player->getGeneral2Name();
    }

    heroSKinBtn->hide();
}

void Dashboard::onSkinChangingFinished()
{
    GraphicsPixmapHoverItem *avatarItem = NULL;
    QSanButton *heroSKinBtn = NULL;
    QString generalName;

    if (sender() == _m_avatarIcon) {
        avatarItem = _m_avatarIcon;
        heroSKinBtn = m_changeHeadHeroSkinButton;
        generalName = m_player->getGeneralName();
    } else {
        avatarItem = _m_smallAvatarIcon;
        heroSKinBtn = m_changeDeputyHeroSkinButton;
        generalName = m_player->getGeneral2Name();
    }

    if (avatarItem->isUnderMouse() && !_m_skillDock->isUnderMouse())
        heroSKinBtn->show();
}

const ViewAsSkill *Dashboard::currentSkill() const
{
    return view_as_skill;
}

const Card *Dashboard::pendingCard() const
{
    return pending_card;
}
