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

#include "QsLogDestFile.h"
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif
#include <QDateTime>
#include <QtGlobal>
#include <iostream>
#include <QtDebug>
#include <QFileInfo>
#include <QDir>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
namespace Qt {
static const QTextStreamFunction endl = ::endl;
}
#endif

const int QsLogging::SizeRotationStrategy::MaxBackupCount = 10;

QsLogging::RotationStrategy::~RotationStrategy()
{
}

QsLogging::SizeRotationStrategy::SizeRotationStrategy()
    : mCurrentSizeInBytes(0)
    , mMaxSizeInBytes(0)
    , mBackupsCount(0)
{
}

void QsLogging::SizeRotationStrategy::setInitialInfo(const QFile &file)
{
    mFileName = file.fileName();
    mCurrentSizeInBytes = file.size();
}

void QsLogging::SizeRotationStrategy::includeMessageInCalculation(const QString &message)
{
    mCurrentSizeInBytes += message.toUtf8().size();
}

bool QsLogging::SizeRotationStrategy::shouldRotate()
{
    return mCurrentSizeInBytes > mMaxSizeInBytes;
}

// Algorithm assumes backups will be named filename.X, where 1 <= X <= mBackupsCount.
// All X's will be shifted up.
void QsLogging::SizeRotationStrategy::rotate()
{
    if (!mBackupsCount) {
        if (!QFile::remove(mFileName))
            std::cerr << "QsLog: backup delete failed " << qPrintable(mFileName);
        return;
    }

     // 1. find the last existing backup than can be shifted up
     const QString logNamePattern = mFileName + QString::fromUtf8(".%1");
     int lastExistingBackupIndex = 0;
     for (int i = 1;i <= mBackupsCount;++i) {
         const QString backupFileName = logNamePattern.arg(i);
         if (QFile::exists(backupFileName))
             lastExistingBackupIndex = qMin(i, mBackupsCount - 1);
         else
             break;
     }

     // 2. shift up
     for (int i = lastExistingBackupIndex;i >= 1;--i) {
         const QString oldName = logNamePattern.arg(i);
         const QString newName = logNamePattern.arg(i + 1);
         QFile::remove(newName);
         const bool renamed = QFile::rename(oldName, newName);
         if (!renamed) {
             std::cerr << "QsLog: could not rename backup " << qPrintable(oldName)
                       << " to " << qPrintable(newName);
         }
     }

     // 3. rename current log file
     const QString newName = logNamePattern.arg(1);
     if (QFile::exists(newName))
         QFile::remove(newName);
     if (!QFile::rename(mFileName, newName)) {
         std::cerr << "QsLog: could not rename log " << qPrintable(mFileName)
                   << " to " << qPrintable(newName);
     }
}

QIODevice::OpenMode QsLogging::SizeRotationStrategy::recommendedOpenModeFlag()
{
    return QIODevice::Append;
}

void QsLogging::SizeRotationStrategy::setMaximumSizeInBytes(qint64 size)
{
    Q_ASSERT(size >= 0);
    mMaxSizeInBytes = size;
}

void QsLogging::SizeRotationStrategy::setBackupCount(int backups)
{
    Q_ASSERT(backups >= 0);
    mBackupsCount = qMin(backups, SizeRotationStrategy::MaxBackupCount);
}


QsLogging::FileDestination::FileDestination(const QString& filePath, RotationStrategyPtr rotationStrategy)
    : mRotationStrategy(rotationStrategy)
{
    mFile.setFileName(filePath);
    QString fileDir = QFileInfo(filePath).absolutePath();
    QDir dir(fileDir);
    if(!dir.exists()) {
        dir.mkdir(fileDir);
    }
    if (!mFile.open(QFile::WriteOnly | QFile::Text | mRotationStrategy->recommendedOpenModeFlag()))
        std::cerr << "QsLog: could not open log file " << qPrintable(filePath);
    mOutputStream.setDevice(&mFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    mOutputStream.setCodec(QTextCodec::codecForName("UTF-8"));
#endif

    mRotationStrategy->setInitialInfo(mFile);
}

void QsLogging::FileDestination::write(const QString& message, Level)
{
    mRotationStrategy->includeMessageInCalculation(message);
    if (mRotationStrategy->shouldRotate()) {
        mOutputStream.setDevice(NULL);
        mFile.close();
        mRotationStrategy->rotate();
        if (!mFile.open(QFile::WriteOnly | QFile::Text | mRotationStrategy->recommendedOpenModeFlag()))
            std::cerr << "QsLog: could not reopen log file " << qPrintable(mFile.fileName());
        mRotationStrategy->setInitialInfo(mFile);
        mOutputStream.setDevice(&mFile);
    }

    mOutputStream << message << Qt::endl;
    mOutputStream.flush();
}

bool QsLogging::FileDestination::isValid()
{
    return mFile.isOpen();
}

QsLogging::DailyRotationStrategy::DailyRotationStrategy():
    rotation_hour_(0),
    rotation_minute_(0)
{

}

void QsLogging::DailyRotationStrategy::setInitialInfo(const QFile &file)
{
    mFileName = file.fileName();
    rotation_tp_ = next_rotation_tp(rotation_hour_,rotation_minute_);
}

void QsLogging::DailyRotationStrategy::includeMessageInCalculation(const QString &message)
{

}

bool QsLogging::DailyRotationStrategy::shouldRotate()
{
    QDateTime nowTime = QDateTime::currentDateTime();
//    qDebug()<<nowTime.toString("yyyy-MM-dd HH:mm");
//    qDebug()<<rotation_tp_.toString("yyyy-MM-dd HH:mm");
    if(nowTime > rotation_tp_ ){
        rotation_tp_ = next_rotation_tp(rotation_hour_,rotation_minute_);
        return true;
    }
    return false;
}

void QsLogging::DailyRotationStrategy::rotate()
{

    QString fileName = getFileName();

    QString fileDir = QFileInfo(fileName).absolutePath();
    QString filefilter = QFileInfo(fileName).suffix();
    QDir dir(fileDir);
    QStringList filename ;
    filename << "*."+filefilter;//可叠加，可使用通配符筛选
    QStringList results;
    results = dir.entryList(filename,QDir::Files | QDir::Readable,QDir::Time);
    if(results.length()>29) {
        for(int i=29;i<results.length();i++)
        {
            QFile::remove(results.at(i));
        }
    }
}

QString QsLogging::DailyRotationStrategy::getFileName()
{
    QDateTime nowdt = QDateTime::currentDateTime();
    QString fileName = DailyRotationStrategy::calc_filename(mFileName, nowdt);
    return fileName;
}

QIODevice::OpenMode QsLogging::DailyRotationStrategy::recommendedOpenModeFlag()
{
    return QIODevice::Append;
}

void QsLogging::DailyRotationStrategy::setRotation_hour(int newRotation_hour)
{
    rotation_hour_ = newRotation_hour;
}

void QsLogging::DailyRotationStrategy::setRotation_minute(int newRotation_minute)
{
    rotation_minute_ = newRotation_minute;
}

QString QsLogging::DailyRotationStrategy::calc_filename(const QString fileName, QDateTime dt)
{
    QStringList fileNamesplit = fileName.split(".");
    if(fileNamesplit.length()<2)
        fileNamesplit.append("");
    int year = dt.date().year();
    int month = dt.date().month();
    int day = dt.date().day();
    QString retFileName = QString("%1_%2_%3_%4.%5").arg(fileNamesplit.at(0)).arg(year).arg(month).arg(day).arg(fileNamesplit.at(1));
    return retFileName;
}

QDateTime QsLogging::DailyRotationStrategy::next_rotation_tp(int rotation_hour, int rotation_minute)
{
    QDateTime nowdt = QDateTime::currentDateTime();
//    qDebug()<<nowdt.toString("yyyy-MM-dd HH:mm");
    QTime rotationTime;
    rotationTime.setHMS(rotation_hour,rotation_minute,0);
    nowdt.setTime(rotationTime);
    nowdt = nowdt.addDays(1);
//    qDebug()<<nowdt.toString("yyyy-MM-dd HH:mm");
    return nowdt;
}

QsLogging::DailyFileDestination::DailyFileDestination(const QString& filePath, RotationStrategyPtr rotationStrategy)
    : mRotationStrategy_(rotationStrategy)
{
    mRotationStrategy_->setInitialInfo(QFile(filePath));

    QString fileName = mRotationStrategy_->getFileName();
    mFile.setFileName(fileName);
    QString fileDir = QFileInfo(fileName).absolutePath();
    QString filefilter = QFileInfo(fileName).suffix();
    QDir dir(fileDir);
    if(!dir.exists()) {
        dir.mkdir(fileDir);
    }
//    QStringList filename ;
//    filename << "*."+filefilter;//可叠加，可使用通配符筛选
//    QStringList results;
//    results = dir.entryList(filename,QDir::Files | QDir::Readable,QDir::Time);
//    if(results.length()>30) {
//        for(int i=30;i<results.length();i++)
//        {
//            QFile::remove(results.at(i));
//        }
//    }
    //qDebug()<<results;//results里就是获取的所有文件名了


    if (!mFile.open(QFile::WriteOnly | QFile::Text))
        std::cerr << "QsLog: could not open log file " << qPrintable(filePath);
    mOutputStream.setDevice(&mFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    mOutputStream.setCodec(QTextCodec::codecForName("UTF-8"));
#endif
}

void QsLogging::DailyFileDestination::write(const QString &message, Level level)
{
//    mRotationStrategy->includeMessageInCalculation(message);
    if (mRotationStrategy_->shouldRotate()) {
        mOutputStream.setDevice(NULL);
        mFile.close();
        mRotationStrategy_->rotate();
        QString fileName = mRotationStrategy_->getFileName();
        mFile.setFileName(fileName);
        if (!mFile.open(QFile::WriteOnly | QFile::Text | mRotationStrategy_->recommendedOpenModeFlag()))
            std::cerr << "QsLog: could not reopen log file " << qPrintable(mFile.fileName());
//        mRotationStrategy->setInitialInfo(mFile);
        mOutputStream.setDevice(&mFile);
    }

    mOutputStream << message << Qt::endl;
    mOutputStream.flush();
}

bool QsLogging::DailyFileDestination::isValid()
{
    return mFile.isOpen();
}
