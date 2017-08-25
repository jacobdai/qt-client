/*
 * This file is part of the xTuple ERP: PostBooks Edition, a free and
 * open source Enterprise Resource Planning software suite,
 * Copyright (c) 1999-2016 by OpenMFG LLC, d/b/a xTuple.
 * It is licensed to you under the Common Public Attribution License
 * version 1.0, the full text of which (including xTuple-specific Exhibits)
 * is available at www.xtuple.com/CPAL.  By using this software, you agree
 * to be bound by its terms.
 */

#include "selectPayments.h"
#include "mqlutil.h"

#include <QSqlError>
#include <QInputDialog>

#include <metasql.h>
#include <openreports.h>
#include <parameter.h>
#include <xdateinputdialog.h>
#include <QMessageBox>

#include "apOpenItem.h"
#include "dspGLSeries.h"
#include "errorReporter.h"
#include "guiclient.h"
#include "miscVoucher.h"
#include "selectBankAccount.h"
#include "selectPayment.h"
#include "storedProcErrorLookup.h"
#include "voucher.h"

selectPayments::selectPayments(QWidget* parent, const char* name, Qt::WindowFlags fl, bool pAutoFill)
    : XWidget(parent, name, fl)
{
  setupUi(this);

  _ignoreUpdates = true;

  connect(_clear, SIGNAL(clicked()), this, SLOT(sClear()));
  connect(_clearAll, SIGNAL(clicked()), this, SLOT(sClearAll()));
  connect(_selectDate, SIGNAL(currentIndexChanged(int)), this, SLOT(sFillList()));
  connect(_startDate, SIGNAL(newDate(const QDate&)), this, SLOT(sFillList()));
  connect(_endDate, SIGNAL(newDate(const QDate&)), this, SLOT(sFillList()));
  connect(_onOrBeforeDate, SIGNAL(newDate(const QDate&)), this, SLOT(sFillList()));
  connect(_query, SIGNAL(clicked()), this, SLOT(sFillList()));
  connect(_print, SIGNAL(clicked()), this, SLOT(sPrint()));
  connect(_select, SIGNAL(clicked()), this, SLOT(sSelect()));
  connect(_selectDiscount, SIGNAL(clicked()), this, SLOT(sSelectDiscount()));
  connect(_selectDue, SIGNAL(clicked()), this, SLOT(sSelectDue()));
  connect(_selectLine, SIGNAL(clicked()), this, SLOT(sSelectLine()));
  connect(_applyallcredits, SIGNAL(clicked()), this, SLOT(sApplyAllCredits()));
  connect(_vendorgroup, SIGNAL(updated()), this, SLOT(sFillList()));
  connect(_bankaccnt, SIGNAL(newID(int)), this, SLOT(sFillList()));
  connect(_apopen, SIGNAL(populateMenu(QMenu*, QTreeWidgetItem*, int)), this, SLOT(sPopulateMenu(QMenu*, QTreeWidgetItem*)));

  _bankaccnt->setType(XComboBox::APBankAccounts);

  _apopen->addColumn(tr("Vendor"),         -1,           Qt::AlignLeft  ,        true, "vendor" );
  _apopen->addColumn(tr("Doc. Type"),      _orderColumn, Qt::AlignCenter,        true, "doctype" );
  _apopen->addColumn(tr("Doc. #"),         _orderColumn, Qt::AlignRight ,        true, "apopen_docnumber" );
  _apopen->addColumn(tr("Inv. #"),         _orderColumn, Qt::AlignRight ,        true, "apopen_invcnumber" );
  _apopen->addColumn(tr("P/O #"),          _orderColumn, Qt::AlignRight ,        true, "apopen_ponumber" );
  _apopen->addColumn(tr("Due Date"),       _dateColumn,  Qt::AlignCenter,        true, "apopen_duedate" );
  _apopen->addColumn(tr("Doc. Date"),      _dateColumn,  Qt::AlignCenter,        false, "apopen_docdate" );
  _apopen->addColumn(tr("Amount"),         _moneyColumn, Qt::AlignRight ,        true, "amount" );
  _apopen->addColumn(tr("Amount (%1)").arg(CurrDisplay::baseCurrAbbr()), _moneyColumn, Qt::AlignRight, false, "base_amount"  );
  _apopen->addColumn(tr("Running Amount (%1)").arg(CurrDisplay::baseCurrAbbr()), _moneyColumn, Qt::AlignRight,         false, "running_amount" );
  _apopen->addColumn(tr("Approved"),       _moneyColumn, Qt::AlignRight ,        true, "selected" );
  _apopen->addColumn(tr("Approved (%1)").arg(CurrDisplay::baseCurrAbbr()), _moneyColumn, Qt::AlignRight, false, "base_selected"  );
  _apopen->addColumn(tr("Running Approved (%1)").arg(CurrDisplay::baseCurrAbbr()), _moneyColumn, Qt::AlignRight,       true, "running_selected"  );
  _apopen->addColumn(tr("Discount"),       _moneyColumn, Qt::AlignRight ,        true, "discount" );
  _apopen->addColumn(tr("Discount (%1)").arg(CurrDisplay::baseCurrAbbr()), _moneyColumn, Qt::AlignRight, false, "base_discount"  );
  _apopen->addColumn(tr("Currency"),       _currencyColumn, Qt::AlignLeft,       true, "curr_concat" );
  _apopen->addColumn(tr("Status"),         _currencyColumn, Qt::AlignCenter,     true, "apopen_status" );
  _apopen->addColumn(tr("On Hold Comment"), -1,             Qt::AlignLeft,       true, "apopen_hold_comment" );

//  if (omfgThis->singleCurrency())
//  {
//    _apopen->hideColumn("curr_concat");
//    _apopen->headerItem()->setText(11, tr("Running"));
//  }

  if (_privileges->check("ApplyAPMemos"))
      connect(_apopen, SIGNAL(valid(bool)), _applyallcredits, SLOT(setEnabled(bool)));

  connect(omfgThis, SIGNAL(paymentsUpdated(int, int, bool)), this, SLOT(sFillList()));

  _ignoreUpdates = false;

  if(pAutoFill)
    sFillList();
}

selectPayments::~selectPayments()
{
  // no need to delete child widgets, Qt does it all for us
}

void selectPayments::languageChange()
{
  retranslateUi(this);
}

bool selectPayments::setParams(ParameterList &params)
{
  _vendorgroup->appendValue(params);

  return true;
}

void selectPayments::sPrint()
{
  ParameterList params;
  if (! setParams(params))
    return;

  orReport report("SelectPaymentsList", params);
  if (report.isValid())
    report.print();
  else
    report.reportError(this);
}

void selectPayments::sSelectDue()
{
  XSqlQuery selectSelectDue;
  ParameterList params;
  params.append("type", "P");

  int bankaccntid = _bankaccnt->id();
  if(bankaccntid == -1)
  {
    selectBankAccount newdlg(this, "", true);
    newdlg.set(params);
    bankaccntid = newdlg.exec();
  }

  if (bankaccntid >= 0)
  {
    MetaSQLQuery mql = mqlLoad("selectPayments", "dueitems");
    ParameterList params;
    if (! setParams(params))
        return;
    params.append("bankaccnt_id", bankaccntid);
    selectSelectDue = mql.toQuery(params);
    if (selectSelectDue.first())
    {
      int result = selectSelectDue.value("result").toInt();
      if (result < 0)
      {
          ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Payment Information"),
                               storedProcErrorLookup("selectPayment", result),
                               __FILE__, __LINE__);
        return;
      }
    }
    else
    {
      ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Payment Information"),
                                        selectSelectDue, __FILE__, __LINE__);
      return;
    }

    omfgThis->sPaymentsUpdated(-1, -1, true);
  }
}

void selectPayments::sSelectDiscount()
{
  XSqlQuery selectSelectDiscount;
  ParameterList params;
  params.append("type", "P");

  int bankaccntid = _bankaccnt->id();
  if(bankaccntid == -1)
  {
    selectBankAccount newdlg(this, "", true);
    newdlg.set(params);
    bankaccntid = newdlg.exec();
  }

  if (bankaccntid >= 0)
  {
    MetaSQLQuery mql = mqlLoad("selectPayments", "discountitems");
    ParameterList params;
    if (! setParams(params))
        return;
    params.append("bankaccnt_id", bankaccntid);
    selectSelectDiscount = mql.toQuery(params);
    if (selectSelectDiscount.first())
    {
      int result = selectSelectDiscount.value("result").toInt();
      if (result < 0)
      {
        ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Payment Information"),
                               storedProcErrorLookup("selectPayment", result),
                               __FILE__, __LINE__);
        return;
      }
    }
    else
    {
      ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Payment Information"),
                           selectSelectDiscount, __FILE__, __LINE__);
      return;
    }

    omfgThis->sPaymentsUpdated(-1, -1, true);
  }
}

void selectPayments::sClearAll()
{
  XSqlQuery selectClearAll;
  MetaSQLQuery mql = mqlLoad("selectPayments", "clearall");
  ParameterList params;
  if (! setParams(params))
    return;
  selectClearAll = mql.toQuery(params);
  if (selectClearAll.first())
  {
    int result = selectClearAll.value("result").toInt();
    if (result < 0)
    {
      ErrorReporter::error(QtCriticalMsg, this, tr("Error Clearing Payment Information"),
                             storedProcErrorLookup("clearPayment", result),
                             __FILE__, __LINE__);
      return;
    }
  }
  else if (ErrorReporter::error(QtCriticalMsg, this, tr("Error Clearing Payment Information"),
                                selectClearAll, __FILE__, __LINE__))
  {
    return;
  }

  omfgThis->sPaymentsUpdated(-1, -1, true);
}

void selectPayments::sSelect()
{
  _ignoreUpdates = true;
  bool update = false;
  QList<XTreeWidgetItem*> list = _apopen->selectedItems();
  XTreeWidgetItem * cursor = 0;
  XSqlQuery slct;
  slct.prepare("SELECT apopen_status FROM apopen WHERE apopen_id=:apopen_id;");
  for(int i = 0; i < list.size(); i++)
  {
    cursor = (XTreeWidgetItem*)list.at(i);
    slct.bindValue(":apopen_id", cursor->id());
    slct.exec();
    if (slct.first())
    {
      if (slct.value("apopen_status").toString() == "H")
      {
        QMessageBox::critical( this, tr("Can not do Payment"), tr( "Item is On Hold" ) );
        return;
      }
      else
      {
        ParameterList params;
        params.append("apopen_id", cursor->id());

        if(_bankaccnt->id() != -1)
          params.append("bankaccnt_id", _bankaccnt->id());

        selectPayment newdlg(this, "", true);
        newdlg.set(params);
        if(newdlg.exec() != XDialog::Rejected)
          update = true;
      }
    }
    else
    {
      QMessageBox::critical( this, tr("Can not do Payment"), tr( "apopen not found" ) );
      return;
    }
  }
  _ignoreUpdates = false;
  if(update)
    sFillList();
}
void selectPayments::sSelectLine()
{
  XSqlQuery selectSelectLine;
  ParameterList params;
  params.append("type", "P");

  int bankaccntid = _bankaccnt->id();
  if(bankaccntid == -1)
  {
    selectBankAccount newdlg(this, "", true);
    newdlg.set(params);
    bankaccntid = newdlg.exec();
  }

  if (bankaccntid != -1)
  {
    bool update = false;
    QList<XTreeWidgetItem*> list = _apopen->selectedItems();
    XTreeWidgetItem * cursor = 0;
    selectSelectLine.prepare("SELECT selectPayment(:apopen_id, :bankaccnt_id) AS result;");
    XSqlQuery slctln;
    slctln.prepare( "SELECT apopen_status FROM apopen WHERE apopen_id=:apopen_id;");
    for(int i = 0; i < list.size(); i++)
    {
      cursor = (XTreeWidgetItem*)list.at(i);
      selectSelectLine.bindValue(":apopen_id", cursor->id());
      selectSelectLine.bindValue(":bankaccnt_id", bankaccntid);
          slctln.bindValue(":apopen_id", cursor->id());
      slctln.exec();
      if (slctln.first())
      {
        if (slctln.value("apopen_status").toString() != "H")
            {
      selectSelectLine.exec();
      if (selectSelectLine.first())
      {
        int result = selectSelectLine.value("result").toInt();
        if (result < 0)
        {
          ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Payment Information"),
                                 storedProcErrorLookup("selectPayment", result),
                                 __FILE__, __LINE__);
          return;
        }
      }
      else if (ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Payment Information"),
                                    selectSelectLine, __FILE__, __LINE__))
      {
        return;
      }
                }
      update = true;
    }
    }
    if(update)
      omfgThis->sPaymentsUpdated(-1, -1, true);
  }
}

void selectPayments::sClear()
{
  XSqlQuery selectClear;
  bool update = false;
  QList<XTreeWidgetItem*> list = _apopen->selectedItems();
  XTreeWidgetItem * cursor = 0;
  selectClear.prepare("SELECT clearPayment(:apopen_id) AS result;");
  for(int i = 0; i < list.size(); i++)
  {
    cursor = (XTreeWidgetItem*)list.at(i);
    selectClear.bindValue(":apopen_id", cursor->altId());
    selectClear.exec();
    if (selectClear.first())
    {
      int result = selectClear.value("result").toInt();
      if (result < 0)
      {
        ErrorReporter::error(QtCriticalMsg, this, cursor->text(0) + " " + cursor->text(2) + "\n" +
                               tr(" Error Clearing Payment Information"),
                               storedProcErrorLookup("clearPayment", result),
                               __FILE__, __LINE__);
        return;
      }
    }
    else if (ErrorReporter::error(QtCriticalMsg, this, tr("Error Clearing Payment Information"),
                                  selectClear, __FILE__, __LINE__))
    {
      return;
    }
    update = true;
  }

  if(update)
    omfgThis->sPaymentsUpdated(-1, -1, true);
}

void selectPayments::sApplyAllCredits()
{
  XSqlQuery selectApplyAllCredits;
  MetaSQLQuery mql = mqlLoad("selectPayments", "applyallcredits");
  ParameterList params;
  if (! setParams(params))
    return;
  selectApplyAllCredits = mql.toQuery(params);
  if (selectApplyAllCredits.first())
    sFillList();
  else
  {
    ErrorReporter::error(QtCriticalMsg, this, tr("Applying all Credits"),
                         selectApplyAllCredits, __FILE__, __LINE__);
    return;
  }
}

void selectPayments::sFillList()
{
  XSqlQuery selectFillList;
  if(_ignoreUpdates)
    return;

//  if (_vendorgroup->isSelectedVend())
//    _apopen->showColumn(9);
//  else
//    _apopen->hideColumn(9);

  if ( (_selectDate->currentIndex() == 1 && !_onOrBeforeDate->isValid())  ||
        (_selectDate->currentIndex() == 2 && (!_startDate->isValid() || !_endDate->isValid())) )
    return;

  int _currid = -1;
  if (_bankaccnt->isValid())
  {
    selectFillList.prepare( "SELECT bankaccnt_curr_id "
               "FROM bankaccnt "
               "WHERE (bankaccnt_id=:bankaccnt_id);" );
    selectFillList.bindValue(":bankaccnt_id", _bankaccnt->id());
    selectFillList.exec();
    if (selectFillList.first())
      _currid = selectFillList.value("bankaccnt_curr_id").toInt();
  }

  ParameterList params;
  if (! setParams(params))
    return;
  params.append("voucher", tr("Voucher"));
  params.append("debitMemo", tr("Debit Memo"));
  params.append("creditMemo", tr("Credit Memo"));
  if (_selectDate->currentIndex()==1)
    params.append("olderDate", _onOrBeforeDate->date());
  else if (_selectDate->currentIndex()==2)
  {
    params.append("startDate", _startDate->date());
    params.append("endDate", _endDate->date());
  }
  if (_currid >= 0)
    params.append("curr_id", _currid);

  MetaSQLQuery mql = mqlLoad("apOpenItems", "selectpayments");
  selectFillList = mql.toQuery(params);
  _apopen->populate(selectFillList,true);
  if (ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Payment Information"),
                                selectFillList, __FILE__, __LINE__))
  {
    return;
  }
}

void selectPayments::sPopulateMenu(QMenu *pMenu,QTreeWidgetItem *selected)
{
  QString status(selected->text(1));
  QAction *menuItem;
  XTreeWidgetItem * item = (XTreeWidgetItem*)selected;

  if (_apopen->currentItem()->text("doctype") == tr("Voucher"))
  {
    menuItem = pMenu->addAction(tr("View Voucher..."), this, SLOT(sViewVoucher()));
    menuItem->setEnabled(_privileges->check("ViewVouchers") || _privileges->check("MaintainVouchers"));

    if(item->rawValue("selected") == 0.0)
    {
      menuItem = pMenu->addAction(tr("Void Voucher..."), this, SLOT(sVoidVoucher()));
      menuItem->setEnabled(_privileges->check("VoidPostedVouchers"));
    }
  }
  
  XSqlQuery menu;
  menu.prepare( "SELECT apopen_status FROM apopen WHERE apopen_id=:apopen_id;");
  menu.bindValue(":apopen_id", _apopen->id());
  menu.exec();
  if (menu.first())
  {
    menuItem = pMenu->addAction(tr("Edit A/P Open..."), this, SLOT(sEdit()));
    menuItem->setEnabled(_privileges->check("EditAPOpenItem"));
    
    pMenu->addAction(tr("View A/P Open..."), this, SLOT(sView()));
    
    menuItem = pMenu->addAction(tr("View G/L Series..."), this, SLOT(sViewGLSeries()));
    menuItem->setEnabled(_privileges->check("ViewGLTransactions"));

    if(menu.value("apopen_status").toString() == "O")
    {
      menuItem = pMenu->addAction(tr("On Hold"), this, SLOT(sOnHold()));
      menuItem->setEnabled(_privileges->check("EditAPOpenItem"));
    }
    if(menu.value("apopen_status").toString() == "H")
    {
      menuItem = pMenu->addAction(tr("Open"), this, SLOT(sOpen()));
      menuItem->setEnabled(_privileges->check("EditAPOpenItem"));
    }
  }
}

void selectPayments::sEdit()
{
  ParameterList params;
  params.append("mode", "edit");
  params.append("apopen_id", _apopen->id());
  
  apOpenItem newdlg(this, "", true);
  newdlg.set(params);
  newdlg.exec();
  
  sFillList();
}

void selectPayments::sView()
{
  ParameterList params;
  params.append("mode", "view");
  params.append("apopen_id", _apopen->id());
  
  apOpenItem newdlg(this, "", true);
  newdlg.set(params);
  newdlg.exec();
}

void selectPayments::sViewGLSeries()
{
  XSqlQuery dspViewGLSeries;
  dspViewGLSeries.prepare("SELECT apopen_distdate, apopen_journalnumber"
                          "  FROM apopen"
                          " WHERE(apopen_id=:apopen_id);");
  dspViewGLSeries.bindValue(":apopen_id", _apopen->id());
  dspViewGLSeries.exec();
  if(dspViewGLSeries.first())
  {
    ParameterList params;
    params.append("startDate", dspViewGLSeries.value("apopen_distdate").toDate());
    params.append("endDate", dspViewGLSeries.value("apopen_distdate").toDate());
    params.append("journalnumber", dspViewGLSeries.value("apopen_journalnumber").toInt());
    
    dspGLSeries *newdlg = new dspGLSeries();
    newdlg->set(params);
    omfgThis->handleNewWindow(newdlg);
  }
  else
    ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Payment Information"),
                       dspViewGLSeries, __FILE__, __LINE__);
}

void selectPayments::sOpen()
{
  XSqlQuery open;
  open.prepare("UPDATE apopen SET apopen_status = 'O', apopen_hold_comment = NULL WHERE apopen_id=:apopen_id;");
  open.bindValue(":apopen_id", _apopen->id());
  open.exec();
  sFillList();
}

void selectPayments::sOnHold()
{
  XSqlQuery selectpayment;
  bool ok;
  QString comment;

  selectpayment.prepare("SELECT * FROM apselect WHERE apselect_apopen_id = :apopen_id;");
  selectpayment.bindValue(":apopen_id", _apopen->id());
  selectpayment.exec();
  if (selectpayment.first())
  {
    QMessageBox::critical( this, tr("Can not change Status"),
                          tr("<p>You cannot set this item as On Hold. "
                             "This Item is already approved for payment."));
    return;
  }

  comment = QInputDialog::getText(this, tr("On Hold Comment"),
        tr("Enter an On Hold Comment:"), QLineEdit::Normal, comment, &ok);
  if(!ok)
    return;

  XSqlQuery onhold;
  onhold.prepare("UPDATE apopen SET apopen_status = 'H', apopen_hold_comment = :comment "
                 " WHERE apopen_id=:apopen_id;");
  onhold.bindValue(":apopen_id", _apopen->id());
  onhold.bindValue(":comment", comment);
  onhold.exec();
  sFillList();
}

void selectPayments::sViewVoucher()
{
  int voheadid;
  int poheadid;
  XSqlQuery open;
  open.prepare("SELECT vohead_id, COALESCE(pohead_id, -1) AS pohead_id "
               "FROM vohead LEFT OUTER JOIN pohead ON (pohead_id=vohead_pohead_id) "
               "WHERE vohead_number=:vohead_number;");
  open.bindValue(":vohead_number", _apopen->currentItem()->text("apopen_docnumber"));
  open.exec();
  if (open.first())
  {
    voheadid = open.value("vohead_id").toInt();
    poheadid = open.value("pohead_id").toInt();
  }
  else
    return;
  
  if (!checkSitePrivs(voheadid))
    return;
  
  ParameterList params;
  params.append("mode", "view");
  params.append("vohead_id", voheadid);
  
  if (poheadid == -1)
  {
    miscVoucher *newdlg = new miscVoucher();
    newdlg->set(params);
    omfgThis->handleNewWindow(newdlg);
  }
  else
  {
    voucher *newdlg = new voucher();
    newdlg->set(params);
    omfgThis->handleNewWindow(newdlg);
  }
}

void selectPayments::sVoidVoucher()
{
  bool update = false;
  QList<XTreeWidgetItem*> list = _apopen->selectedItems();
  XTreeWidgetItem * cursor = 0;
  XSqlQuery dspVoidVoucher;
  dspVoidVoucher.prepare("SELECT voidApopenVoucher(:apopen_id, :voidDate) AS result;");
  for(int i = 0; i < list.size(); i++)
  {
    cursor = (XTreeWidgetItem*)list.at(i);
    if ( (cursor->rawValue("doctype") == tr("Voucher")) && (cursor->rawValue("selected") == 0.0) )
    {
      XDateInputDialog newdlg(this, "", true);
      ParameterList params;
      params.append("label", tr("On what date did you void the Voucher?"));
      params.append("default", cursor->rawValue("apopen_docdate"));
      newdlg.set(params);
      int returnVal = newdlg.exec();
      if (returnVal == XDialog::Accepted)
      {
        QDate voidDate = newdlg.getDate();
        dspVoidVoucher.bindValue(":apopen_id", cursor->id());
        dspVoidVoucher.bindValue(":voidDate", voidDate);
        dspVoidVoucher.exec();
      
        if(dspVoidVoucher.first())
        {
          if(dspVoidVoucher.value("result").toInt() < 0)
          {
            ErrorReporter::error(QtCriticalMsg, this, tr("Error Voiding Voucher"),
                                 dspVoidVoucher, __FILE__, __LINE__);
            return;
          }
        }
        else
        {
          ErrorReporter::error(QtCriticalMsg, this, tr("Voiding Voucher"),
                               dspVoidVoucher, __FILE__, __LINE__);
          return;
        }
        update = true;
      }
    }
  }
  if(update)
    sFillList();
}

bool selectPayments::checkSitePrivs(int orderid)
{
  if (_preferences->boolean("selectedSites"))
  {
    XSqlQuery check;
    check.prepare("SELECT checkVoucherSitePrivs(:voheadid) AS result;");
    check.bindValue(":voheadid", orderid);
    check.exec();
    if (check.first())
    {
      if (!check.value("result").toBool())
      {
        QMessageBox::critical(this, tr("Access Denied"),
                              tr("<p>You may not view or edit this Voucher as "
                                 "it references a Site for which you have not "
                                 "been granted privileges.")) ;
        return false;
      }
    }
    else if (ErrorReporter::error(QtCriticalMsg, this, tr("Checking Privileges"),
                                  check, __FILE__, __LINE__))
      return false;
  }
  return true;
}
