#ifndef _RECORDER_H
#define _RECORDER_H

#include "protocol.h"

#include <QObject>
#include <QTime>
#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include <QImage>
#include <QMap>

class Recorder : public QObject
{
    Q_OBJECT

public:
    explicit Recorder(QObject *parent = NULL);

    static QImage TXT2PNG(QByteArray data);
    static QByteArray PNG2TXT(const QString filename);

    bool save(const QString &filename) const;
    QList<QByteArray> getRecords() const;

public slots:
    void recordLine(const QByteArray &line);

private:
    QTime watch;
    QByteArray data;
};

class Replayer : public QThread
{
    Q_OBJECT

public:
    explicit Replayer(QObject *parent, const QString &filename);

    int getDuration() const;
    qreal getSpeed();

    QString getPath() const;

    int m_commandSeriesCounter;

public slots:
    void uniform();
    void toggle();
    void speedUp();
    void slowDown();

protected:
    virtual void run();

private:
    QString filename;
    qreal speed;
    bool playing;
    QMutex mutex;
    QSemaphore play_sem;
    int duration;
    int pair_offset;

    struct Pair
    {
        int elapsed;
        QByteArray cmd;
    };
    QList<Pair> pairs;

signals:
    void command_parsed(const QByteArray &cmd);
    void elasped(int secs);
    void speed_changed(qreal speed);
};

#endif

