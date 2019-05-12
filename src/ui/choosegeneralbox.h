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

#ifndef _CHOOSE_GENERAL_BOX_H
#define _CHOOSE_GENERAL_BOX_H

#include "carditem.h"
#include "timed-progressbar.h"
#include "graphicsbox.h"

class Button;
class QGraphicsDropShadowEffect;

class GeneralCardItem : public CardItem
{
    Q_OBJECT
	Q_ENUMS(ExpandType)

public:
	enum ExpandType
    {
        None, ExpandUp, ExpandDown, ExpandNone
    };

    friend class ChooseGeneralBox;
    void showCompanion();
    void hideCompanion();
	void setExpandType(ExpandType type);

protected:
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

private:
    GeneralCardItem(const QString &generalName, const int skinId);

    bool hasCompanion;
	ExpandType expand_type;

public slots:
    virtual void changeGeneral(const QString &generalName);
};

class ChooseGeneralBox : public GraphicsBox
{
    Q_OBJECT

public:
    explicit ChooseGeneralBox();

    void paintLayout(QPainter *painter);
    QRectF boundingRect() const;
    void clear();

public slots:
    void chooseGeneral(const QStringList &generals, bool m_viewOnly = false, bool single_result = false, const QString &reason = QString(), bool convert_enabled = false);
    void reply();
    void adjustItems();
	void showConvertion();
	void changeConvertion(const QString &general_name, bool show = true);
    void createConvertion(GeneralCardItem *card_item, bool first_row, QStringList all_generals);

private:
    int general_number;
    bool single_result;
    bool m_viewOnly;
	GeneralCardItem *lord_item;
    QList<GeneralCardItem *> items, selected;
    static const int top_dark_bar = 27;
    static const int top_blank_width = 42;
    static const int bottom_blank_width = 68;
    static const int card_bottom_to_split_line = 23;
    static const int card_to_center_line = 5;
	static const int lord_to_card_center_line = 20;
    static const int left_blank_width = 37;
    static const int split_line_to_card_seat = 15;

	QMap<QString, QList<GeneralCardItem *> > _m_convert_items;
	QString _m_current_convert_name;

    //data index
    static const int S_DATA_INITIAL_HOME_POS = 9527;

    Button *confirm;
    QGraphicsProxyWidget *progress_bar_item;
    QSanCommandProgressBar *progress_bar;

    void _initializeItems();

private slots:
    void _adjust();
    void _onItemClicked();
};

#endif // _CHOOSE_GENERAL_BOX_H
