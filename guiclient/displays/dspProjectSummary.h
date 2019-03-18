/*
 * This file is part of the xTuple ERP: PostBooks Edition, a free and
 * open source Enterprise Resource Planning software suite,
 * Copyright (c) 1999-2019 by OpenMFG LLC, d/b/a xTuple.
 * It is licensed to you under the Common Public Attribution License
 * version 1.0, the full text of which (including xTuple-specific Exhibits)
 * is available at www.xtuple.com/CPAL.  By using this software, you agree
 * to be bound by its terms.
 */

#ifndef DSPPROJECTSUMMARY_H
#define DSPPROJECTSUMMARY_H

#include "display.h"

class dspProjectSummary : public display
{
    Q_OBJECT

public:
    dspProjectSummary(QWidget* parent = 0, const char* name = 0, Qt::WindowFlags fl = Qt::Window);

    virtual bool setParams(ParameterList &);

public slots:
    virtual void sNew();
    virtual void sEdit();
    virtual void sView();
    virtual void sOpenProject(QString);
    virtual void sPopulateMenu(QMenu * pMenu, QTreeWidgetItem * pSelected, int);
};

#endif // DSPPROJECTSUMMARY_H