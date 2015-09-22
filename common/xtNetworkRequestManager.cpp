/*
 * This file is part of the xTuple ERP: PostBooks Edition, a free and
 * open source Enterprise Resource Planning software suite,
 * Copyright (c) 1999-2015 by OpenMFG LLC, d/b/a xTuple.
 * It is licensed to you under the Common Public Attribution License
 * version 1.0, the full text of which (including xTuple-specific Exhibits)
 * is available at www.xtuple.com/CPAL.  By using this software, you agree
 * to be bound by its terms.
 */

#include "xtNetworkRequestManager.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QDebug>

#define DEBUG false

xtNetworkRequestManager::xtNetworkRequestManager(const QUrl & url, QMutex &mutex) {
  nwam = new QNetworkAccessManager;
  _nwrep = 0;
  _response = 0;
  _mutex = &mutex;
  _mutex->lock();
  _loop = new QEventLoop;
  startRequest(url);
}
void xtNetworkRequestManager::startRequest(const QUrl & url) {
    _nwrep = nwam->get(QNetworkRequest(url));
    connect(_nwrep, SIGNAL(finished()), SLOT(requestCompleted()));
    connect(nwam, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(sslErrors(QNetworkReply*,QList<QSslError>)));
    connect(nwam, SIGNAL(finished(QNetworkReply*)), _loop, SLOT(quit()));
    _loop->exec();
}
void xtNetworkRequestManager::requestCompleted() {
  _response = _nwrep->readAll(); //we don't really care here but store it anyways
  _nwrep->close();
  QVariant possibleRedirect = _nwrep->attribute(QNetworkRequest::RedirectionTargetAttribute);
  if(DEBUG){
      qDebug() << "redirect=" << possibleRedirect;
      qDebug() << "replyError=" << _nwrep->errorString();
      qDebug() << "replyErrorCode=" << _nwrep->error();
  }
  if(_nwrep->error() == QNetworkReply::NoError){
      _nwrep->deleteLater();
      _mutex->unlock();
  }
  else if(!possibleRedirect.isNull()){
      QUrl newUrl = possibleRedirect.toUrl();
      _nwrep->deleteLater();
      startRequest(newUrl);
  }
}
void xtNetworkRequestManager::sslErrors(QNetworkReply*, const QList<QSslError> &errors) {
#if QT_VERSION >= 0x050000
    QString errorString;
       foreach (const QSslError &error, errors) {
           if (!errorString.isEmpty())
               errorString += ", ";
           errorString += error.errorString();
       }
   if(DEBUG)
   qDebug() << "errorString= " << errorString;
#endif
}
xtNetworkRequestManager::~xtNetworkRequestManager() {
    nwam->deleteLater();
    nwam = 0;
}

