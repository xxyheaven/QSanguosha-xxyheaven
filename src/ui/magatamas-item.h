#ifndef _MAGATAMAS_ITEM_H
#define _MAGATAMAS_ITEM_H

#include <QGraphicsObject>
#include <qpixmap.h>
#include "skin-bank.h"

class MagatamasBoxItem : public QGraphicsObject
{
    Q_OBJECT

public:
    MagatamasBoxItem();
    MagatamasBoxItem(QGraphicsItem *parent);
    inline int getHp() const
    {
        return m_hp;
    }
    void setHp(int hp);
    inline int getMaxHp() const
    {
        return m_maxHp;
    }
    void setMaxHp(int maxHp);
    inline void setBackgroundVisible(bool visible)
    {
        m_showBackground = visible;
    }
    inline bool isBackgroundVisible() const
    {
        return m_showBackground;
    }
    inline void setMagatamasSpacing(int spacing)
    {
        m_magatamasSpacing = spacing;
    }
    inline int getMagatamasSpacing() const
    {
        return m_magatamasSpacing;
    }
    inline void setMagatamasOffset(int offset)
    {
        m_magatamasOffset = offset;
    }
    inline int getMagatamasOffset() const
    {
        return m_magatamasOffset;
    }
    void setAnchor(QPoint anchor, Qt::Alignment align);
    inline void setAnchorEnable(bool enabled)
    {
        anchorEnabled = enabled;
    }
    inline bool isAnchorEnable()
    {
        return anchorEnabled;
    }
    void setIconSize(QSize size);
    inline void setImageArea(QRect rect)
    {
        m_imageArea = rect;
    }
    inline void setNumberArea(QRect rect)
    {
        m_numberArea = rect;
    }
    inline void setResourceName(QString name)
    {
        m_resourceName = name;
    }
    inline QSize getIconSize() const
    {
        return m_iconSize;
    }
    virtual QRectF boundingRect() const;
    virtual void update();
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

protected:
    void _autoAdjustPos();
    void _updateLayout();
    void _doHpChangeAnimation(int newHp);
    QPoint m_anchor;
    Qt::Alignment m_align;
    bool anchorEnabled;
    int m_hp;
    int m_maxHp;
    bool m_showBackground;
    int m_magatamasSpacing;
    int m_magatamasOffset;
    QSize m_iconSize;
    QRect m_imageArea;
    QRect m_numberArea;
    QString m_resourceName;
    QPixmap _icons[4];
    QPixmap _bgImages[7];
    QPixmap _numberIcons[33];
};
#endif

