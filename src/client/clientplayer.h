#ifndef _CLIENT_PLAYER_H
#define _CLIENT_PLAYER_H

#include "player.h"
#include "clientstruct.h"

class Client;
class QTextDocument;

class ClientPlayer : public Player
{
    Q_OBJECT
    Q_PROPERTY(int handcard READ getHandcardNum WRITE setHandcardNum)

public:
    explicit ClientPlayer(Client *client);
    virtual QList<const Card *> getHandcards() const;
    void setCards(const QList<int> &card_ids);
    QTextDocument *getMarkDoc() const;
    void changePile(const QString &name, bool add, QList<int> card_ids);
    QString getDeathPixmapPath() const;
    void setHandcardNum(int n);
    virtual QString getGameMode() const;

    virtual void setFlags(const QString &flag);
    virtual int aliveCount() const;
    virtual int getHandcardNum() const;
    virtual void removeCard(const Card *card, Place place);
    virtual void addCard(const Card *card, Place place);
    virtual void addKnownHandCard(const Card *card);
    virtual bool isLastHandCard(const Card *card, bool contain = false) const;
    virtual void setMark(const QString &mark, int value, bool is_tip = false);

    virtual void setHeadSkinId(int id);
    virtual void setDeputySkinId(int id);

private:
    int handcard_num;
    QList<const Card *> known_cards;
    QTextDocument *mark_doc;

signals:

    void equiparea_sealed(const QString &name);
    void equiparea_unsealed(const QString &name);

    void judgearea_sealed();
    void judgearea_unsealed();

    void pile_changed(const QString &name);
	void count_changed(const QString &name, int value);
    void tip_changed(const QString &name, bool is_add);
    void drank_changed();
    void action_taken();
    void duanchang_invoked();
    void headSkinIdChanged(const QString &generalName);
    void deputySkinIdChanged(const QString &generalName);
};

extern ClientPlayer *Self;

#endif

