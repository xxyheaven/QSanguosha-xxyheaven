#ifndef _CARD_ITEM_H
#define _CARD_ITEM_H

#include "card.h"
#include "qsan-selectable-item.h"
#include "settings.h"
#include <QAbstractAnimation>
#include <QMutex>
#include <QSize>
#include "skin-bank.h"

class FilterSkill;
class General;
class QGraphicsDropShadowEffect;

class CardItem : public QSanSelectableItem
{
    Q_OBJECT
    Q_ENUMS(AnimationType)

public:
    enum AnimationType
    {
        Normal, FromLeft, FromRight, FromCenter
    };

    CardItem(const Card *card);
    CardItem(const QString &general_name);
    ~CardItem();

    virtual QRectF boundingRect() const;
    virtual void setEnabled(bool enabled);

    const Card *getCard() const;
    void setCard(const Card *card);
    inline int getId() const
    {
        return m_cardId;
    }

    // For move card animation
    void setHomePos(QPointF home_pos);
    QPointF homePos() const;
    QAbstractAnimation *getGoBackAnimation(bool doFadeEffect, int animation_type = -1, bool smoothTransition = false,
        int duration = Config.S_MOVE_CARD_ANIMATION_DURATION);
    void goBack(bool playAnimation, bool doFade = true, int animation_type = -1, int duration = Config.S_MOVE_CARD_ANIMATION_DURATION);
    inline QAbstractAnimation *getCurrentAnimation(bool)
    {
        return m_currentAnimation;
    }
    inline void setHomeOpacity(double opacity)
    {
        m_opacityAtHome = opacity;
    }
    inline double getHomeOpacity()
    {
        return m_opacityAtHome;
    }

    void showAvatar(const General *general);
    void hideAvatar();
    void showSmallCard(const QString &card_name);
    void hideSmallCard();
    void setAutoBack(bool auto_back);
    void changeGeneral(const QString &general_name);
    void setFootnote(const QString &desc);

    inline bool isSelected() const
    {
        return m_isSelected;
    }
    inline void setSelected(bool selected)
    {
        m_isSelected = selected;
    }

    inline bool isChosen() const
    {
        return m_isChosen;
    }
    inline void setChosen(bool selected)
    {
        m_isChosen = selected;
    }

    inline bool isHovered() const
    {
        return m_isHovered;
    }

    bool isEquipped() const;

    void setFrozen(bool is_frozen);
    inline bool isFrozen()
    {
        return frozen;
    }

    inline void showFootnote()
    {
        _m_showFootnote = true;
    }
    inline void hideFootnote()
    {
        _m_showFootnote = false;
    }

    static CardItem *FindItem(const QList<CardItem *> &items, int card_id);

    struct UiHelper
    {
        int tablePileClearTimeStamp;
    } m_uiHelper;

    void clickItem()
    {
        emit clicked();
    }

    void setOuterGlowEffectEnabled(const bool &willPlay);
    bool isOuterGlowEffectEnabled() const;

    void setOuterGlowColor(const QColor &color);
    QColor getOuterGlowColor() const;

private slots:
    void currentAnimationDestroyed();

protected:
    void _initialize();
    QAbstractAnimation *m_currentAnimation;
    QImage _m_footnoteImage;
    bool _m_showFootnote;
    Card::Suit _m_validate_suit;
    // QGraphicsPixmapItem *_m_footnoteItem;
    QMutex m_animationMutex;
    double m_opacityAtHome;
    bool m_isSelected, m_isChosen; // for playercardbox
    bool m_isHovered;
    bool _m_isUnknownGeneral;
    int _skinId;
    bool auto_back, frozen;
    static const int _S_CLICK_JITTER_TOLERANCE;
    static const int _S_MOVE_JITTER_TOLERANCE;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *);
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
    int m_cardId;
    const Card *m_card;
    QString _m_avatarName, _m_smallCardName;
    QPointF home_pos;
	bool outerGlowEffectEnabled;
	QColor outerGlowColor;
    QGraphicsDropShadowEffect *outerGlowEffect;
    QPointF _m_lastMousePressScenePos;

signals:
    void toggle_discards();
    void clicked();
    void double_clicked();
    void thrown();
    void released();
    void enter_hover();
    void leave_hover();
    void movement_animation_finished();
	void hoverChanged(const bool &enter);
};

#endif

