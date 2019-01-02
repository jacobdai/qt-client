/*
 * This file is part of the xTuple ERP: PostBooks Edition, a free and
 * open source Enterprise Resource Planning software suite,
 * Copyright (c) 1999-2018 by OpenMFG LLC, d/b/a xTuple.
 * It is licensed to you under the Common Public Attribution License
 * version 1.0, the full text of which (including xTuple-specific Exhibits)
 * is available at www.xtuple.com/CPAL.  By using this software, you agree
 * to be bound by its terms.
 */

#ifndef CUSTOMERTYPELIST_H
#define CUSTOMERTYPELIST_H

#include "guiclient.h"
#include "xdialog.h"
#include <parameter.h>

#include "ui_customerTypeList.h"

class customerTypeList : public XDialog, public Ui::customerTypeList
{
    Q_OBJECT

public:
    customerTypeList(QWidget* parent = 0, const char* name = 0, bool modal = false, Qt::WindowFlags fl = 0);
    ~customerTypeList();

public slots:
    virtual enum SetResponse set(const ParameterList & pParams );
    virtual void sClose();
    virtual void sClear();
    virtual void sSelect();
    virtual void sFillList();

protected slots:
    virtual void languageChange();

private:
    int _custtypeid;

};

#endif // CUSTOMERTYPELIST_H
