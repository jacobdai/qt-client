/*
 * This file is part of the xTuple ERP: PostBooks Edition, a free and
 * open source Enterprise Resource Planning software suite,
 * Copyright (c) 1999-2019 by OpenMFG LLC, d/b/a xTuple.
 * It is licensed to you under the Common Public Attribution License
 * version 1.0, the full text of which (including xTuple-specific Exhibits)
 * is available at www.xtuple.com/CPAL.  By using this software, you agree
 * to be bound by its terms.
 */

#ifndef GUIERRORCHECK_H
#define GUIERRORCHECK_H

#include <QString>
#include <QObject>
#include <QtScript>
#include <QList>

class QWidget;

class GuiErrorCheck
{
  public:
    GuiErrorCheck(bool hasError, QWidget *widget, QString msg);
    GuiErrorCheck();
    ~GuiErrorCheck();

    static bool reportErrors(QWidget *parent, QString title, QList<GuiErrorCheck> list);

    // order of declarations should match order in constructor:
    // http://gcc.gnu.org/ml/gcc-help/2006-06/msg00068.html
    bool        hasError;
    QWidget    *widget;
    QString     msg;
};

// Script exposure
Q_DECLARE_METATYPE(GuiErrorCheck)
Q_DECLARE_METATYPE(QList<GuiErrorCheck>)
void setupGuiErrorCheck(QScriptEngine *engine);
QScriptValue constructGuiErrorCheck(QScriptContext *context, QScriptEngine *engine);

#endif // GUIERRORCHECK_H
