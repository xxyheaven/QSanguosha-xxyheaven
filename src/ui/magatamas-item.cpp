#include "magatamas-item.h"
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include "skin-bank.h"
#include "sprite.h"
#include "pixmapanimation.h"

MagatamasBoxItem::MagatamasBoxItem()
    : QGraphicsObject(NULL)
{
    m_hp = 0;
    m_maxHp = 0;
}

MagatamasBoxItem::MagatamasBoxItem(QGraphicsItem *parent)
    : QGraphicsObject(parent)
{
    m_hp = 0;
    m_maxHp = 0;
}

void MagatamasBoxItem::_updateLayout()
{
    for (int i = 0; i < 4; i++) {
        _icons[i] = G_ROOM_SKIN.getPixmap(QString(QSanRoomSkin::S_SKIN_KEY_MAGATAMAS).arg(m_resourceName).arg(QString::number(i)));
    }

    for (int i = 1; i < 7; i++) {
        _bgImages[i] = G_ROOM_SKIN.getPixmap(QString(QSanRoomSkin::S_SKIN_KEY_MAGATAMAS_BG).arg(QString::number(i)));
    }
    for (int i = 1; i < 4; i++) {
        for (int j = 0; j < 11; j++) {
            _numberIcons[(i-1)*11+j] = G_ROOM_SKIN.getPixmap(m_resourceName + QString(QSanRoomSkin::S_SKIN_KEY_MAGATAMAS_NUMBER), QString::number(i), QString::number(j), true);
        }
    }
}

void MagatamasBoxItem::setIconSize(QSize size)
{
    m_iconSize = size;
    _updateLayout();
}

QRectF MagatamasBoxItem::boundingRect() const
{
    int buckets = qMin(m_maxHp, m_showBackground ? 6 : 5);
    if (m_showBackground) {
        return QRectF(m_imageArea.topLeft(), _bgImages[buckets].size());
    } else {
        return QRectF(m_imageArea.topLeft(), QSizeF(m_iconSize.width(), buckets * (m_iconSize.height()+ m_magatamasSpacing)));
    }
}

void MagatamasBoxItem::setHp(int hp)
{
    _doHpChangeAnimation(hp);
    m_hp = hp;
    update();
}

void MagatamasBoxItem::setAnchor(QPoint anchor, Qt::Alignment align)
{
    m_anchor = anchor;
    m_align = align;
}

void MagatamasBoxItem::setMaxHp(int maxHp)
{
    m_maxHp = maxHp;
    _autoAdjustPos();
}

void MagatamasBoxItem::_autoAdjustPos()
{
    if (!anchorEnabled) return;
    QRectF rect = boundingRect();
    Qt::Alignment hAlign = m_align & Qt::AlignHorizontal_Mask;
    if (hAlign == Qt::AlignRight)
        setX(m_anchor.x() - rect.width());
    else if (hAlign == Qt::AlignHCenter)
        setX(m_anchor.x() - rect.width() / 2);
    else
        setX(m_anchor.x());
    Qt::Alignment vAlign = m_align & Qt::AlignVertical_Mask;
    if (vAlign == Qt::AlignBottom)
        setY(m_anchor.y() - rect.height());
    else if (vAlign == Qt::AlignVCenter)
        setY(m_anchor.y() - rect.height() / 2);
    else
        setY(m_anchor.y());
}

void MagatamasBoxItem::update()
{
    _updateLayout();
    _autoAdjustPos();
    QGraphicsItem::update();
}

void MagatamasBoxItem::_doHpChangeAnimation(int newHp)
{
    if (newHp >= m_hp) return;

	int width = m_iconSize.width();
    int height = m_iconSize.height();
    int xStep, yStep;
    xStep = 0;
    yStep = height;

    if (newHp < 0)
        newHp = 0;

	PixmapAnimation *pma = PixmapAnimation::GetPixmapAnimation(this, "destroy");
	
	if (m_showBackground) {
	    pma->moveBy((boundingRect().width() - pma->boundingRect().width()) / 2,
                (boundingRect().height() - pma->boundingRect().height()));
	    pma->moveBy(5, 5);
	
	    int pos = m_maxHp > 5 ? 4 : newHp;
	
	    pma->moveBy(- xStep * pos, - yStep * pos);
		
	} else {
		pma->moveBy((width - pma->boundingRect().width()) / 2,
                (height - pma->boundingRect().height()));

		pma->moveBy(5, 5);
		
		
	    int pos = m_maxHp > 5 ? 0 : (m_maxHp - 1 - newHp);
	
	    pma->moveBy(xStep * pos, yStep * pos);
	}
}

void MagatamasBoxItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (m_maxHp <= 0) return;
    int imageIndex = 0;
    if (m_hp == m_maxHp || m_hp > m_maxHp*2/3)
        imageIndex = 3;
    else if (m_hp > m_maxHp/3)
        imageIndex = 2;
    else if (m_hp > 0)
        imageIndex = 1;
    int buckets = qMin(m_maxHp, m_showBackground ? 6 : 5);

    int yStep = m_iconSize.height() + m_magatamasSpacing;

    if (m_showBackground) {
        painter->save();
        painter->translate(boundingRect().topLeft());
        painter->drawPixmap(0, 0, _bgImages[buckets]);
        painter->restore();
    }

    painter->save();
    painter->translate(boundingRect().bottomLeft()+QPointF(m_magatamasOffset, 0));

    if (m_maxHp <= 5) {
        int i;
        for (i = 1; i <= m_maxHp; i++) {
            if (m_showBackground && i <= m_hp) continue;
            QRect rect(0, -yStep * i, m_imageArea.width(), m_imageArea.height());
            painter->drawPixmap(rect, _icons[0]);
        }
        for (i = 1 ; i <= m_hp; i++) {
            QRect rect(0, -yStep * i, m_imageArea.width(), m_imageArea.height());
            painter->drawPixmap(rect, _icons[imageIndex]);
        }
    } else {
        int numIndex= qMax(1, imageIndex);
        int x = (m_imageArea.width()-m_numberArea.width())/2;
        int yStep2 = m_numberArea.height() + m_magatamasSpacing;

        int num1 = m_maxHp%10;
        int x1 = m_maxHp > 9 ? (m_imageArea.width()/2) : x;
        QRect rect1(x1, -m_numberArea.y()-yStep2, m_numberArea.width(), m_numberArea.height());
        painter->drawPixmap(rect1, _numberIcons[(numIndex-1)*11+num1]);
        if (m_maxHp > 9) {
            num1 = (m_maxHp/10)%10;
            QRect rect11(m_imageArea.width()/2-m_numberArea.width(), -m_numberArea.y()-yStep2, m_numberArea.width(), m_numberArea.height());
            painter->drawPixmap(rect11, _numberIcons[(numIndex-1)*11+num1]);
        }

        QRect rect2(x, -m_numberArea.y()-2*yStep2, m_numberArea.width(), m_numberArea.height());
        painter->drawPixmap(rect2, _numberIcons[(numIndex-1)*11+10]);

        int hp = qMax(0, m_hp);
        int num3 = hp%10;
        int x3 = hp > 9 ? (m_imageArea.width()/2) : x;
        QRect rect3(x3, -m_numberArea.y()-3*yStep2, m_numberArea.width(), m_numberArea.height());
        painter->drawPixmap(rect3, _numberIcons[(numIndex-1)*11+num3]);
        if (hp > 9) {
            num3 = (hp/10)%10;
            QRect rect33(m_imageArea.width()/2-m_numberArea.width(), -m_numberArea.y()-3*yStep2, m_numberArea.width(), m_numberArea.height());
            painter->drawPixmap(rect33, _numberIcons[(numIndex-1)*11+num3]);
        }

        QRect rect(0, -m_numberArea.x()-m_numberArea.y()-3*yStep2-yStep, m_imageArea.width(), m_imageArea.height());
        painter->drawPixmap(rect, _icons[imageIndex]);
    }
    painter->restore();
}

