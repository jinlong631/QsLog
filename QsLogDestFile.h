// Copyright (c) 2013, Razvan Petru
// All rights reserved.

// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:

// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice, this
//   list of conditions and the following disclaimer in the documentation and/or other
//   materials provided with the distribution.
// * The name of the contributors may not be used to endorse or promote products
//   derived from this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef QSLOGDESTFILE_H
#define QSLOGDESTFILE_H

#include "QsLogDest.h"
#include <QFile>
#include <QTextStream>
#include <QtGlobal>
#include <QSharedPointer>
#include <QDateTime>

namespace QsLogging
{
class RotationStrategy
{
public:
    virtual ~RotationStrategy();

    virtual void setInitialInfo(const QFile &file) = 0;
    virtual void includeMessageInCalculation(const QString &message) = 0;
    virtual bool shouldRotate() = 0;
    virtual void rotate() = 0;
    virtual QString getFileName() = 0;
    virtual QIODevice::OpenMode recommendedOpenModeFlag() = 0;
};

// Never rotates file, overwrites existing file.
class NullRotationStrategy : public RotationStrategy
{
public:
    void setInitialInfo(const QFile &) override {}
    void includeMessageInCalculation(const QString &) override {}
    bool shouldRotate() override { return false; }
    void rotate() override {}
    QString getFileName() override { return "";}
    QIODevice::OpenMode recommendedOpenModeFlag() override { return QIODevice::Truncate; }
};

// Rotates after a size is reached, keeps a number of <= 10 backups, appends to existing file.
class SizeRotationStrategy : public RotationStrategy
{
public:
    SizeRotationStrategy();
    static const int MaxBackupCount;

    void setInitialInfo(const QFile &file) override;
    void includeMessageInCalculation(const QString &message) override;
    bool shouldRotate() override;
    void rotate() override;
    QString getFileName() override { return "";}
    QIODevice::OpenMode recommendedOpenModeFlag() override;

    void setMaximumSizeInBytes(qint64 size);
    void setBackupCount(int backups);

private:
    QString mFileName;
    qint64 mCurrentSizeInBytes;
    qint64 mMaxSizeInBytes;
    int mBackupsCount;
};
class DailyRotationStrategy : public RotationStrategy
{
public:
    DailyRotationStrategy();
    void setInitialInfo(const QFile &file) override;
    void includeMessageInCalculation(const QString &message) override;
    bool shouldRotate() override;
    void rotate() override;
    QString getFileName() override;
    QIODevice::OpenMode recommendedOpenModeFlag() override;

    void setRotation_hour(int newRotation_hour);
    void setRotation_minute(int newRotation_minute);

    QString calc_filename(const QString fileName, QDateTime dt);
    QDateTime next_rotation_tp(int rotation_hour,int rotation_minute);
private:
    QString mFileName;

    int rotation_hour_;
    int rotation_minute_;
    QDateTime rotation_tp_;
};
typedef QSharedPointer<RotationStrategy> RotationStrategyPtr;

// file message sink
class FileDestination : public Destination
{
public:
    FileDestination(const QString& filePath, RotationStrategyPtr rotationStrategy);
    void write(const QString& message, Level level) override;
    bool isValid() override;

private:
    QFile mFile;
    QTextStream mOutputStream;
    QSharedPointer<RotationStrategy> mRotationStrategy;
};
class DailyFileDestination : public Destination
{
public:
    DailyFileDestination(const QString& filePath, RotationStrategyPtr rotationStrategy);
    void write(const QString& message, Level level) override;
    bool isValid() override;

private:
    QFile mFile;
    QTextStream mOutputStream;
    QSharedPointer<RotationStrategy> mRotationStrategy_;
};
}

#endif // QSLOGDESTFILE_H
