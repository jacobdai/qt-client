/*
 * This file is part of the xTuple ERP: PostBooks Edition, a free and
 * open source Enterprise Resource Planning software suite,
 * Copyright (c) 1999-2014 by OpenMFG LLC, d/b/a xTuple.
 * It is licensed to you under the Common Public Attribution License
 * version 1.0, the full text of which (including xTuple-specific Exhibits)
 * is available at www.xtuple.com/CPAL.  By using this software, you agree
 * to be bound by its terms.
 */

#include "unpostedInvoices.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QSqlError>
#include <QVariant>

#include <metasql.h>
#include  <openreports.h>

#include "failedPostList.h"
#include "getGLDistDate.h"
#include "invoice.h"
#include "mqlutil.h"
#include "printInvoice.h"
#include "storedProcErrorLookup.h"
#include "distributeInventory.h"
#include "errorReporter.h"

unpostedInvoices::unpostedInvoices(QWidget* parent, const char* name, Qt::WindowFlags fl)
    : display(parent, "unpostedInvoices", fl)
{
  if (name)
    setObjectName(name);

  setupUi(optionsWidget());
  setWindowTitle(tr("Unposted Invoices"));
  setListLabel(tr("Unposted Invoices"));
  setMetaSQLOptions("invoices", "detail");
  setNewVisible(true);

  list()->addColumn(tr("Invoice #"),     _orderColumn,    Qt::AlignLeft,   true,  "invchead_invcnumber" );
  list()->addColumn(tr("Prnt'd"),        _orderColumn,    Qt::AlignCenter, true,  "invchead_printed" );
  list()->addColumn(tr("S/O #"),         _orderColumn,    Qt::AlignLeft,   true,  "invchead_ordernumber" );
  list()->addColumn(tr("Customer"),      -1,              Qt::AlignLeft,   true,  "cust_name" );
  list()->addColumn(tr("Ship-to"),       100,             Qt::AlignLeft,   false, "invchead_shipto_name" );
  list()->addColumn(tr("Invc. Date"),    _dateColumn,     Qt::AlignCenter, true,  "invchead_invcdate" );
  list()->addColumn(tr("Ship Date"),     _dateColumn,     Qt::AlignCenter, true,  "invchead_shipdate" );
  list()->addColumn(tr("G/L Dist Date"), _dateColumn,     Qt::AlignCenter, true,  "gldistdate" );
  list()->addColumn(tr("Recurring"),     _ynColumn,       Qt::AlignCenter, false, "isRecurring" );
  list()->addColumn(tr("Ship Date"),     _dateColumn,     Qt::AlignCenter, false, "invchead_shipdate" );
  list()->addColumn(tr("P/O #"),         _orderColumn,    Qt::AlignCenter, false, "invchead_ponumber" );
  list()->addColumn(tr("Total Amount"),  _bigMoneyColumn, Qt::AlignRight,  true,  "extprice" );
  list()->addColumn(tr("Currency"),      _currencyColumn, Qt::AlignLeft,   true,  "curr" );
  list()->addColumn(tr("Margin"),        _bigMoneyColumn, Qt::AlignRight,  true,  "margin" );
  list()->addColumn(tr("Margin %"),      _prcntColumn,    Qt::AlignRight,  true,  "marginpercent");
  list()->setSelectionMode(QAbstractItemView::ExtendedSelection);

  if (! _privileges->check("ChangeARInvcDistDate"))
    list()->hideColumn(7);

  if (_privileges->check("MaintainMiscInvoices"))
    connect(list(), SIGNAL(itemSelected(int)), this, SLOT(sEdit()));
  else
  {
    newAction()->setEnabled(false);
    connect(list(), SIGNAL(itemSelected(int)), this, SLOT(sView()));
  }

  if (_preferences->boolean("XCheckBox/forgetful"))
    _printJournal->setChecked(true);

  connect(omfgThis, SIGNAL(invoicesUpdated(int, bool)), this, SLOT(sFillList()));

  sFillList();
}

void unpostedInvoices::sNew()
{
  invoice::newInvoice(-1);
}

void unpostedInvoices::sEdit()
{
  QList<XTreeWidgetItem*> selected = list()->selectedItems();
  for (int i = 0; i < selected.size(); i++)
      if (checkSitePrivs(((XTreeWidgetItem*)(selected[i]))->id()))
        invoice::editInvoice(((XTreeWidgetItem*)(selected[i]))->id());
      
}

void unpostedInvoices::sView()
{
  QList<XTreeWidgetItem*> selected = list()->selectedItems();
  for (int i = 0; i < selected.size(); i++)
      if (checkSitePrivs(((XTreeWidgetItem*)(selected[i]))->id()))
        invoice::viewInvoice(((XTreeWidgetItem*)(selected[i]))->id());
}

void unpostedInvoices::sDelete()
{
  XSqlQuery unpostedDelete;
  if ( QMessageBox::warning( this, tr("Delete Selected Invoices"),
                             tr("<p>Are you sure that you want to delete the "
			        "selected Invoices?"),
                             tr("Delete"), tr("Cancel"), QString::null, 1, 1 ) == 0)
  {
    unpostedDelete.prepare("SELECT deleteInvoice(:invchead_id) AS result;");

    QList<XTreeWidgetItem*> selected = list()->selectedItems();
    for (int i = 0; i < selected.size(); i++)
    {
      if (checkSitePrivs(((XTreeWidgetItem*)(selected[i]))->id()))
	  {
        unpostedDelete.bindValue(":invchead_id", ((XTreeWidgetItem*)(selected[i]))->id());
        unpostedDelete.exec();
        if (unpostedDelete.first())
        {
	      int result = unpostedDelete.value("result").toInt();
	      if (result < 0)
          {
            ErrorReporter::error(QtCriticalMsg, this, tr("Error Deleting Invoice Information"),
                                   storedProcErrorLookup("deleteInvoice", result),
                                   __FILE__, __LINE__);
          }
        }
        else if (unpostedDelete.lastError().type() != QSqlError::NoError)
          ErrorReporter::error(QtCriticalMsg, this, tr("Error Deleting Invoice Information: %1\n")
                             .arg(selected[i]->text(0)) + unpostedDelete.lastError().databaseText(),
                             unpostedDelete, __FILE__, __LINE__);
      }
    }

    omfgThis->sInvoicesUpdated(-1, true);
    omfgThis->sBillingSelectionUpdated(-1, -1);
  }
}

void unpostedInvoices::sPrint()
{
  QList<XTreeWidgetItem*> selected = list()->selectedItems();
  printInvoice newdlg(this, "", true);

  for (int i = 0; i < selected.size(); i++)
  {
    if (checkSitePrivs(((XTreeWidgetItem*)(selected[i]))->id()))
    {
      ParameterList params;
      params.append("invchead_id", ((XTreeWidgetItem*)(selected[i]))->id());
      params.append("persistentPrint");

      newdlg.set(params);

      if (!newdlg.isSetup())
      {
        if(newdlg.exec() == QDialog::Rejected)
          break;
        newdlg.setSetup(true);
      }
    }
  }

  omfgThis->sInvoicesUpdated(-1, true);
}

void unpostedInvoices::sPost()
{
  XSqlQuery unpostedPost;
  bool changeDate = false;
  QDate newDate = QDate();

  if (_privileges->check("ChangeARInvcDistDate"))
  {
    getGLDistDate newdlg(this, "", true);
    newdlg.sSetDefaultLit(tr("Invoice Date"));
    if (newdlg.exec() == XDialog::Accepted)
    {
      newDate = newdlg.date();
      changeDate = (newDate.isValid());
    }
    else
      return;
  }

  int journal = -1;
  unpostedPost.exec("SELECT fetchJournalNumber('AR-IN') AS result;");
  if (unpostedPost.first())
  {
    journal = unpostedPost.value("result").toInt();
    if (journal < 0)
    {
      ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Journal Number"),
                             storedProcErrorLookup("fetchJournalNumber", journal),
                             __FILE__, __LINE__);
      return;
    }
  }
  else if (ErrorReporter::error(QtCriticalMsg, this, tr("Error Retrieving Journal Number"),
                                unpostedPost, __FILE__, __LINE__))
  {
    return;
  }

  XSqlQuery xrate;
  xrate.prepare("SELECT curr_rate "
		"FROM curr_rate, invchead "
		"WHERE ((curr_id=invchead_curr_id)"
		"  AND  (invchead_id=:invchead_id)"
		"  AND  (invchead_invcdate BETWEEN curr_effective AND curr_expires));");
  // if SUM becomes dependent on curr_id then move XRATE before it in the loop
  XSqlQuery sum;
  sum.prepare("SELECT invoicetotal(invchead_id) AS subtotal, "
              " invchead_invcnumber "
              "FROM invchead "
              "WHERE invchead_id=:invchead_id;");

  XSqlQuery setDate;
  setDate.prepare("UPDATE invchead SET invchead_gldistdate=:distdate "
		  "WHERE invchead_id=:invchead_id;");

  QList<XTreeWidgetItem*> selected = list()->selectedItems();
  // Update the invoice gl distribution dates
  for (int i = 0; i < selected.size(); i++)
  {
    if (checkSitePrivs(((XTreeWidgetItem*)(selected[i]))->id()))
    {
      int id = ((XTreeWidgetItem*)(selected[i]))->id();

      // always change gldistdate.  if using invoice date then will set to null
      setDate.bindValue(":distdate",    newDate);
      setDate.bindValue(":invchead_id", id);
      setDate.exec();
      if (setDate.lastError().type() != QSqlError::NoError)
      {
        ErrorReporter::error(QtCriticalMsg, this, tr("Error Posting Invoice Information"),
                             setDate, __FILE__, __LINE__);
      }
    }
  }

  int succeeded = 0;
  QList<QString> failedInvoices;
  QList<QString> errors;
  for (int i = 0; i < selected.size(); i++)
  {
    int invoiceId = ((XTreeWidgetItem*)(selected[i]))->id();
    QString invoiceNumber;

    // Validate total and ask user to continue if 0, validate currency and exchange rate
    sum.bindValue(":invchead_id", invoiceId);
    if (sum.exec() && sum.first())
    {
      invoiceNumber = sum.value("invchead_invcnumber").toString();

      if (sum.value("subtotal").toDouble() == 0 && QMessageBox::question(this, tr("Invoice Has Value 0"),
              tr("Invoice #%1 has a total value of 0.\n"
               "Would you like to post it anyway?")
          .arg(selected[i]->text(0)),
        QMessageBox::Yes,
        QMessageBox::No | QMessageBox::Default)
      == QMessageBox::No)
        continue;
    }
    else if (ErrorReporter::error(QtCriticalMsg, this, tr("Error Posting Invoice Information"),
                                    sum, __FILE__, __LINE__))
    {
      continue;
    }
    else if (sum.value("subtotal").toDouble() != 0)
    {
      xrate.bindValue(":invchead_id", invoiceId);
      xrate.exec();
      if (xrate.lastError().type() != QSqlError::NoError)
      {
          ErrorReporter::error(QtCriticalMsg, this, tr("Error Posting Invoice Information %1\n%2")
                               .arg(selected[i]->text(0))
                               .arg(xrate.lastError().databaseText()),
                               xrate, __FILE__, __LINE__);
        continue;
      }
      else if (!xrate.first() || xrate.value("curr_rate").isNull())
      {
          ErrorReporter::error(QtCriticalMsg, this, tr("Error Occurred"),
                               tr("%1: Could not post Invoice #%1 because of a missing exchange rate.")
                               .arg(windowTitle()),__FILE__,__LINE__);
        continue;
      }
    }

    // Set the series for this invoice
    int itemlocSeries = distributeInventory::SeriesCreate(0, 0, QString(), QString());
    if (itemlocSeries < 0)
    {
      failedInvoices.append(invoiceNumber);
      errors.append(tr("Failed to create a new series for the invoice."));
      continue;
    }

    XSqlQuery cleanup; // Stage cleanup function to be called on error
    cleanup.prepare("SELECT deleteitemlocseries(:itemlocSeries, TRUE);");
    cleanup.bindValue(":itemlocSeries", itemlocSeries);

    bool invoiceLineFailed = false;
    bool hasControlledItems = false;

    // Handle the Inventory and G/L Transactions for any billed Inventory where invcitem_updateinv is true
    XSqlQuery items;
    items.prepare("SELECT item_number, itemsite_id, invcitem_id, "
                  " (invcitem_billed * invcitem_qty_invuomratio) * -1 AS qty, "
                  " invchead_invcnumber "
                  "FROM invchead " 
                  " JOIN invcitem ON invcitem_invchead_id = invchead_id "
                  "   AND invcitem_billed <> 0 " 
                  "   AND invcitem_updateinv "
                  " JOIN itemsite ON itemsite_item_id = invcitem_item_id " 
                  "   AND itemsite_warehous_id = invcitem_warehous_id "
                  " JOIN item ON item_id = invcitem_item_id "
                  "WHERE invchead_id = :invchead_id "
                  " AND itemsite_costmethod != 'J' "
                  " AND (itemsite_loccntrl OR itemsite_controlmethod IN ('L', 'S')) "
                  " AND itemsite_controlmethod != 'N' "
                  "ORDER BY invcitem_id;");
    items.bindValue(":invchead_id", invoiceId);
    items.exec();
    while (items.next())
    {
      int result = distributeInventory::SeriesCreate(items.value("itemsite_id").toInt(), 
        items.value("qty").toDouble(), "IN", "SH", items.value("invcitem_id").toInt(), itemlocSeries);
      if (result < 0)
      {
        cleanup.exec();
        failedInvoices.append(invoiceNumber);
        errors.append(tr("Error Creating itemlocdist Record for item %1").arg(items.value("item_number").toString()));
        invoiceLineFailed = true;
        break;
      }
      else if (itemlocSeries == 0) // The first time through the loop, set itemlocSeries
        itemlocSeries = result;

      hasControlledItems = true;
    }

    if (invoiceLineFailed)
      continue;

    // Distribute the items from above
    if (hasControlledItems && distributeInventory::SeriesAdjust(itemlocSeries, this, QString(), QDate(), QDate(), true)
      == XDialog::Rejected)
    {
      cleanup.exec();
      // If it's not the last invoice in the loop, ask the user to exit loop or continue
      if (i != (selected.size() -1))
      {
        if (QMessageBox::question(this,  tr("Post Invoices"),
          tr("Posting distribution detail for invoice number %1 was cancelled but "
             "there other invoices to Post. Continue posting the remaining invoices?")
          .arg(invoiceNumber), 
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
        {
          failedInvoices.append(invoiceNumber);
          errors.append(tr("Detail Distribution Cancelled"));
          continue;
        }
        else
        {
          failedInvoices.append(invoiceNumber);
          errors.append(tr("Detail Distribution Cancelled"));
          break;
        }
      }
    }

    // Post this invoice
    XSqlQuery rollback;
    rollback.prepare("ROLLBACK;");

    unpostedPost.exec("BEGIN;"); // TODO - remove this after postInvoice function no longer returns negative error codes

    XSqlQuery post;
    post.prepare("SELECT postInvoice(:invchead_id, :journal, :itemlocSeries, TRUE) AS result;");
    post.bindValue(":invchead_id", invoiceId);
    post.bindValue(":journal",     journal);
    post.bindValue(":itemlocSeries", itemlocSeries);
    post.exec();
    if (post.first())
    {
      int result = post.value("result").toInt();
      if (result < 0 || result != itemlocSeries)
      {
        rollback.exec();
        cleanup.exec();
        failedInvoices.append(invoiceNumber);
        errors.append(tr("Error Posting Invoice %1").arg(storedProcErrorLookup("postInvoice", result))); 
        continue;
      }

      unpostedPost.exec("COMMIT;");
    }
    else 
    {
      rollback.exec();
      cleanup.exec();
      failedInvoices.append(invoiceNumber);
      errors.append(tr("Error Posting Invoice %1").arg(post.lastError().databaseText())); 
      continue;
    }
  } // invoice loop

  if (_printJournal->isChecked())
  {
    ParameterList params;
    params.append("source", "A/R");
    params.append("sourceLit", tr("A/R"));
    params.append("startJrnlnum", journal);
    params.append("endJrnlnum", journal);

    if (_metrics->boolean("UseJournals"))
    {
      params.append("title",tr("Journal Series"));
      params.append("table", "sltrans");
    }
    else
    {
      params.append("title",tr("General Ledger Series"));
      params.append("gltrans", true);
      params.append("table", "gltrans");
    }

    orReport report("GLSeries", params);
    if (report.isValid())
      report.print();
    else
      report.reportError(this);
  }

  omfgThis->sInvoicesUpdated(-1, true);
}

void unpostedInvoices::sPopulateMenu(QMenu * pMenu, QTreeWidgetItem *, int)
{
  QAction *menuItem;

  menuItem = pMenu->addAction(tr("Edit..."), this, SLOT(sEdit()));
  if (!_privileges->check("MaintainMiscInvoices"))
    menuItem->setEnabled(false);

  menuItem = pMenu->addAction(tr("View..."), this, SLOT(sView()));
  if ((!_privileges->check("MaintainMiscInvoices")) && (!_privileges->check("ViewMiscInvoices")))
    menuItem->setEnabled(false);

  menuItem = pMenu->addAction(tr("Delete..."), this, SLOT(sDelete()));
  if (!_privileges->check("MaintainMiscInvoices"))
    menuItem->setEnabled(false);

  pMenu->addSeparator();

  menuItem = pMenu->addAction(tr("Print..."), this, SLOT(sPrint()));
  if (!_privileges->check("PrintInvoices"))
    menuItem->setEnabled(false);

  menuItem = pMenu->addAction(tr("Post..."), this, SLOT(sPost()));
  if (!_privileges->check("PostMiscInvoices"))
    menuItem->setEnabled(false);
}

bool unpostedInvoices::checkSitePrivs(int invcid)
{
  if (_preferences->boolean("selectedSites"))
  {
    XSqlQuery check;
    check.prepare("SELECT checkInvoiceSitePrivs(:invcheadid) AS result;");
    check.bindValue(":invcheadid", invcid);
    check.exec();
    if (check.first())
    {
    if (!check.value("result").toBool())
      {
        QMessageBox::critical(this, tr("Access Denied"),
                                       tr("You may not view or edit this Invoice as it references "
                                       "a Site for which you have not been granted privileges.")) ;
        return false;
      }
    }
  }
  return true;
}

void unpostedInvoices::sFillList()
{
  ParameterList params;
  params.append("unpostedOnly");
  display::sFillList(params, true);
}
