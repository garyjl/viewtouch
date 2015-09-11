/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  
  
 *   This program is free software: you can redistribute it and/or modify 
 *   it under the terms of the GNU General Public License as published by 
 *   the Free Software Foundation, either version 3 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful, 
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *   GNU General Public License for more details. 
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * settings.cc - revision 103 (10/7/98)
 * Implementation of settings module
 */

#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "check.hh"
#include "conf_file.hh"
#include "data_file.hh"
#include "labels.hh"
#include "manager.hh"
#include "printer.hh"
#include "report.hh"
#include "sales.hh"
#include "settings.hh"
#include "settings_zone.hh"
#include "system.hh"
#include "terminal.hh"
#include "utility.hh"
#include "video_zone.hh"



#define CONFIG_TAX_FILE VIEWTOUCH_PATH "/dat/conf/tax.ini"
#define CONFIG_FEES_FILE VIEWTOUCH_PATH "/dat/conf/fees.ini"
#define CONFIG_FASTFOOD_FILE VIEWTOUCH_PATH "/dat/conf/fastfood.ini"

#include <iostream> // temp

/*********************************************************************
 * Global Data
 ********************************************************************/

const char* StoreName[] = {
    "Other", "SunWest", NULL};
int StoreValue[] = {
    STORE_OTHER, STORE_SUNWEST, -1};

const char* PayPeriodName[] = {
    "Weekly", "2 Week", "4 Week", "Semi Monthly", "Semi Monthly 11/26", "Monthly", NULL};
int PayPeriodValue[] = {
    PERIOD_WEEK, PERIOD_2WEEKS, PERIOD_4WEEKS,
    PERIOD_HALFMONTH, PERIOD_HM_11, PERIOD_MONTH, -1};

const char* MealStartName[] = {
    "Breakfast", "Brunch", "Lunch", "Early Dinner",
    "Dinner", "Late Night", NULL};
int MealStartValue[] = {
    INDEX_BREAKFAST, INDEX_BRUNCH, INDEX_LUNCH,
    INDEX_EARLY_DINNER, INDEX_DINNER, INDEX_LATE_NIGHT, -1};

const char* DrawerModeName[] = {
    "Normal", "Assigned", "Server Bank", NULL};
int   DrawerModeValue[] = {
    DRAWER_NORMAL, DRAWER_ASSIGNED, DRAWER_SERVER, -1};

const char* SaleCreditName[] = {
    "First Server", "Last Server", NULL};
int SaleCreditValue[] = {
    1, 0, -1};

const char* SalesPeriodName[] = {
    "None", "1 Week", "2 Weeks", "4 Weeks", "Once/Month", "11/26", NULL};
int SalesPeriodValue[] = {
    SP_NONE, SP_WEEK, SP_2WEEKS, SP_4WEEKS, SP_MONTH, SP_HM_11, -1};

const char* ReceiptPrintName[] = {
    "On Send", "On Finalize", "On Both", "Never", NULL};
int ReceiptPrintValue[] = {
    RECEIPT_SEND, RECEIPT_FINALIZE, RECEIPT_BOTH, RECEIPT_NONE, -1};

const char* DrawerPrintName[] = {
    "On Pull", "On Balance", "On Both", "Never", NULL};
int DrawerPrintValue[] = {
    DRAWER_PRINT_PULL, DRAWER_PRINT_BALANCE, DRAWER_PRINT_BOTH, DRAWER_PRINT_NEVER, -1};

const char* RoundingName[] = {
    "None", "Drop Pennies", "Round Up Gratuity", NULL};
int RoundingValue[] = {
    ROUNDING_NONE, ROUNDING_DROP_PENNIES, ROUNDING_UP_GRATUITY, -1};

const char* PrinterName[] = {
    "None", "Kitchen1", "Kitchen2", "Bar1", "Bar2", "Expediter",
    "Kitchen1 notify2", "Kitchen2 notify1", "Remote Order",
    "Default", NULL};
int PrinterValue[] = {
    PRINTER_NONE, PRINTER_KITCHEN1, PRINTER_KITCHEN2, PRINTER_BAR1,
    PRINTER_BAR2, PRINTER_EXPEDITER,
    PRINTER_KITCHEN1_NOTIFY, PRINTER_KITCHEN2_NOTIFY,
    PRINTER_REMOTEORDER, PRINTER_DEFAULT, -1};

const char* MeasureSystemName[] = {"Standard U.S.", "Metric", NULL};
int   MeasureSystemValue[] = {MEASURE_STANDARD, MEASURE_METRIC, -1};

const char* DateFormatName[] = {"MM/DD/YY", "DD/MM/YY", NULL };
int   DateFormatValue[] = {DATE_MMDDYY, DATE_DDMMYY, -1};

const char* NumberFormatName[] = {"1,000,000.00", "1.000.000,00", NULL};
int   NumberFormatValue[] = {NUMBER_STANDARD, NUMBER_EURO, -1};

const char* TimeFormatName[] = {"12 hour", "24 hour", NULL};
int   TimeFormatValue[] = {TIME_12HOUR, TIME_24HOUR, -1};

#ifdef CREDITMCVE
const char* AuthorizeName[] = {"None", "MainStreet", NULL};
int   AuthorizeValue[] = {CCAUTH_NONE, CCAUTH_MAINSTREET, -1};
int   ccauth_defined = CCAUTH_MAINSTREET;
#elif defined CREDITCHEQ
const char* AuthorizeName[] = {"None", "CreditCheq", NULL};
int   AuthorizeValue[] = {CCAUTH_NONE, CCAUTH_CREDITCHEQ, -1};
int   ccauth_defined = CCAUTH_CREDITCHEQ;
#else
const char* AuthorizeName[] = {"None", NULL};
int   AuthorizeValue[] = {CCAUTH_NONE, -1};
int   ccauth_defined = CCAUTH_NONE;
#endif

const char* MarkName[] = {" ", "X", NULL};
int   MarkValue[] = {0, 1, -1};

const char* HourName[] = {
    "12am", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
    "12pm", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
    "12am", NULL};

int WeekDays[] = { WEEKDAY_SUNDAY, WEEKDAY_MONDAY, WEEKDAY_TUESDAY,
                   WEEKDAY_WEDNESDAY, WEEKDAY_THURSDAY, WEEKDAY_FRIDAY,
                   WEEKDAY_SATURDAY, -1 };

namespace confmap
{
    enum variable_keys
    {
        V_GST = 0, V_PST, V_HST, V_QST, V_ROYALTY_RATE, V_ADVERTISE_FUND,
        V_DAILY_CERT_FEE, V_DEBIT_COST, V_CREDIT_RATE, V_CREDIT_COST,
        V_LINE_ITEM_COST, V_TAX_TAKEOUT_FOOD, V_PERSONALIZE_FAST_FOOD,
    V_FOOD_INCLUSIVE, V_ALCOHOL_INCLUSIVE, V_MERCHANDISE_INCLUSIVE, 
    V_ROOM_INCLUSIVE
    };

    enum section_titles
    {
        S_SALES_TAX_CANADA = 0, S_MISC, S_ELEC_TRANS,
    };

    const char* vars[] =
    {
        "GST", "PST", "HST", "QST", "royalty_rate", "advertise_fund",
        "daily_cert_fee", "debit_cost", "credit_rate", "credit_cost",
        "line_item_cost", "tax_takeout_food", "personalize_fast_food",
    "food_inclusive", "alcohol_inclusive", "merchandise_inclusive",
    "room_inclusive"
    };

    const char* sects[] =
    {
        "Sales Tax Canada", "Miscellany", "Electronic Transactions",
    };
}

/*********************************************************************
 * MoneyInfo Class
 ********************************************************************/

MoneyInfo::MoneyInfo()
{
    next = NULL;
    fore = NULL;
    id = -1;
    rounding = 0;
    round_amount = 1;
    exchange = 1.0;
}

int MoneyInfo::Read(InputDataFile &df, int version)
{
    int error = 0;
    error += df.Read(id);
    error += df.Read(name);
    error += df.Read(short_name);
    error += df.Read(symbol);
    error += df.Read(rounding);
    error += df.Read(round_amount);
    error += df.Read(exchange);
    return error;
}

int MoneyInfo::Write(OutputDataFile &df, int version)
{
    int error = 0;
    error += df.Write(id);
    error += df.Write(name);
    error += df.Write(short_name);
    error += df.Write(symbol);
    error += df.Write(rounding);
    error += df.Write(round_amount);
    error += df.Write(exchange);
    return error;
}


/*********************************************************************
 * TaxInfo Class
 ********************************************************************/

TaxInfo::TaxInfo()
{
    next = NULL;
    fore = NULL;
    id = -1;
    flags = 0;
    amount = 0;
}

int TaxInfo::Read(InputDataFile &df, int version)
{
    int error = 0;
    error += df.Read(id);
    error += df.Read(name);
    error += df.Read(short_name);
    error += df.Read(flags);
    error += df.Read(amount);
    return error;
}

int TaxInfo::Write(OutputDataFile &df, int version)
{
    int error = 0;
    error += df.Write(id);
    error += df.Write(name);
    error += df.Write(short_name);
    error += df.Write(flags);
    error += df.Write(amount);
    return error;
}

/*********************************************************************
 * MediaInfo Class
 ********************************************************************/

MediaInfo::MediaInfo()
{
    FnTrace("MediaInfo::MediaInfo()");
    id     = -1;
    local  = 1;
}


/*********************************************************************
 * DiscountInfo Class
 ********************************************************************/

DiscountInfo::DiscountInfo()
{
    amount = 0;
    flags  = 0;
    active = 1;
}

DiscountInfo *DiscountInfo::Copy()
{
    FnTrace("DiscountInfo::Copy()");
    DiscountInfo *retdiscount = new DiscountInfo();
    retdiscount->name.Set(name);
    retdiscount->id = id;
    retdiscount->amount = amount;
    retdiscount->flags = flags;
    retdiscount->local = local;
    return retdiscount;
}

int DiscountInfo::Read(InputDataFile &df, int version)
{
    FnTrace("DiscountInfo::Read()");
    int error = 0;
    error += df.Read(id);
    error += df.Read(name);
    error += df.Read(flags);
    error += df.Read(amount);
    if (version >= 40)
        error += df.Read(local);
    if (version >= 43)
        error += df.Read(active);
    return error;
}

int DiscountInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("DiscountInfo::Write()");
    int error = 0;
    error += df.Write(id);
    error += df.Write(name);
    error += df.Write(flags);
    error += df.Write(amount);
    error += df.Write(local);
    error += df.Write(active);
    return error;
}


/*********************************************************************
 * CompInfo Class
 ********************************************************************/

CompInfo::CompInfo()
{
    flags = 0;
    active = 1;
}

CompInfo *CompInfo::Copy()
{
    FnTrace("CompInfo::Copy()");
    CompInfo *retcomp = new CompInfo();
    retcomp->name.Set(name);
    retcomp->id = id;
    retcomp->flags = flags;
    retcomp->local = local;
    return retcomp;
}

int CompInfo::Read(InputDataFile &df, int version)
{
    FnTrace("CompInfo::Read()");
    int error = 0;
    error += df.Read(id);
    error += df.Read(name);
    error += df.Read(flags);
    if (version >= 40)
        error += df.Read(local);
    if (version >= 43)
        error += df.Read(active);
    return error;
}

int CompInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("CompInfo::Write()");
    int error = 0;
    error += df.Write(id);
    error += df.Write(name);
    error += df.Write(flags);
    error += df.Write(local);
    error += df.Write(active);
    return error;
}


/*********************************************************************
 * CouponInfo Class
 ********************************************************************/

CouponInfo::CouponInfo()
{
    FnTrace("CouponInfo::CouponInfo()");
    amount = 0;
    flags  = 0;
    active = 1;
    family = 0;
    item_id = 0;
    item_name.Set("");
    start_time.Clear();
    end_time.Clear();
    start_date.Clear();
    end_date.Clear();
    days = 0;
    months = 0;
    start_time.Clear();
    end_time.Clear();
    days = 0;
    months = 0;
    automatic = 0;
}

CouponInfo *CouponInfo::Copy()
{
    FnTrace("CouponInfo::Copy()");
    CouponInfo *retcoupon = new CouponInfo();

    retcoupon->name.Set(name);
    retcoupon->id = id;
    retcoupon->amount = amount;
    retcoupon->flags = flags;
    retcoupon->local = local;
    retcoupon->family = family;
    retcoupon->item_id = item_id;
    retcoupon->item_name.Set(item_name);
    retcoupon->start_time = start_time;
    retcoupon->end_time = end_time;
    retcoupon->start_date = start_date;
    retcoupon->end_date = end_date;
    retcoupon->days = days;
    retcoupon->months = months;
    retcoupon->automatic = automatic;

    return retcoupon;
}

int CouponInfo::Read(InputDataFile &df, int version)
{
    FnTrace("CouponInfo::Read()");
    int error = 0;
    int temp;  // for obsolete items

    error += df.Read(id);
    error += df.Read(name);
    error += df.Read(flags);
    error += df.Read(amount);
    if (version >= 40)
        error += df.Read(local);
    if (version >= 43)
        error += df.Read(active);
    if (version >= 75)
    {
        if (version < 80)
        {
            error += df.Read(temp);  // obsolete item_specific
            if (temp)
                flags |= TF_ITEM_SPECIFIC;
            error += df.Read(temp);  // obsolete apply_to_all
            if (temp)
                flags |= TF_APPLY_EACH;
        }
        error += df.Read(family);
        error += df.Read(item_id);
    }
    if (version >= 76)
    {
        if (version < 78)
            error += df.Read(temp);  // obsolete datetime, never used
        error += df.Read(start_time);
        error += df.Read(end_time);
        error += df.Read(days);
        error += df.Read(months);
    }
    if (version >= 77)
        error += df.Read(automatic);
    if (version >= 78)
    {
        error += df.Read(start_date);
        error += df.Read(end_date);
    }
    if (version >= 81)
        error += df.Read(item_name);

    return error;
}

int CouponInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("CouponInfo::Write()");
    int error = 0;

    error += df.Write(id);
    error += df.Write(name);
    error += df.Write(flags);
    error += df.Write(amount);
    error += df.Write(local);
    error += df.Write(active);
    error += df.Write(family);
    error += df.Write(item_id);
    error += df.Write(start_time);
    error += df.Write(end_time);
    error += df.Write(days);
    error += df.Write(months);
    error += df.Write(automatic);
    error += df.Write(start_date);
    error += df.Write(end_date);
    error += df.Write(item_name);

    return error;
}

int CouponInfo::Apply(SubCheck *subcheck, Payment *payment)
{
    FnTrace("CouponInfo::Apply()");
    int retval = 0;
    Order *order = subcheck->OrderList();
    SalesItem *item = NULL;
    int payment_value = 0;

    while (order != NULL)
    {
        if (order->IsReduced() == 0 && order->IsEmployeeMeal() == 0)
        {
            item = order->Item(&MasterSystem->menu);
            if (Applies(item, automatic))
            {
                order->IsReduced(1);
                payment_value += CPAmount(order->item_cost, order->count);
            }
        }
        order = order->next;
    }
    if (payment == NULL)
        payment = subcheck->FindPayment(TENDER_COUPON, id);
    if (payment != NULL)
    {
        payment->amount = amount;
        payment->value = payment_value;
    }

    return retval;
}

/****
 * Applies:  This method actually serves two purposes.  It will return
 *  a positive integer if the coupon is valid for this subcheck.  The
 *  positive integer will actually be the count of items to which
 *  this coupon can be applied.
 ****/
int CouponInfo::Applies(SubCheck *subcheck, int aut)
{
    FnTrace("CouponInfo::Applies(SubCheck *, int)");
    int retval = 1;

    if (active == 0)
        retval = 0;
    else if (aut != automatic)
        retval = 0;
    else
    {
        retval = AppliesTime();
        if (retval && (flags & TF_ITEM_SPECIFIC))
            retval = AppliesItem(subcheck);
    }

    return retval;
}

int CouponInfo::Applies(SalesItem *item, int aut)
{
    FnTrace("CouponInfo::Applies(SalesItem *, int)");
    int retval = 0;

    if (item == NULL)
        retval = 0;
    else if (active == 0)
        retval = 0;
    else if (aut != automatic)
        retval = 0;
    else
    {
        retval = AppliesTime();
        if (retval)
            retval = AppliesItem(item);
    }

    return retval;
}

int CouponInfo::AppliesTime()
{
    FnTrace("CouponInfo::AppliesTime()");
    int retval = 1;

    int day = SystemTime.WeekDay();

    // first check the date and time
    if (retval &&start_date.IsSet() && end_date.IsSet())
    {
        if (SystemTime < start_date || SystemTime > end_date)
            retval = 0;
    }
    if (retval && start_time.IsSet() && end_time.IsSet())
    {
        // start_time and end_time may have year, weekday, etc. set and we
        // don't care about those.  So use blank TimeInfo instances.
        TimeInfo now;
        TimeInfo start;
        TimeInfo end;

        now.Clear();
        now.Hour(SystemTime.Hour());
        now.Min(SystemTime.Min());

        start.Clear();
        start.Hour(start_time.Hour());
        start.Min(start_time.Min());

        end.Clear();
        end.Hour(end_time.Hour());
        end.Min(end_time.Min());

        if (now < start || now > end)
            retval = 0;
    }

    // now check the weekday
    if (retval && days && !(days & (WeekDays[day])))
        retval = 0;

    return retval;
}

int CouponInfo::AppliesItem(SubCheck *subcheck)
{
    FnTrace("CouponInfo::AppliesItem()");
    int retval = 0;
    Order *order = NULL;

    if (subcheck != NULL)
    {
        order = subcheck->OrderList();
        while (order != NULL)
        {
            if (order->item_family == family)
            {
                if (item_name.length < 1)
                    retval += order->count;
                else
                {
                    if (strcmp(item_name.Value(), order->item_name.Value()) == 0)
                        retval += order->count;
                }
            }
            order = order->next;
        }
    }

    return retval;
}

int CouponInfo::AppliesItem(SalesItem *item)
{
    FnTrace("CouponInfo::AppliesItem()");
    int retval = 0;

    if (item != NULL)
    {
        if (flags & TF_ITEM_SPECIFIC)
        {
            if (family != item->family)
                retval = 0;
            else if (item_name.length == 0)
                retval = 0;
            else if (strcmp(item_name.Value(), item->item_name.Value()) == 0)
                retval = 1;
            else if (strcmp(item_name.Value(), ALL_ITEMS_STRING) == 0)
                retval = 1;
        }
        else
            retval = 1;
    }

    return retval;
}

/****
 * Amount:  Returns the full amount to be charged to the
 *  customer, not just the amount of the coupon.
 * NOTE:  This is only for item specific coupons.
 ****/
int CouponInfo::Amount(SubCheck *subcheck)
{
    FnTrace("CouponInfo::Amount(Subcheck *)");
    int retval = 0;
    int item_cost = 0;
    int item_count = 0;

    if (active == 0)
        retval = 0;
    else if (flags & TF_ITEM_SPECIFIC)
    {
        item_count = Applies(subcheck);
        if (item_count > 0 && subcheck->OrderList() != NULL)
        {
            item_cost = subcheck->OrderList()->item_cost;
            if (item_cost > 0)
                retval = Amount(item_cost, item_count);
        }
    }

    return retval;
}

/****
 * CPAmount:  Returns the full amount of deductions for the coupon.
 *  So if the coupon is $1 off each of four items, $4 will be
 *  returned.
 * NOTE:  This is only for item specific coupons.
 ****/
int CouponInfo::CPAmount(SubCheck *subcheck)
{
    FnTrace("CouponInfo::CPAmount(SubCheck *)");
    int retval = 0;
    int item_cost = 0;
    int item_count = 0;

    if (active == 0)
        retval = 0;
    else if (flags & TF_ITEM_SPECIFIC)
    {
        item_count = Applies(subcheck);
        if (item_count > 0 && subcheck->OrderList() != NULL)
        {
            item_cost = subcheck->OrderList()->item_cost;
            if (item_cost > 0)
            {
                retval = CPAmount(item_cost, item_count);
            }
        }
    }

    return retval;
}

/****
 * Amount:  Returns the full amount to be charged to the
 *  customer, not just the amount of the coupon.
 * NOTE:  This is only for item specific coupons.
 ****/
int CouponInfo::Amount(int item_cost, int item_count)
{
    FnTrace("CouponInfo::Amount(int, int)");
    int retval = 0;
    Flt price;
    Flt percent;

    if (active == 0)
        retval = 0;
    else if (flags & TF_SUBSTITUTE)
        retval = amount;
    else if (flags & TF_IS_PERCENT)
    {
        price = PriceToFlt(item_cost);
        percent = PercentToFlt(amount);
        price = price - (price * percent);
        retval = FltToPrice(price);
    }
    else
        retval = item_cost - amount;

    retval = retval * item_count;

    return retval;
}

/****
 * CPAmount:  Returns the full amount of deductions for the coupon.
 *  So if the coupon is $1 off each of four items, $4 will be
 *  returned.
 * NOTE:  This is only for item specific coupons.
 ****/
int CouponInfo::CPAmount(int item_cost, int item_count)
{
    FnTrace("CouponInfo::CPAmount(int, int)");
    int retval = 0;
    int total_cost = item_cost * item_count;

    if (active == 0)
        retval = 0;
    else
        retval = total_cost - Amount(item_cost, item_count);

    return retval;
}


/*********************************************************************
 * CreditCardInfo Class
 ********************************************************************/

CreditCardInfo::CreditCardInfo()
{
    active = 1;
}

CreditCardInfo *CreditCardInfo::Copy()
{
    FnTrace("CreditCardInfo::Copy()");
    CreditCardInfo *retcreditcard = new CreditCardInfo();
    retcreditcard->name.Set(name);
    retcreditcard->id = id;
    retcreditcard->local = local;
    return retcreditcard;
}

int CreditCardInfo::Read(InputDataFile &df, int version)
{
    FnTrace("CreditCardInfo::Read()");
    int error = 0;
    error += df.Read(id);
    error += df.Read(name);
    if (version >= 40)
        error += df.Read(local);
    if (version >= 43)
        error += df.Read(active);
    return error;
}

int CreditCardInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("CreditCardInfo::Write()");
    int error = 0;
    error += df.Write(id);
    error += df.Write(name);
    error += df.Write(local);
    error += df.Write(active);
    return error;
}


/*********************************************************************
 * MealInfo Class
 ********************************************************************/

MealInfo::MealInfo()
{
    amount = 0;
    flags  = 0;
    active = 1;
}

MealInfo *MealInfo::Copy()
{
    FnTrace("MealInfo::Copy()");
    MealInfo *retmeal = new MealInfo();
    retmeal->name.Set(name);
    retmeal->id = id;
    retmeal->amount = amount;
    retmeal->flags = flags;
    retmeal->local = local;
    return retmeal;
}

int MealInfo::Read(InputDataFile &df, int version)
{
    FnTrace("MealInfo::Read()");
    int error = 0;
    error += df.Read(id);
    error += df.Read(name);
    error += df.Read(flags);
    error += df.Read(amount);
    if (version >= 43)
        error += df.Read(active);
    return error;
}

int MealInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("MealInfo::Write()");
    int error = 0;
    error += df.Write(id);
    error += df.Write(name);
    error += df.Write(flags);
    error += df.Write(amount);
    error += df.Write(active);
    return error;
}


/*********************************************************************
 * TermInfo Class
 ********************************************************************/

TermInfo::TermInfo()
{
    FnTrace("TermInfo::TermInfo()");
    next          = NULL;
    fore          = NULL;
    type          = TERMINAL_NORMAL;
    sortorder     = CHECK_ORDER_NEWOLD;
    printer_model = 0;
    printer_port  = 5964;
    cdu_type      = -1;
    drawers       = 0;
    dpulse        = 0;
    stripe_reader = 0;
    kitchen       = 0;
    sound_volume  = 0;
    display_host.Set("unknown");
    term_hardware = 0;
    isserver      = 0;
    print_workorder = 1;
    workorder_heading = 0;

    cc_credit_termid.Set("");
    cc_debit_termid.Set("");
}

int TermInfo::Read(InputDataFile &df, int version)
{
    FnTrace("TermInfo::Read()");
    int error = 0;
    error += df.Read(name);
    if (version >= 32)
        error += df.Read(term_hardware);

    error += df.Read(type);
    error += df.Read(display_host);
    error += df.Read(printer_host);
    error += df.Read(printer_model);
    error += df.Read(printer_port);
    error += df.Read(drawers);
    error += df.Read(stripe_reader);
    error += df.Read(kitchen);
    if (version >= 30)
        error += df.Read(sound_volume);
    if (version >= 33)
        error += df.Read(sortorder);
    if (version >= 41)
        error += df.Read(isserver);
    if (version >= 46)
    {
        error += df.Read(cdu_type);
        error += df.Read(cdu_path);
    }
    if (version >= 55)
        error += df.Read(dpulse);
    if (version >= 57)
    {
        error += df.Read(cc_credit_termid);
        error += df.Read(cc_debit_termid);
    }
    if (version >= 92)
        error += df.Read(print_workorder);
    if (version >= 93)
        error += df.Read(workorder_heading);

    // dpulse is used when there are two drawers attached to one
    // printer, and two terminals printing to that printer.  AKA,
    // the terminals share the printer, but each has its own
    // drawer.  One terminal prints to pulse 1, the other prints
    // to pulse 2.  When two drawers are used, the number of the
    // drawer is used to determine the control code.
    if (drawers > 1)
        dpulse = 0;

    return error;
}

int TermInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("TermInfo::Write()");
    int error = 0;

    error += df.Write(name);
    error += df.Write(term_hardware);
    error += df.Write(type);
    error += df.Write(display_host);
    error += df.Write(printer_host);
    error += df.Write(printer_model);
    error += df.Write(printer_port);
    error += df.Write(drawers);
    error += df.Write(stripe_reader);
    error += df.Write(kitchen);
    error += df.Write(sound_volume);
    error += df.Write(sortorder);
    error += df.Write(isserver);
    error += df.Write(cdu_type);
    error += df.Write(cdu_path);
    error += df.Write(dpulse);
    error += df.Write(cc_credit_termid);
    error += df.Write(cc_debit_termid);
    error += df.Write(print_workorder);
    error += df.Write(workorder_heading);
    
    return error;
}

int TermInfo::OpenTerm(Control *control_db, int update)
{
    FnTrace("TermInfo::OpenTerm()");
    if (control_db == NULL)
        return 1;

    Terminal *term = NewTerminal(display_host.Value(), term_hardware, isserver);
    if (term == NULL)
        return 1;

    int flag = UPDATE_TERMINALS;
    term->is_server = IsServer();
    term->name.Set(name);
    term->type = type;
    term->original_type = type;
    term->sortorder = sortorder;
    term->cdu = NewCDUObject(cdu_path.Value(), cdu_type);
    term->cc_credit_termid.Set(cc_credit_termid);
    term->cc_debit_termid.Set(cc_debit_termid);
    term->print_workorder = print_workorder;
    term->workorder_heading = workorder_heading;
    if (printer_model != MODEL_NONE)
    {
        if (printer_host.length > 0)
            term->printer_host.Set(printer_host);
        else
            term->printer_host.Set(display_host);

        term->printer_port = printer_port;
        Printer *printer = control_db->NewPrinter(name.Value(),
                                                  term->printer_host.Value(),
                                                  printer_port,
                                                  printer_model);
        if (printer != NULL)
        {
            if (drawers == 1)
                printer->pulse = dpulse;
            printer->term_name.Set(name);
            term->drawer_count = drawers;
            flag |= UPDATE_PRINTERS;
        }
    }

    control_db->Add(term);
    if (update)
    {
        term->Initialize();
        control_db->UpdateAll(flag, NULL);
    }

    return 0;
}

Terminal *TermInfo::FindTerm(Control *control_db)
{
    FnTrace("TermInfo::FindTerm()");
    for (Terminal *term = control_db->TermList(); term != NULL; term = term->next)
    {
        if (term->host == display_host)
            return term;
    }
    return NULL;
}

Printer *TermInfo::FindPrinter(Control *control_db)
{
    FnTrace("TermInfo::FindPrinter()");
    if (printer_host.length > 0)
        return control_db->FindPrinter(printer_host.Value(), printer_port);
    else
        return control_db->FindPrinter(display_host.Value(), printer_port);
}

/****
 * IsServer:  Sets isserver and returns previous value, or just returns
 *  that previous value.
 ****/
int TermInfo::IsServer(int set)
{
    FnTrace("TermInfo::IsServer()");
    int retval = isserver;
    if (set >= 0)
    {
        isserver = set;
    }
    return retval;
}


/*********************************************************************
 * PrinterInfo Class  -- This is only used for the printers in the
 * Remote Printer view, not for the printers attached to terminals.
 ********************************************************************/

PrinterInfo::PrinterInfo()
{
    FnTrace("PrinterInfo::PrinterInfo()");
    next         = NULL;
    fore         = NULL;
    type         = 0;
    model        = 0;
    port         = 0;
    order_margin = 0;
    kitchen_mode = PRINT_LARGE | PRINT_NARROW;
}

// Member Functions
int PrinterInfo::Read(InputDataFile &df, int version)
{
    FnTrace("PrinterInfo::Read()");
    int error = 0;

    if (version >= 28)
        error += df.Read(name);
    error += df.Read(host);
    error += df.Read(port);
    error += df.Read(model);
    error += df.Read(type);
    if (version >= 50)
        error += df.Read(kitchen_mode);
    if (version >= 93)
        error += df.Read(order_margin);

    return error;
}

int PrinterInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("PrinterInfo::Write()");

    int error = 0;
    error += df.Write(name);
    error += df.Write(host);
    error += df.Write(port);
    error += df.Write(model);
    error += df.Write(type);
    error += df.Write(kitchen_mode);
    error += df.Write(order_margin);

    return error;
}

int PrinterInfo::OpenPrinter(Control *control_db, int update)
{
    FnTrace("PrinterInfo::OpenPrinter()");
    if (control_db == NULL)
        return 1;

    Printer *p = control_db->NewPrinter(host.Value(), port, model);
    if (p)
    {
        p->SetType(type);
        p->SetKitchenMode(kitchen_mode);
    p->order_margin = order_margin;
        if (update)
            control_db->UpdateAll(UPDATE_PRINTERS, NULL);
    }
    return 0;
}

Printer *PrinterInfo::FindPrinter(Control *control_db)
{
    FnTrace("PrinterInfo::FindPrinter()");
    return control_db->FindPrinter(host.Value(), port);
}

const char* PrinterInfo::Name()
{
    FnTrace("PrinterInfo::Name()");
    if (name.length > 0)
        return name.Value();
    else
        return FindStringByValue(type, PrinterTypeValue,
                                 PrinterTypeName, UnknownStr);
}

/****
 * DebugPrint:  This is for debugging only.  I now have a memory
 *   corruption issue that seems to be overwriting printer info,
 *   and I want to be able to print all of the printer info
 *   at several steps so I can maybe determine when it's getting
 *   corrupted.
 ****/
void PrinterInfo::DebugPrint(int printall)
{
    FnTrace("PrinterInfo::DebugPrint()");
    printf("Printer:\n");
    printf("    Name:   %s\n", name.Value());
    printf("    Host:   %s\n", host.Value());
    printf("    Port:   %d\n", port);
    printf("    Model:  %d\n", model);
    printf("    Type:   %d\n", type);
    printf("    Kitchen Mode:  %d\n", kitchen_mode);

    if (printall && next != NULL)
        next->DebugPrint(printall);
}


/*********************************************************************
 * Settings Class
 ********************************************************************/

Settings::Settings()
{
    allow_iconify      = 1;
    email_send_server.Set("");
    expire_days        = 0;
    license_key.Set("");
    changed            = 0;
    screen_blank_time  = 60;
    start_page_timeout = 60;
    delay_time1        = 15;
    delay_time2        = 5;
    use_seats          = 0;
    password_mode      = PW_NONE;
    min_pw_len         = 3;
    sale_credit        = 0;
    drawer_mode        = DRAWER_NORMAL;
    day_start          = 0;
    day_end            = 0;
    receipt_print      = RECEIPT_BOTH;
    drawer_print       = DRAWER_PRINT_NEVER;
    receipt_all_modifiers = 0;
    receipt_header_length = 0;  // extra space only
    order_num_style    = 0;  // normal size
    table_num_style    = 0;  // ibid
    shifts_used        = 4;
    split_kitchen      = 0;
    region             = 0;
    store              = 0;
    developer_key      = 123456789; // silly default
    price_rounding     = ROUNDING_NONE;
    double_mult        = 2;
    double_add         = 0;
    combine_accounts   = 1;
    discount_alcohol   = 1;
    change_for_checks  = 1;
    change_for_credit  = 1;
    change_for_gift    = 0;
    change_for_roomcharge = 0;
    header_flags       = 0;
    footer_flags       = 0;
    media_balanced =
        (1<<TENDER_CASH_AVAIL)|(1<<TENDER_CHECK)|(1<<TENDER_CHARGE_CARD)|
        (1<<TENDER_GIFT)|(1<<TENDER_COUPON)|(1<<TENDER_EXPENSE);
    overtime_shift     = 0;
    overtime_week      = 0;
    wage_week_start    = 0;
    authorize_method   = CCAUTH_NONE;
    card_types         = CARD_TYPE_NONE;
    auto_authorize = 0;
    use_entire_cc_num  = 0;
    save_entire_cc_num = 0;
    show_entire_cc_num = 0;
    allow_cc_preauth   = 1;
    allow_cc_cancels   = 0;
    merchant_receipt   = 1;
#ifdef CREDITCHEQ
    finalauth_receipt  = 1;
    void_receipt       = 1;
#else
    finalauth_receipt  = 0;
    void_receipt       = 0;
#endif
    cash_receipt       = 0;
    cc_bar_mode        = 0;
    cc_merchant_id.Set("");
    cc_server.Set("");
    cc_port.Set("");
    cc_user.Set("");
    cc_password.Set("");
    cc_connect_timeout = 30;
    cc_preauth_add     = 0;
    cc_noconn_message1.Set("");
    cc_noconn_message2.Set("");
    cc_noconn_message3.Set("");
    cc_noconn_message4.Set("");
    cc_voice_message1.Set("");
    cc_voice_message2.Set("");
    cc_voice_message3.Set("");
    cc_voice_message4.Set("");
    always_open = 0;
    use_item_target = 0;
    min_day_length = 7200;  // the original default
    fast_takeouts = 0;

    int i;
    for (i = 0; i < MAX_SHIFTS; ++i)
        shift_start[i] = -1;

    for (i = 0; i < MAX_MEALS; ++i)
    {
        meal_start[i]  = -1;
        meal_active[i] = 0;
    }

    for (i = 0; i < MAX_FAMILIES; ++i)
    {
        family_printer[i] = PRINTER_NONE;
        family_group[i]   = SALESGROUP_FOOD;
        video_target[i]   = PRINTER_DEFAULT;
    }

    for (i = 0; i < MAX_JOBS; ++i)
    {
        job_active[i] = 0;
        job_flags[i]  = 0;
        job_level[i]  = 0;
    }

    // Locale/Region Settings
    time_format     = TIME_12HOUR;
    date_format     = DATE_MMDDYY;
    number_format   = NUMBER_STANDARD;
    measure_system  = MEASURE_STANDARD;
    money_symbol.Set("$");

    // Currency/Tax Settings  
    tax_food                = 0.0;
    tax_alcohol             = 0.0;
    tax_room                = 0.0;
    tax_merchandise         = 0.0;
    tax_GST                 = 0.0; // Explicit Fix for Canadian Implementation
    tax_PST                 = 0.0; // Explicit Fix for Canadian Implementation
    tax_HST                 = 0.0; // Explicit Fix for Canadian Implementation
    tax_QST                 = 0.0; // Explicit Fix for Canadian Implementation
    tax_VAT                 = 0.0;
    food_inclusive      = 0;
    room_inclusive      = 0;
    alcohol_inclusive       = 0;
    merchandise_inclusive   = 0;
    
    // FastFood Settings
    tax_takeout_food        = 1;   // tax takeout food by default
    personalize_fast_food   = 0;   // don't force jump to customer data for fast food by default

    royalty_rate            = 0.0;
    advertise_fund          = 0.0;
    // added in Jan 2006 project for donato
    debit_cost              = 0.0;
    credit_rate             = 0.0;
    credit_cost             = 0.0;
    line_item_cost          = 0.0;
    daily_cert_fee          = 0.0;


    last_tax_id     = 0;
    last_money_id   = 0;

    // Report settings
    sales_period          = SP_MONTH;
    labor_period          = SP_MONTH;
    show_modifiers        = 0;
    default_report_period = SP_DAY;
    print_report_header   = 1;
    print_timeout         = 15;
    report_start_midnight = 1;
    kv_print_method       = KV_PRINT_UNMATCHED;
    kv_show_user          = 0;

    // Media
    last_discount_id   = 0;
    last_coupon_id     = 0;
    last_creditcard_id = 0;
    last_comp_id       = 0;
    last_meal_id       = 0;
    balance_auto_coupons = 0;

    // credit/debit card
    visanet_currency = 840;   // U.S. dollar
    visanet_country  = 840;   // U.S.
    visanet_city     = 97401; // Postal zip code
    visanet_language = 0;     // U.S. English
    visanet_timezone = 708;   // PST
    visanet_category = 5999;  // Merchant category

    low_acct_num = 1000;      // American defaults (or Gene's defaults)
    high_acct_num = 9999;
    allow_user_select = 0;
    drawer_day_float = 0;
    drawer_night_float = 0;
    default_tab_amount = 0;
    country_code = 0;
    store_code = 0;
    drawer_account = 0;
    split_check_view = SPLIT_CHECK_ITEM;
    mod_separator = MOD_SEPARATE_NL;
    expire_message1.Set("Please contact Support.");
    expire_message2.Set("at");
    expire_message3.Set("541-515-5913");
    expire_message4.Set("");
    allow_multi_coupons = 0;
    cc_print_custinfo       = 0;

    for (i = 0; i < MAX_CDU_LINES; i++)
    {
        cdu_header[i].Set("");
    }
}

int Settings::Load(const char* file)
{
    FnTrace("Settings::Load()");
    if (file)
        filename.Set(file);

    int val, version = 0;
    InputDataFile df;
    if (df.Open(filename.Value(), version))
        return 1;

    // VERSION NOTES
    // 25 (5/8/97)   earliest supported version
    // 26 (6/4/97)   added locale specification & overtime definition
    // 27 (7/2/97)   added authorize_method & always_open; terms/printers redone
    // 28 (10/8/97)  added change_for_roomcharge flag; added use_item_target
    //               locale settings added; money & tax definitions added
    // 29 (2/27/98)  credit/debit authorization info added
    //               order entry window size/position added
    // 30 (8/13/98)  sound/volume settings added to TermInfo; update info added
    //               added room & merchandise tax values
    // 31 (9/10/98)  added delay_time1, delay_time2 fields
    // 32 (10/7/98)  added term_hardware field to TermInfo
    // 33 (4/15/02)  added sort order to TermInfo
    // 34 (4/16/02)  added video target information
    // 35 (5/14/02)  added low and high account number limits
    // 36 (5/14/02)  added allow_user_select, day and night floats
    // 37 (5/15/02)  added country and store codes, drawer account
    // 38            added minimum day length
    // 39 (5/18/02)  added drawer float times (start of day, start of night)
    // 40 (7/15/02)  added local/global setting for discounts, coupons, etc.
    // 41 (7/22/02)  added isserver setting to TermInfo
    // 42 (8/29/02)  added license_key
    // 43 (9/3/02)   added active/inactive setting for disconts, coupons, etc.
    // 44 (9/3/02)   added MealInfo to media.dat
    // 45            added royalty_rate
    // 46 (10/14/02) added cdu_type and cdu_path settings to TermInfo
    // 47 (10/14/02) added headers for cdu when not displaying money
    // 48 (12/16/02) added smtp outgoing server (email_send_server)
    // 49            added Reply To address for outgoing emails
    // 50 (1/31/03)  added Kitchen Mode, specifically for Epson printers
    // 51 (2/14/03)  added fast_takeouts
    // 52 (2/14/03)  added tax_VAT
    // 53 (3/5/03)   added money_symbol
    // 54            added require_drawer_balance
    // 55            added dpulse, selects which drawer to open when 2+ are present
    // 56            added default_report_period
    // 57 (5/14/03)  added credit/charge card settings
    // 58            added store_address2
    // 59 (8/28/03)  added print_report_header
    // 60 (9/8/03)   added card_types
    // 61 (10/15/03) added allow_cc_preauth
    // 62 (10/16/03) added allow_cc_cancels and merchant_receipt
    // 63 (10/21/03) added cash_receipt
    // 64 (10/23/03) added split_check_view for Gene
    // 65 (11/21/03) added cc_connect_timeout
    // 66 (11/21/03) added cc messages
    // 67 (11/21/03) added cc_merchant_id
    // 68 (12/17/03) added cc_preauth_add
    // 69 (12/23/03) added mod_separator
    // 70 (12/29/03) added print_timeout
    // 71 (01/12/04) added expire_message settings
    // 72 (01/21/04) added finalauth_receipt, void_receipt, cc_bar_mode
    // 73 (02/06/04) added report_start_midnight
    // 75-78         item specific and time sensitive coupon additions
    // 79 (02/18/04) added allow_multi_coupons
    // 82 (04/05/04) added allow_iconify
    // 83 (09/23/04) added receipt_all_modifiers
    // 84 (11/12/04) added receipt_header_length, order_num_style, table_num_style
    // 85 (11/23/04) added drawer_print
    // 86 (01/27/05) added kv_print_method
    // 87 (05/17/05) added default_tab_amount
    // 88 (05/19/05) added balance_auto_coupons
    // 89 (05/25/05) added advertise_fund
    // 90 (06/29/05) added cc_print_custinfo
    // 91 (11/02/05) added kv_show_user
    // 92 (06/11/15) added print_workorder

    genericChar str[256];
    if (version < 25 || version > SETTINGS_VERSION)
    {
        sprintf(str, "Unknown Settings file version %d", version);
        ReportError(str);
        return 1;
    }

    int tmp, error = 0;
    df.Read(store_name);
    df.Read(store_address);
    if (version >= 58)
        df.Read(store_address2);
    df.Read(region);
    df.Read(store);
    df.Read(val); tax_food    = PercentToFlt(val);
    df.Read(val); tax_alcohol = PercentToFlt(val);

    if (version >= 30)
    {
        df.Read(val); tax_room        = PercentToFlt(val);
        df.Read(val); tax_merchandise = PercentToFlt(val);
    }

    df.Read(val); tax_GST             = PercentToFlt(val);
    df.Read(val); tax_PST             = PercentToFlt(val);
    df.Read(val); tax_HST             = PercentToFlt(val);
    df.Read(val); tax_QST             = PercentToFlt(val);
    if (version >= 45)
    {
        df.Read(val);
        royalty_rate    = PercentToFlt(val);
    }
    if (version >= 52)
    {
        df.Read(val);
        tax_VAT         = PercentToFlt(val);
    }

    df.Read(screen_blank_time);
    if (version >= 73)
        df.Read(start_page_timeout);

    if (version >= 31)
    {
        df.Read(delay_time1);
        df.Read(delay_time2);
    }
    df.Read(use_seats);
    df.Read(password_mode);
    df.Read(sale_credit);
    df.Read(drawer_mode);
    df.Read(receipt_print);
    df.Read(shifts_used);
    df.Read(split_kitchen);
    df.Read(developer_key);
    df.Read(price_rounding);
    if (version <= 26)
    {
        // report_model
        df.Read(tmp);
        if (tmp != MODEL_NONE)
        {
            PrinterInfo *pi = new PrinterInfo;
            pi->type = PRINTER_REPORT;

#ifdef LINUX
            pi->host.Set("lp0");
#endif
#ifdef BSD
            pi->host.Set("lpt0");
            //pi->host.Set("ttyd1");
#endif

            pi->port = 0;
            pi->model = tmp;
            Add(pi);
        }
    }
    df.Read(double_mult);
    df.Read(double_add);
    df.Read(combine_accounts);
    df.Read(change_for_checks);
    df.Read(change_for_credit);
    df.Read(change_for_gift);
    if (version >= 28)
        df.Read(change_for_roomcharge);
    df.Read(sales_period);
    df.Read(sales_start);
    df.Read(labor_period);
    df.Read(labor_start);
    df.Read(discount_alcohol);
    if (version >= 26)
    {
        df.Read(language);
        df.Read(overtime_shift);
        df.Read(overtime_week);
        df.Read(wage_week_start);
        if (wage_week_start >= 10080) // minutes in week
            wage_week_start = 0;
        if (wage_week_start < 0)
            wage_week_start = 0;
        if (version <= 26)
            df.Read(tmp);  // obsolete entry
    }
    if (version >= 27)
    {
        df.Read(authorize_method);
        // Do not allow the authorize method to get out of range.
        // Also, for now we only allow one authorization method to be
        // defined for the current binaries, so if it isn't that,
        // it has to be NONE.  This prevents error that might otherwise
        // be caused by copying CREDITCHEQ data files into MAINSTREET
        // binaries.
        if (authorize_method < 0 || authorize_method > CCAUTH_MAX)
            authorize_method = CCAUTH_NONE;
        else if (authorize_method != ccauth_defined)
            authorize_method = CCAUTH_NONE;
        df.Read(always_open);
    }
    if (version >= 28)
    {
        df.Read(use_item_target);
        df.Read(time_format);
        df.Read(date_format);
        df.Read(number_format);
        df.Read(measure_system);
    }

    int n = 0;
    int x = 0;
    int i = 0;
    df.Read(n);
    for (i = 0; i < n; ++i)
    {
        df.Read(x);
        job_active[x] = 1;
        df.Read(job_flags[x]);
        df.Read(job_level[x]);
    }

    if (version >= 47)
    {
        for (i = 0; i < MAX_CDU_LINES; i++)
        {
            df.Read(cdu_header[i]);
        }
    }

    for (i = 0; i < MAX_HEADER_LINES; ++i)
    {
        df.Read(receipt_header[i]);
    }

    for (i = 0; i < MAX_FOOTER_LINES; ++i)
    {
        df.Read(receipt_footer[i]);
    }

    df.Read(header_flags);
    df.Read(footer_flags);

    for (i = 0; i < MAX_SHIFTS; ++i)
    {
        df.Read(shift_start[i]);
        shift_start[i] %= 1440;
    }

    for (i = 0; i < MAX_MEALS; ++i)
    {
        df.Read(meal_active[i]);
        df.Read(meal_start[i]);
    }

    for (i = 0; i < MAX_FAMILIES; ++i)
    {
        df.Read(family_group[i]);
        df.Read(family_printer[i]);
        if ( version >= 34 )
        {
            df.Read(video_target[i]);
        }
    }

    if (version <= 26)
    {
        Str phost;
        int pport;
        int pmodel;
        for (i = 0; i < 16; ++i)
        {
            df.Read(phost);
            df.Read(pport);
            df.Read(pmodel);
            if (phost.length > 0)
            {
                PrinterInfo *pi = new PrinterInfo;
                pi->host.Set(phost);
                pi->port = pport;
                pi->model = pmodel;
                pi->type = i + 1;
                Add(pi);
            }
        }

        Str thost;
        int thardware;
        int ttype;
        int tkitchen;
        for (i = 0; i < 16; ++i)
        {
            df.Read(thost);
            df.Read(thardware);
            df.Read(ttype);
            df.Read(tkitchen);

            if (thost.length > 0)
            {
                TermInfo *ti = new TermInfo;
                sprintf(str, "Term %d", i + 1);
                ti->name.Set(str);
                ti->type = ttype;
                ti->display_host.Set(thost);
                ti->printer_host.Clear();
                ti->printer_model = MODEL_EPSON;
                switch (thardware)
                {
                case 0: ti->printer_model = MODEL_NONE; break;
                case 1: break;
                case 2: ti->drawers = 1; break;
                case 3: ti->drawers = 2; break;
                }
                ti->kitchen = tkitchen;
                Add(ti);
            }
        }
    }

    int count = 0;
    if (version >= 27)
    {
        df.Read(count);
        for (i = 0; i < count; ++i)
        {
            if (df.end_of_file)
            {
                ReportError("Unexpected end of terminals in settings");
                return 1;
            }
            TermInfo *ti = new TermInfo;
            ti->Read(df, version);
            Add(ti);
        }

        df.Read(count);
        for (i = 0; i < count; ++i)
        {
            if (df.end_of_file)
            {
                ReportError("Unexpected end of printers in settings");
                return 1;
            }
            PrinterInfo *pi = new PrinterInfo;
            pi->Read(df, version);
            Add(pi);
        }
    }

    df.Read(last_discount_id);
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of discounts in settings");
            return 1;
        }
        DiscountInfo *ds = new DiscountInfo;
        ds->Read(df, version);
        if (ds->name.length > 0)
        {
            if (MediaIsDupe(discount_list.Head(), ds->id))
                ds->id = MediaFirstID(discount_list.Head(), 1);
            Add(ds);
        }
    }

    df.Read(last_coupon_id);
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of coupons in settings");
            return 1;
        }
        CouponInfo *cp = new CouponInfo;
        cp->Read(df, version);
        if (cp->name.length > 0)
        {
            if (MediaIsDupe(coupon_list.Head(), cp->id))
                cp->id = MediaFirstID(coupon_list.Head(), 1);
            Add(cp);
        }
    }

    df.Read(last_creditcard_id);
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of credit cards in settings");
            return 1;
        }
        CreditCardInfo *cc = new CreditCardInfo;
        cc->Read(df, version);
        if (cc->name.length > 0)
        {
            if (MediaIsDupe(creditcard_list.Head(), cc->id))
                cc->id = MediaFirstID(creditcard_list.Head(), 1);
            Add(cc);
        }
    }

    df.Read(last_comp_id);
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of comps in settings");
            return 1;
        }
        CompInfo *cm = new CompInfo;
        cm->Read(df, version);
        if (cm->name.length > 0)
        {
            if (MediaIsDupe(comp_list.Head(), cm->id))
                cm->id = MediaFirstID(comp_list.Head(), 1);
            Add(cm);
        }
    }

    df.Read(last_meal_id);
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of employee meals in settings");
            return 1;
        }
        MealInfo *mi = new MealInfo;
        mi->Read(df, version);
        if (mi->name.length > 0)
        {
            if (MediaIsDupe(meal_list.Head(), mi->id))
                mi->id = MediaFirstID(meal_list.Head(), 1);
            Add(mi);
        }
    }

    if (version >= 28)
    {
        df.Read(last_tax_id);
        df.Read(count);
        for (i = 0; i < count; ++i)
        {
            if (df.end_of_file)
            {
                ReportError("Unexpected end of tax definitions in settings");
                return 1;
            }
            TaxInfo *tx = new TaxInfo;
            tx->Read(df, version);
            Add(tx);
        }

        df.Read(last_money_id);
        df.Read(count);
        for (i = 0; i < count; ++i)
        {
            if (df.end_of_file)
            {
                ReportError("Unexpected end of money definitions in settings");
                return 1;
            }
            MoneyInfo *my = new MoneyInfo;
            my->Read(df, version);
            Add(my);
        }
    }
    if (version >= 29)
    {
        for (i = 0; i < 4; ++i)
        {
            df.Read(oewindow[i].x);
            df.Read(oewindow[i].y);
            df.Read(oewindow[i].w);
            df.Read(oewindow[i].h);
        }
        df.Read(visanet_acquirer_bin);
        df.Read(visanet_merchant);
        df.Read(visanet_store);
        df.Read(visanet_terminal);
        df.Read(visanet_currency);
        df.Read(visanet_country);
        df.Read(visanet_city);
        df.Read(visanet_language);
        df.Read(visanet_timezone);
        df.Read(visanet_category);
    }
    if (version >= 30)
    {
        df.Read(update_address);
        df.Read(update_user);
        df.Read(update_password);
    }

    if (version >= 35)
    {
        df.Read(low_acct_num);
        df.Read(high_acct_num);
    }

    if (version >= 36)
    {
        df.Read(allow_user_select);
        df.Read(drawer_day_float);
        df.Read(drawer_night_float);
    }
    if (version >= 37)
    {
        df.Read(country_code);
        df.Read(store_code);
        df.Read(drawer_account);
    }
    if (version >= 38)
        df.Read(min_day_length);
    if (version >= 39)
    {
        df.Read(drawer_day_start);
        df.Read(drawer_night_start);
    }
    if (version >= 42)
        df.Read(license_key);
    if (version >= 48)
        df.Read(email_send_server);
    if (version >= 49)
        df.Read(email_replyto);
    if (version >= 51)
        df.Read(fast_takeouts);
    if (version >= 53)
        df.Read(money_symbol);
    if (version >= 54)
        df.Read(require_drawer_balance);
    if (version >= 56)
        df.Read(default_report_period);
    if (version >= 57)
    {
        df.Read(auto_authorize);
        df.Read(use_entire_cc_num);
        df.Read(save_entire_cc_num);
        df.Read(show_entire_cc_num);
        if (version >= 61)
            df.Read(allow_cc_preauth);
        if (version >= 62)
        {
            df.Read(allow_cc_cancels);
            df.Read(merchant_receipt);
        }
        if (version >= 63)
            df.Read(cash_receipt);
        if (version >= 67)
            df.Read(cc_merchant_id);
        df.Read(cc_server);
        df.Read(cc_port);
        df.Read(cc_user);
        df.Read(cc_password);
        if (version >= 65)
            df.Read(cc_connect_timeout);
        if (version >= 68)
            df.Read(cc_preauth_add);
        if (version >= 66)
        {
            df.Read(cc_noconn_message1);
            df.Read(cc_noconn_message2);
            df.Read(cc_noconn_message3);
            df.Read(cc_noconn_message4);
            df.Read(cc_voice_message1);
            df.Read(cc_voice_message2);
            df.Read(cc_voice_message3);
            df.Read(cc_voice_message4);
        }
    }

    if (version >= 59)
        df.Read(print_report_header);
    if (version >= 70)
        df.Read(print_timeout);
    if (version >= 60)
        df.Read(card_types); // see below for mods to this value
    if (version >= 64)
        df.Read(split_check_view);
    if (version >= 69)
        df.Read(mod_separator);
    if (version >= 70)
    {
        df.Read(expire_message1);
        df.Read(expire_message2);
        df.Read(expire_message3);
        df.Read(expire_message4);
    }
    if (version >= 72)
    {
        df.Read(finalauth_receipt);
        df.Read(void_receipt);
        df.Read(cc_bar_mode);
        // Gene wanted the Revenue and Final Balance reports to start at the same
        // time as shift start, sales period, and labor period.  Some people may
        // also want the reports to start at midnight as they used to.  Especially
        // Doug, who has 48 stores installed already.  So, all new stores will
        // use the new method.  But for older stores, default it to off.
        // The new stores are all those with settings versions 72 or greater.
        // Doug's stores all have settings versions older than that because
        // I never sent him version 72 binaries.  BAK Feb 6, 2004
        report_start_midnight = 0;
    }
    if (version >= 73)
        df.Read(report_start_midnight);
    if (version >= 79)
        df.Read(allow_multi_coupons);
    if (version >= 82)
        df.Read(allow_iconify);
    if (version >= 83)
        df.Read(receipt_all_modifiers);
    if (version >= 84)
    {
        df.Read(receipt_header_length);
        df.Read(order_num_style);
        df.Read(table_num_style);
    }
    if (version >= 85)
        df.Read(drawer_print);
    if (version >= 86)
        df.Read(kv_print_method);
    if (version >= 87)
        df.Read(default_tab_amount);
    if (version >= 88)
        df.Read(balance_auto_coupons);
    if (version >= 89)
        df.Read(advertise_fund);
    if (version >= 90)
        df.Read(cc_print_custinfo);
    if (version >= 91)
        df.Read(kv_show_user);

    if (store == STORE_SUNWEST)
        media_balanced |=
            (1<<TENDER_COMP)|(1<<TENDER_DISCOUNT)|(1<<TENDER_EMPLOYEE_MEAL)|
            (1<<TENDER_PAID_TIP);

    if (authorize_method == CCAUTH_MAINSTREET)
        card_types &= ~CARD_TYPE_DEBIT;
    else if (authorize_method == CCAUTH_CREDITCHEQ)
        cc_print_custinfo = 0;
    card_types &= ~CARD_TYPE_GIFT;

    // read from config files.  because this is the way of the future,
    // settings in config files always override settings from .dat files.
    // however, once the settings have been saved, they should be the same
    // in both places.

    {
        using namespace confmap;

        ConfFile conf(CONFIG_TAX_FILE, true);
        Flt conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_FOOD_INCLUSIVE], sects[S_MISC]))
        food_inclusive = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_ROOM_INCLUSIVE], sects[S_MISC]))
        room_inclusive = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_ALCOHOL_INCLUSIVE], sects[S_MISC]))
        alcohol_inclusive = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_MERCHANDISE_INCLUSIVE], sects[S_MISC]))
        merchandise_inclusive = conf_tmp;

        if (conf.GetValue(conf_tmp, vars[V_GST], sects[S_SALES_TAX_CANADA]))
            tax_GST = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_PST], sects[S_SALES_TAX_CANADA]))
            tax_PST = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_HST], sects[S_SALES_TAX_CANADA]))
            tax_HST = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_QST], sects[S_SALES_TAX_CANADA]))
            tax_QST = conf_tmp;

        conf.SetFilename(CONFIG_FEES_FILE);
        conf.Load();
        if (conf.GetValue(conf_tmp, vars[V_ROYALTY_RATE], sects[S_MISC]))
            royalty_rate = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_ADVERTISE_FUND], sects[S_MISC]))
            advertise_fund = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_DAILY_CERT_FEE], sects[S_MISC]))
            daily_cert_fee = conf_tmp;

        if (conf.GetValue(conf_tmp, vars[V_DEBIT_COST], sects[S_ELEC_TRANS]))
            debit_cost = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_CREDIT_RATE], sects[S_ELEC_TRANS]))
            credit_rate = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_CREDIT_COST], sects[S_ELEC_TRANS]))
            credit_cost = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_LINE_ITEM_COST], sects[S_ELEC_TRANS]))
            line_item_cost = conf_tmp;

        conf.SetFilename(CONFIG_FASTFOOD_FILE);
        conf.Load();
        if (conf.GetValue(conf_tmp, vars[V_PERSONALIZE_FAST_FOOD], sects[S_MISC]))
            personalize_fast_food = conf_tmp;
        if (conf.GetValue(conf_tmp, vars[V_TAX_TAKEOUT_FOOD], sects[S_MISC]))
            tax_takeout_food = conf_tmp;
        else
            tax_takeout_food = 1;

    }

    return error;
}

int Settings::Save()
{
    FnTrace("Settings::Save()");
    if (filename.length <= 0)
        return 1;
    int error = 0;
    int count;
    int n = 0;
    int i;

    BackupFile(filename.Value());

    // Write out SETTINGS_VERSION
    OutputDataFile df;
    if (df.Open(filename.Value(), SETTINGS_VERSION))
        return 1;

    df.Write(store_name);
    df.Write(store_address);
    df.Write(store_address2);
    df.Write(region);
    df.Write(store);

    df.Write(FltToPercent(tax_food));
    df.Write(FltToPercent(tax_alcohol));
    df.Write(FltToPercent(tax_room));
    df.Write(FltToPercent(tax_merchandise));
    df.Write(FltToPercent(tax_GST)); // Explicit Fix for canadian implementation
    df.Write(FltToPercent(tax_PST)); // Explicit Fix for canadian implementation
    df.Write(FltToPercent(tax_HST)); // Explicit Fix for canadian implementation
    df.Write(FltToPercent(tax_QST)); // Explicit Fix for canadian implementation
    df.Write(FltToPercent(royalty_rate));
    df.Write(FltToPercent(tax_VAT));

    df.Write(screen_blank_time);
    df.Write(start_page_timeout);
    df.Write(delay_time1);
    df.Write(delay_time2);
    df.Write(use_seats);
    df.Write(password_mode);
    df.Write(sale_credit);
    df.Write(drawer_mode);
    df.Write(receipt_print);
    df.Write(shifts_used);
    df.Write(split_kitchen);
    df.Write(developer_key);
    df.Write(price_rounding);
    df.Write(double_mult);
    df.Write(double_add);
    df.Write(combine_accounts);
    df.Write(change_for_checks);
    df.Write(change_for_credit);
    df.Write(change_for_gift);
    df.Write(change_for_roomcharge);
    df.Write(sales_period);
    df.Write(sales_start);
    df.Write(labor_period);
    df.Write(labor_start);
    df.Write(discount_alcohol);
    df.Write(language);
    df.Write(overtime_shift);
    df.Write(overtime_week);
    df.Write(wage_week_start);
    df.Write(authorize_method);
    df.Write(always_open);
    df.Write(use_item_target);
    df.Write(time_format);
    df.Write(date_format);
    df.Write(number_format);
    df.Write(measure_system);

    for (i = 0; i < MAX_JOBS; ++i)
        if (job_active[i])
            ++n;
    df.Write(n);
    for (i = 0; i < MAX_JOBS; ++i)
        if (job_active[i])
        {
            df.Write(i);
            df.Write(job_flags[i]);
            df.Write(job_level[i]);
        }

    for (i = 0; i < MAX_CDU_LINES; i++)
    {
        df.Write(cdu_header[i]);
    }

    for (i = 0; i < MAX_HEADER_LINES; ++i)
    {
        df.Write(receipt_header[i]);
    }
    for (i = 0; i < MAX_FOOTER_LINES; ++i)
    {
        df.Write(receipt_footer[i]);
    }
    df.Write(header_flags);
    df.Write(footer_flags);

    for (i = 0; i < MAX_SHIFTS; ++i)
        df.Write(shift_start[i]);
    for (i = 0; i < MAX_MEALS; ++i)
    {
        df.Write(meal_active[i]);
        df.Write(meal_start[i]);
    }
    for (i = 0; i < MAX_FAMILIES; ++i)
    {
        df.Write(family_group[i]);
        df.Write(family_printer[i]);
        df.Write(video_target[i]);
    }

    df.Write(term_list.Count());
    for (TermInfo *ti = term_list.Head(); ti != NULL; ti = ti->next)
        ti->Write(df, SETTINGS_VERSION);

    df.Write(printer_list.Count());
    for (PrinterInfo *pi = printer_list.Head(); pi != NULL; pi = pi->next)
        pi->Write(df, SETTINGS_VERSION);

    df.Write(last_discount_id);
    count = DiscountCount(LOCAL_MEDIA);
    df.Write(count);
    DiscountInfo *discount = discount_list.Head();
    while (discount != NULL)
    {
        if (discount->IsLocal())
            discount->Write(df, SETTINGS_VERSION);
        discount = discount->next;
    }

    df.Write(last_coupon_id);
    count = CouponCount(LOCAL_MEDIA);
    df.Write(count);
    CouponInfo *coupon = coupon_list.Head();
    while (coupon != NULL)
    {
        if (coupon->IsLocal())
            coupon->Write(df, SETTINGS_VERSION);
        coupon = coupon->next;
    }

    df.Write(last_creditcard_id);
    count = CreditCardCount(LOCAL_MEDIA);
    df.Write(count);
    CreditCardInfo *creditcard = creditcard_list.Head();
    while (creditcard != NULL)
    {
        if (creditcard->IsLocal())
            creditcard->Write(df, SETTINGS_VERSION);
        creditcard = creditcard->next;
    }

    df.Write(last_comp_id);
    count = CompCount(LOCAL_MEDIA);
    df.Write(count);
    CompInfo *comp = comp_list.Head();
    while (comp != NULL)
    {
        if (comp->IsLocal())
            comp->Write(df, SETTINGS_VERSION);
        comp = comp->next;
    }

    df.Write(last_meal_id);
    count = meal_list.Count();
    df.Write(count);
    for (MealInfo *mi = meal_list.Head(); mi != NULL; mi = mi->next)
        mi->Write(df, SETTINGS_VERSION);

    df.Write(last_tax_id);
    df.Write(tax_list.Count());
    for (TaxInfo *tx = tax_list.Head(); tx != NULL; tx = tx->next)
        tx->Write(df, SETTINGS_VERSION);

    df.Write(last_money_id);
    df.Write(money_list.Count());
    for (MoneyInfo *my = money_list.Head(); my != NULL; my = my->next)
        my->Write(df, SETTINGS_VERSION);

    for (i = 0; i < 4; ++i)
    {
        df.Write(oewindow[i].x);
        df.Write(oewindow[i].y);
        df.Write(oewindow[i].w);
        df.Write(oewindow[i].h);
    }
    df.Write(visanet_acquirer_bin);
    df.Write(visanet_merchant);
    df.Write(visanet_store);
    df.Write(visanet_terminal);
    df.Write(visanet_currency);
    df.Write(visanet_country);
    df.Write(visanet_city);
    df.Write(visanet_language);
    df.Write(visanet_timezone);
    df.Write(visanet_category);
    df.Write(update_address);
    df.Write(update_user);
    df.Write(update_password);

    df.Write(low_acct_num);
    df.Write(high_acct_num);

    df.Write(allow_user_select);
    df.Write(drawer_day_float);
    df.Write(drawer_night_float);

    df.Write(country_code);
    df.Write(store_code);
    df.Write(drawer_account);
    df.Write(min_day_length);
    df.Write(drawer_day_start);
    df.Write(drawer_night_start);
    df.Write(license_key);
    df.Write(email_send_server);
    df.Write(email_replyto);
    df.Write(fast_takeouts);
    df.Write(money_symbol);
    df.Write(require_drawer_balance);
    df.Write(default_report_period);
    df.Write(auto_authorize);
    df.Write(use_entire_cc_num);
    df.Write(save_entire_cc_num);
    df.Write(show_entire_cc_num);
    df.Write(allow_cc_preauth);
    df.Write(allow_cc_cancels);
    df.Write(merchant_receipt);
    df.Write(cash_receipt);
    df.Write(cc_merchant_id);
    df.Write(cc_server);
    df.Write(cc_port);
    df.Write(cc_user);
    df.Write(cc_password);
    df.Write(cc_connect_timeout);
    df.Write(cc_preauth_add);
    df.Write(cc_noconn_message1);
    df.Write(cc_noconn_message2);
    df.Write(cc_noconn_message3);
    df.Write(cc_noconn_message4);
    df.Write(cc_voice_message1);
    df.Write(cc_voice_message2);
    df.Write(cc_voice_message3);
    df.Write(cc_voice_message4);
    df.Write(print_report_header);
    df.Write(print_timeout);
    df.Write(card_types);
    df.Write(split_check_view);
    df.Write(mod_separator);
    df.Write(expire_message1);
    df.Write(expire_message2);
    df.Write(expire_message3);
    df.Write(expire_message4);
    df.Write(finalauth_receipt);
    df.Write(void_receipt);
    df.Write(cc_bar_mode);
    df.Write(report_start_midnight);
    df.Write(allow_multi_coupons);
    df.Write(allow_iconify);
    df.Write(receipt_all_modifiers);
    df.Write(receipt_header_length);
    df.Write(order_num_style);
    df.Write(table_num_style);
    df.Write(drawer_print);
    df.Write(kv_print_method);
    df.Write(default_tab_amount);
    df.Write(balance_auto_coupons);
    df.Write(advertise_fund);
    df.Write(cc_print_custinfo);
    df.Write(kv_show_user);

    df.Close();

    changed = 0;
    SaveMedia();

    // save settings to config files.  eventually all settings should be
    // written to config files instead of .dat files.
    {
        using namespace confmap;

        ConfFile conf(CONFIG_TAX_FILE);
        error += conf.SetValue(food_inclusive, vars[V_FOOD_INCLUSIVE], sects[S_MISC]);
        error += conf.SetValue(room_inclusive, vars[V_ROOM_INCLUSIVE], sects[S_MISC]);
        error += conf.SetValue(alcohol_inclusive, vars[V_ALCOHOL_INCLUSIVE], sects[S_MISC]);
        error += conf.SetValue(merchandise_inclusive, vars[V_MERCHANDISE_INCLUSIVE], sects[S_MISC]);
        error += conf.SetValue(tax_GST, vars[V_GST], sects[S_SALES_TAX_CANADA]);
        error += conf.SetValue(tax_PST, vars[V_PST], sects[S_SALES_TAX_CANADA]);
        error += conf.SetValue(tax_HST, vars[V_HST], sects[S_SALES_TAX_CANADA]);
        error += conf.SetValue(tax_QST, vars[V_QST], sects[S_SALES_TAX_CANADA]);
        if (! conf.Save()) {
            cerr << "  failed to save tax config file" << endl;
            error++;
        }

        conf.SetFilename(CONFIG_FEES_FILE);

        error += conf.SetValue(royalty_rate, vars[V_ROYALTY_RATE], sects[S_MISC]);
        error += conf.SetValue(advertise_fund, vars[V_ADVERTISE_FUND], sects[S_MISC]);
        error += conf.SetValue(daily_cert_fee, vars[V_DAILY_CERT_FEE], sects[S_MISC]);

        error += conf.SetValue(debit_cost, vars[V_DEBIT_COST], sects[S_ELEC_TRANS]);
        error += conf.SetValue(credit_rate, vars[V_CREDIT_RATE], sects[S_ELEC_TRANS]);
        error += conf.SetValue(credit_cost, vars[V_CREDIT_COST], sects[S_ELEC_TRANS]);
        error += conf.SetValue(line_item_cost, vars[V_LINE_ITEM_COST], sects[S_ELEC_TRANS]);
        if (! conf.Save()) {
            cerr << "  failed to save fees config file" << endl;
            error++;
        }

        conf.SetFilename(CONFIG_FASTFOOD_FILE);
        
        error += conf.SetValue(personalize_fast_food, vars[V_PERSONALIZE_FAST_FOOD], sects[S_MISC]);
        error += conf.SetValue(tax_takeout_food, vars[V_TAX_TAKEOUT_FOOD], sects[S_MISC]);

        if (! conf.Save()) {
            cerr << "  failed to save fastfood config file" << endl;
            error++;
        }
    }

    return error;
}

/****
 *  MediaFirstID: This method will verify that the returned value is
 *    the lowest unique ID equal to or above idnum.  So if you call
 *    FirstID(50) you'll get the first unique ID 50 or above.
 ****/
int Settings::MediaFirstID(MediaInfo *mi, int idnum)
{
    FnTrace("Settings::MediaFirstID()");
    int retid = idnum;
    MediaInfo *curr = mi;

    while (curr != NULL && curr->Fore() != NULL)
        curr = curr->Fore();

    while (curr != NULL)
    {
        if (retid < curr->id)
        {
            curr = NULL;
        }
        else if (retid > curr->id)
        {
            curr = curr->Next();
        }
        else
        {
            retid += 1;
            curr = curr->Next();
        }
    }

    return retid;
}

int Settings::MediaIsDupe(MediaInfo *mi, int id, int thresh)
{
    FnTrace("Settings::MediaIsDupe()");
    int retval = 0;
    int count = 0;

    // rewind to head
    while (mi != NULL && mi->Fore() != NULL)
        mi = mi->Fore();

    // now accumulate a count
    while (mi != NULL)
    {
        if (mi->id == id)
            count += 1;
        mi = mi->Next();
    }

    if (count > thresh)
        retval = count;

    return retval;
}

int Settings::DiscountCount(int local, int active)
{
    FnTrace("Settings::DiscountCount()");
    DiscountInfo *discount = discount_list.Head();
    int count = 0;

    while (discount != NULL)
    {
        if (local == ALL_MEDIA && active == ALL_MEDIA)
            count += 1;
        else if (local == ALL_MEDIA && active == discount->active)
            count += 1;
        else if (local == discount->local && active == ALL_MEDIA)
            count += 1;
        else if (local == discount->local && active == discount->active)
            count += 1;
        discount = discount->next;
    }

    return count;
}

int Settings::CouponCount(int local, int active)
{
    FnTrace("Settings::CouponCount()");
    CouponInfo *coupon = coupon_list.Head();
    int count = 0;

    while (coupon != NULL)
    {
        if (local == ALL_MEDIA && active == ALL_MEDIA)
            count += 1;
        else if (local == ALL_MEDIA && active == coupon->active)
            count += 1;
        else if (local == coupon->local && active == ALL_MEDIA)
            count += 1;
        else if (local == coupon->local && active == coupon->active)
            count += 1;
        coupon = coupon->next;
    }

    return count;
}

int Settings::CreditCardCount(int local, int active)
{
    FnTrace("Settings::CreditCardCount()");
    CreditCardInfo *creditcard = creditcard_list.Head();
    int count = 0;

    while (creditcard != NULL)
    {
        if (local == ALL_MEDIA && active == ALL_MEDIA)
            count += 1;
        else if (local == ALL_MEDIA && active == creditcard->active)
            count += 1;
        else if (local == creditcard->local && active == ALL_MEDIA)
            count += 1;
        else if (local == creditcard->local && active == creditcard->active)
            count += 1;
        creditcard = creditcard->next;
    }

    return count;
}

int Settings::CompCount(int local, int active)
{
    FnTrace("Settings::CompCount()");
    CompInfo *comp = comp_list.Head();
    int count = 0;

    while (comp != NULL)
    {
        if (local == ALL_MEDIA && active == ALL_MEDIA)
            count += 1;
        else if (local == ALL_MEDIA && active == comp->active)
            count += 1;
        else if (local == comp->local && active == ALL_MEDIA)
            count += 1;
        else if (local == comp->local && active == comp->active)
            count += 1;
        comp = comp->next;
    }

    return count;
}

int Settings::MealCount(int local, int active)
{
    FnTrace("Settings::MealCount()");
    MealInfo *meal = meal_list.Head();
    int count = 0;

    while (meal != NULL)
    {
        if (local == ALL_MEDIA && active == ALL_MEDIA)
            count += 1;
        else if (local == ALL_MEDIA && active == meal->active)
            count += 1;
        else if (local == meal->local && active == ALL_MEDIA)
            count += 1;
        else if (local == meal->local && active == meal->active)
            count += 1;
        meal = meal->next;
    }

    return count;
}

/****
 * LoadMedia:  Comp, Coupon, CreditCard, and Discount objects are stored in
 *  a separate file if they are global.  That way, for example, global coupons
 *  can be created at one store and distributed to all stores.
 ****/
int Settings::LoadMedia(const char* file)
{
    FnTrace("Settings::LoadMedia()");
    if (file)
        discount_filename.Set(file);

    int my_discount_id = 0;
    int my_coupon_id = 0;
    int my_creditcard_id = 0;
    int my_comp_id = 0;
    int my_meal_id = 0;

    int version = 0;
    int count;

    InputDataFile df;
    if (df.Open(discount_filename.Value(), version))
        return 1;

    df.Read(my_discount_id);
    if (my_discount_id < GLOBAL_MEDIA_ID)
        my_discount_id = GLOBAL_MEDIA_ID;
    if (last_discount_id < my_discount_id)
        last_discount_id = my_discount_id;
    df.Read(count);
    int i;
    for (i = 0; i < count; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of discounts in media file");
            return 1;
        }
        DiscountInfo *ds = new DiscountInfo;
        ds->Read(df, version);
        if (ds->id < GLOBAL_MEDIA_ID || MediaIsDupe(discount_list.Head(), ds->id))
        {
            last_discount_id += 1;
            ds->id = last_discount_id;
        }
        else
        {
            last_discount_id = ds->id;
        }
        Add(ds);
    }

    df.Read(my_coupon_id);
    if (my_coupon_id < GLOBAL_MEDIA_ID)
        my_coupon_id = GLOBAL_MEDIA_ID;
    if (last_coupon_id < my_coupon_id)
        last_coupon_id = my_coupon_id;
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of coupons in media file");
            return 1;
        }
        CouponInfo *cp = new CouponInfo;
        cp->Read(df, version);
        if (cp->id < GLOBAL_MEDIA_ID || MediaIsDupe(coupon_list.Head(), cp->id))
        {
            last_coupon_id += 1;
            cp->id = last_coupon_id;
        }
        else
        {
            last_coupon_id = cp->id;
        }
        Add(cp);
    }

    df.Read(my_creditcard_id);
    if (my_creditcard_id < GLOBAL_MEDIA_ID)
        my_creditcard_id = GLOBAL_MEDIA_ID;
    if (last_creditcard_id < my_creditcard_id)
        last_creditcard_id = my_creditcard_id;
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of credit cards in media file");
            return 1;
        }
        CreditCardInfo *cc = new CreditCardInfo;
        cc->Read(df, version);
        if (cc->id < GLOBAL_MEDIA_ID || MediaIsDupe(creditcard_list.Head(), cc->id))
        {
            last_creditcard_id += 1;
            cc->id = last_creditcard_id;
        }
        else
        {
            last_creditcard_id = cc->id;
        }
        Add(cc);
    }

    df.Read(my_comp_id);
    if (my_comp_id < GLOBAL_MEDIA_ID)
        my_comp_id = GLOBAL_MEDIA_ID;
    if (last_comp_id < my_comp_id)
        last_comp_id = my_comp_id;
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of comps in media file");
            return 1;
        }
        CompInfo *cm = new CompInfo;
        cm->Read(df, version);
        if (cm->id < GLOBAL_MEDIA_ID || MediaIsDupe(comp_list.Head(), cm->id))
        {
            last_comp_id += 1;
            cm->id = last_comp_id;
        }
        else
        {
            last_comp_id = cm->id;
        }
        Add(cm);
    }

    if (version >= 44)
    {
        df.Read(my_meal_id);
        if (my_meal_id < GLOBAL_MEDIA_ID)
            my_meal_id = GLOBAL_MEDIA_ID;
        if (last_meal_id < my_meal_id)
            last_meal_id = my_meal_id;
        df.Read(count);
        for (i = 0; i < count; ++i)
        {
            if (df.end_of_file)
            {
                ReportError("Unexpected end of meals in media file");
                return 1;
            }
            MealInfo *mi = new MealInfo;
            mi->Read(df, version);
            if (mi->id < GLOBAL_MEDIA_ID || MediaIsDupe(meal_list.Head(), mi->id))
            {
                last_meal_id += 1;
                mi->id = last_meal_id;
            }
            else
            {
                last_meal_id = mi->id;
            }
            Add(mi);
        }
    }

    df.Close();
    return 0;
}

/****
 * SaveMedia:  We need to write the discounts, et al to a separate file
 *  so that the global items can be easily moved from store to store.
 ****/
int Settings::SaveMedia()
{
    FnTrace("Settings::SaveMedia()");
    int count;

    BackupFile(discount_filename.Value());

    // Write out SETTINGS_VERSION
    OutputDataFile df;
    if (df.Open(discount_filename.Value(), SETTINGS_VERSION))
        return 1;

    // Write out Discounts
    df.Write(last_discount_id);
    count = DiscountCount(GLOBAL_MEDIA);
    df.Write(count);
    DiscountInfo *discount = discount_list.Head();
    while (discount != NULL)
    {
        if (discount->IsGlobal())
        {
            if (discount->id < GLOBAL_MEDIA_ID)
                discount->id = MediaFirstID(discount_list.Head(), GLOBAL_MEDIA_ID);
            discount->Write(df, SETTINGS_VERSION);
        }
        discount = discount->next;
    }

    // Write out Coupons
    df.Write(last_coupon_id);
    count = CouponCount(GLOBAL_MEDIA);
    df.Write(count);
    CouponInfo *coupon = coupon_list.Head();
    while (coupon != NULL)
    {
        if (coupon->IsGlobal())
        {
            if (coupon->id < GLOBAL_MEDIA_ID)
                coupon->id = MediaFirstID(coupon_list.Head(), GLOBAL_MEDIA_ID);
            coupon->Write(df, SETTINGS_VERSION);
        }
        coupon = coupon->next;
    }

    // Write out Credit Card
    df.Write(last_creditcard_id);
    count = CreditCardCount(GLOBAL_MEDIA);
    df.Write(count);
    CreditCardInfo *creditcard = creditcard_list.Head();
    while (creditcard != NULL)
    {
        if (creditcard->IsGlobal())
        {
            if (creditcard->id < GLOBAL_MEDIA_ID)
                creditcard->id = MediaFirstID(creditcard_list.Head(), GLOBAL_MEDIA_ID);
            creditcard->Write(df, SETTINGS_VERSION);
        }
        creditcard = creditcard->next;
    }

    // Write out Comps
    df.Write(last_comp_id);
    count = CompCount(GLOBAL_MEDIA);
    df.Write(count);
    CompInfo *comp = comp_list.Head();
    while (comp != NULL)
    {
        if (comp->IsGlobal())
        {
            if (comp->id < GLOBAL_MEDIA_ID)
                comp->id = MediaFirstID(comp_list.Head(), GLOBAL_MEDIA_ID);
            comp->Write(df, SETTINGS_VERSION);
        }
        comp = comp->next;
    }

    // Write out Meals
    df.Write(last_meal_id);
    count = MealCount(GLOBAL_MEDIA);
    df.Write(count);
    MealInfo *meal = meal_list.Head();
    while (meal != NULL)
    {
        if (meal->IsGlobal())
        {
            if (meal->id < GLOBAL_MEDIA_ID)
                meal->id = MediaFirstID(meal_list.Head(), GLOBAL_MEDIA_ID);
            meal->Write(df, SETTINGS_VERSION);
        }
        meal = meal->next;
    }

    df.Close();
    return 0;
}

/****
 * SaveAltMedia:  Write out the current list of media items, local and
 *  global to a file that can be used by all old archives that do not
 *  have their own media.  The new archives get the media entries stored
 *  in them, so they always have the correct record of entries (if "5% Off"
 *  was used three times yesterday and then deleted, it will still show up
 *  in yesterday's archive).  The old archives do not have this information,
 *  and we don't want to modify them (write once, read many), but we also
 *  don't want the reports changing every time we modify the media list.
 *  SaveAltMedia will give us a static snapshot; it should be called only
 *  when the file does not exist.
 ****/
int Settings::SaveAltMedia(const genericChar* altmedia)
{
    FnTrace("Settings::SaveAltMedia()");
    int retval = 0;
    OutputDataFile outfile;
    struct stat sb;
    
    altdiscount_filename.Set(altmedia);
    if (stat(altmedia, &sb) < 0)
    {
        retval = outfile.Open(altmedia, SETTINGS_VERSION);
        if (retval == 0)
        {
            // Write Discounts
            outfile.Write(DiscountCount());
            DiscountInfo *discount = discount_list.Head();
            while (discount != NULL)
            {
                if (discount->active > 0)
                    discount->Write(outfile, SETTINGS_VERSION);
                discount = discount->next;
            }
            // Write Coupons
            outfile.Write(CouponCount());
            CouponInfo *coupon = coupon_list.Head();
            while (coupon != NULL)
            {
                if (coupon->active > 0)
                    coupon->Write(outfile, SETTINGS_VERSION);
                coupon = coupon->next;
            }
            // Write CreditCards
            outfile.Write(CreditCardCount());
            CreditCardInfo *creditcard = creditcard_list.Head();
            while (creditcard != NULL)
            {
                if (creditcard->active > 0)
                    creditcard->Write(outfile, SETTINGS_VERSION);
                creditcard = creditcard->next;
            }
            // Write Comps
            outfile.Write(CompCount());
            CompInfo *comp = comp_list.Head();
            while (comp != NULL)
            {
                if (comp->active > 0)
                    comp->Write(outfile, SETTINGS_VERSION);
                comp = comp->next;
            }
            // Write Meals
            outfile.Write(MealCount());
            MealInfo *meal = meal_list.Head();
            while (meal != NULL)
            {
                if (meal->active > 0)
                    meal->Write(outfile, SETTINGS_VERSION);
                meal = meal->next;
            }
            outfile.Close();
        }
    }
    return retval;
}

int Settings::SaveAltSettings(const genericChar* altsettings)
{
    FnTrace("Settings::SaveAltSettings()");
    int retval = 0;
    OutputDataFile outfile;
    struct stat sb;

    altsettings_filename.Set(altsettings);
    if (stat(altsettings, &sb) < 0)
    {
        retval = outfile.Open(altsettings, SETTINGS_VERSION);
        if (retval == 0)
        {
            outfile.Write(tax_food);
            outfile.Write(tax_alcohol);
            outfile.Write(tax_room);
            outfile.Write(tax_merchandise);
            outfile.Write(tax_GST);
            outfile.Write(tax_PST);
            outfile.Write(tax_HST);
            outfile.Write(tax_QST);
            outfile.Write(tax_VAT);
            outfile.Write(royalty_rate);
            outfile.Write(price_rounding);
            outfile.Write(change_for_credit);
            outfile.Write(change_for_roomcharge);
            outfile.Write(change_for_checks);
            outfile.Write(change_for_gift);
            outfile.Write(discount_alcohol);
            outfile.Write(tax_VAT);
        }
    }
    return retval;
}


int Settings::MealPeriod(TimeInfo &timevar)
{
    FnTrace("Settings::MealPeriod()");
    int meal = INDEX_GENERAL;
    int count = 0;

    int i;
    for (i = 0; i < MAX_MEALS; ++i)
    {
        if (meal_active[i] && meal_start[i] >= 0)
        {
            ++count;
            meal = i;
        }
    }

    if (count > 1)
    {
        int timeint = (timevar.Hour() * 60) + timevar.Min();
        for (i = 0; i < MAX_MEALS; ++i)
        {
            if (meal_active[i] && meal_start[i] >= 0 && timeint >= meal_start[i])
                meal = i;
        }
    }

    return meal;
}

int Settings::FirstShift()
{
    FnTrace("Settings::FirstShift()");
    int i;

    for (i = 0; i < shifts_used; ++i)
    {
        if (shift_start[i] >= 0)
            return i;
    }
    return -1;
}

int Settings::ShiftCount()
{
    FnTrace("Settings::ShiftCount()");
    int count = 0;
    int i;

    for (i = 0; i < shifts_used; ++i)
    {
        if (shift_start[i] >= 0)
            ++count;
    }
    return count;
}

int Settings::ShiftPosition(int shift)
{
    FnTrace("Settings::ShiftPosition()");
    if (shift_start[shift] < 0)
        return -1;
    int pos = 0;
    int i;

    for (i = 0; i <= shift; ++i)
    {
        if (shift_start[i] >= 0)
            ++pos;
    }
    return pos;
}

int Settings::ShiftNumber(TimeInfo &timevar)
{
    FnTrace("Settings::ShiftNumber()");
    int shift = -1;
    int count = 0;
    int i;

    for (i = 0; i < shifts_used; ++i)
    {
        if (shift_start[i] >= 0)
            ++count, shift = i;
    }

    if (count <= 1)
        return shift;

    int timeint = (timevar.Hour() * 60) + timevar.Min();
    for (i = 0; i < shifts_used; ++i)
    {
        if (shift_start[i] >= 0 && timeint >= shift_start[i])
            shift = i;
    }
    return shift;
}

int Settings::NextShift(int shift)
{
    FnTrace("Settings::NextShift()");
    int i = 0;
    do
    {
        ++shift;
        if (shift >= shifts_used)
            shift = 0;

        ++i;
        if (i > shifts_used)
            return -1;
    }
    while (shift_start[shift] < 0);
    return shift;
}

int Settings::ShiftText(char* str, int shift)
{
    FnTrace("Settings::ShiftText()");
    int t[2], ns = NextShift(shift);
    if (ns < 0)
        return 1;

    t[0] = shift_start[shift];
    t[1] = shift_start[ns];

    genericChar buffer[2][256];
    int h, m, pm;
    for (int i = 0; i < 2; ++i)
    {
        m = t[i] % 60;
        h = t[i] / 60;
        pm = (h >= 12);
        h = h % 12;
        if (h == 0)
            h = 12;

        if (m)
            sprintf(buffer[i], "%d:%02d", h, m);
        else
            sprintf(buffer[i], "%d", h);

        if (pm)
            strcat(buffer[i], "pm");
        else
            strcat(buffer[i], "am");
    }

    sprintf(str, "%s-%s", buffer[0], buffer[1]);
    return 0;
}

int Settings::ShiftStart(TimeInfo &start_time, int shift, TimeInfo &ref)
{
    FnTrace("Settings::ShiftStart()");
    int first = FirstShift();
    if (first < 0 || shift_start[shift] < 0)
        return 1;

    start_time = ref;
    start_time.Sec(0);
    start_time.Min(0);
    start_time.Hour(0);
    start_time.AdjustMinutes(shift_start[shift]);

    int minimum = (ref.Hour() * 60) + ref.Min();
    if (minimum < shift_start[first])
        start_time.AdjustDays(-1);
    return 0;
}

int Settings::IsGroupActive(int sales_group)
{
    FnTrace("Settings::IsGroupActive()");
    int i;

    for (i = 0; FamilyValue[i] >= 0; ++i)
    {
        if (family_group[FamilyValue[i]] == sales_group)
            return 1;
    }
    return 0;
}

int Settings::MonthPeriod(TimeInfo &ref, TimeInfo &start, TimeInfo &end)
{
    FnTrace("Settings::MonthPeriod()");
    TimeInfo timevar = ref;
    timevar.Sec(0);
    timevar.Min(0);
    timevar.Hour(0);
    
    start = timevar;
    start.Day(1);
    end = start;
    end.AdjustDays(DaysInMonth(start.Month(), start.Year()));
    
//    printf("Settings::MonthPeriod(): ref=%d/%d/%d\nstart=%d/%d/%d\nend=%d/%d/%d\n",
//    ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year()); 
    return 0;
}

int Settings::SalesPeriod(TimeInfo &ref, TimeInfo &start, TimeInfo &end)
{
    FnTrace("Settings::SalesPeriod()");
    if (sales_period == SP_MONTH)
    {
        TimeInfo timevar = ref;
        timevar.Sec(0);
        timevar.Min(0);
        timevar.Hour(0);

        start = timevar;
        start.Day(sales_start.Day());
        end = start;
        end.AdjustDays(DaysInMonth(start.Month(), start.Year()));
        return 0;
    }

    if (sales_period == SP_HM_11)
    {
        TimeInfo timevar = ref;
        timevar.Sec(0);
        timevar.Min(0);
        timevar.Hour(0);

        start = timevar;
        start.Day(sales_start.Day());
        if (start.Day() <= 11) {
            start.Day(11);
            end = start;
            end.AdjustDays(14);
        } else if (start.Day() < 26) {
            start.Day(26);
            end = start;
            end.AdjustMonths(1);
            end.Day(10);
        } else {
            start.Day(11);
            start.AdjustMonths(1);
            end = start;
            end.AdjustDays(14);
        }
//    printf("Settings::SalesPeriod(): ref=%d/%d/%d : start=%d/%d/%d : end=%d/%d/%d\n",
//    ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year()); 
        return 0;
    }

    int d = 0;
    switch (sales_period)
    {
    case SP_WEEK:   d = 7; break;
    case SP_2WEEKS: d = 14; break;
    case SP_4WEEKS: d = 28; break;
    default:
        return 1;
    }

    sales_start.Sec(0);
    sales_start.Min(0);
    sales_start.Hour(0);

    end = sales_start;
    end.AdjustDays(d);

    while (end <= ref)
        end.AdjustDays(d);

    start = end;
    start.AdjustDays(-d);
    return 0;
}

int Settings::LaborPeriod(TimeInfo &ref, TimeInfo &start, TimeInfo &end)
{
    FnTrace("Settings::LaborPeriod()");
    //printf("Settings::LaborPeriod(Start): ref=%d/%d/%d\nstart=%d/%d/%d\nend=%d/%d/%d\n",
    //ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year());
    //printf("labor_period=%d, SP_MONTH=%d, SP_HM_11=%d, PERIOD_HM_11=%d\n",labor_period, SP_MONTH, SP_HM_11, PERIOD_HM_11); 
    if (labor_period == SP_MONTH)
    {
        TimeInfo timevar = ref;
        timevar.Sec(0);
        timevar.Min(0);
        timevar.Hour(0);

        start = timevar;
        start.Day(labor_start.Day());
        end = start;
        end.AdjustDays(DaysInMonth(start.Month(), start.Year()));
    //printf("Settings::LaborPeriod(Month): ref=%d/%d/%d\nstart=%d/%d/%d\nend=%d/%d/%d\n",
    //ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year());
        return 0;
    }

// should this be PERIOD_HM_11?????
//    if (labor_period == PERIOD_HM_11)
    if (labor_period == SP_HM_11)
    {
        TimeInfo timevar = ref;
        timevar.Sec(0);
        timevar.Min(0);
        timevar.Hour(0);

        start = timevar;
        end = timevar;
        if (start.Day() < 11) {
            start.Day(26);
            start.AdjustMonths(-1);
//          end.Day(10);
            end.Day(11);
        } else if (start.Day() < 26) {
            start.Day(11);
            end = start;
//          end.AdjustDays(14);
            end.AdjustDays(15);
        } else {
            start.Day(26);
//          end.Day(10);
            end.Day(11);
            end.AdjustMonths(1);
        }
    //printf("Settings::LaborPeriod(SP_HM_11): ref=%d/%d/%d\nstart=%d/%d/%d\nend=%d/%d/%d\n",
    //ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year());
        return 0;
    }

    int d = 0;
    switch (labor_period)
    {
    case SP_WEEK:   d = 7; break;
    case SP_2WEEKS: d = 14; break;
    case SP_4WEEKS: d = 28; break;
    default:
        return 1;
    }

    labor_start.Sec(0);
    labor_start.Min(0);
    labor_start.Hour(0);

    end = labor_start;
    end.AdjustDays(d);

    while (end <= ref)
        end.AdjustDays(d);

    start = end;
    start.AdjustDays(-d);
    return 0;
}

int Settings::SetPeriod(TimeInfo &ref, TimeInfo &start, TimeInfo &end,
                        int period_view, TimeInfo *fiscal)
{
    FnTrace("Settings::SetPeriod()");

    // set the starting date according to fiscal period if we have it,
    // first day of first month of current year if not.
    if (fiscal && fiscal->IsSet())
    {
        fiscal->Sec(0);
        end = *fiscal;
// printf("Settings::SetPeriod(): Fiscal reset end to %d/%d/%d\n", end.Month(), end.Day(), end.Year()); 
    }
    else
    {
        end.Set();
        end.Sec(0);
        end.Min(0);
        end.Hour(0);
        end.Day(1);
        end.Month(1);
        end.Year(SystemTime.Year());
// printf("Settings::SetPeriod(): Fiscal not set so end set to %d/%d/%d\n", end.Month(), end.Day(), end.Year()); 
    }
//    printf("Settings::SetPeriod(): beginning ref=%d/%d/%d : start=%d/%d/%d : end=%d/%d/%d\n",
//    ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year()); 

    // make sure we have the correct year
    while (end >= ref)
        end.AdjustYears(-1);

    // advance until end is past reference point
    while (end <= ref)
    {
        switch (period_view)
        {
        case SP_DAY:
            end.AdjustDays(1);
            break;
        case SP_WEEK:
            end.AdjustDays(7);
            break;
        case SP_2WEEKS:
            end.AdjustDays(14);
            break;
        case SP_HM_11:
//    printf("Settings::SetPeriod(): in top ref=%d/%d/%d : end=%d/%d/%d\n",
//    ref.Month(), ref.Day(), ref.Year(), end.Month(), end.Day(), end.Year()); 
            end.AdjustDays(1);
            if (end >= ref) {
                if (ref.Day() <= 10) {
//                                            printf("Settings::SetPeriod(): setting day to 10\n");
                    end.Day(10);
                } else if (ref.Day() <= 25) {
                    end.Day(25);
//                                          printf("Settings::SetPeriod(): setting day to 25\n");
                } else {
                    end.AdjustMonths(1);
                    end.Day(10);
//                                          printf("Settings::SetPeriod(): skipping to 10th of next month\n");
                }
            }
            break;
        case SP_4WEEKS:
            end.AdjustDays(28);
            break;
        case SP_MONTH:
            end.AdjustMonths(1);
            break;
        case SP_QUARTER:
            end.AdjustMonths(3);
            break;
        case SP_YTD:
            end.AdjustYears(1);
            break;
        }
//    printf("Settings::SalesPeriod() Mid: end=%d/%d/%d\n",
//    end.Month(), end.Day(), end.Year()); 
        
    }
//    printf("Settings::SetPeriod(): mid ref=%d/%d/%d : start=%d/%d/%d : end=%d/%d/%d\n",
//    ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year()); 

    // scroll the start back
    start = end;
    switch (period_view)
    {
    case SP_DAY:
        start.AdjustDays(-1);
        break;
    case SP_WEEK:
        start.AdjustDays(-7);
        break;
    case SP_2WEEKS:
        start.AdjustDays(-14);
        break;
    case SP_HM_11:
    // end.Day() should either be 10 or 26, so we need to set start to be
    // the most previous 11 or 25.
        if (end.Day() == 10) {
            start.Day(26);
            start.AdjustMonths(-1);
//          start.AdjustDays(-1*((DaysInMonth(start.Month()-1, start.Year()) - 26) +10));
        } else {
            start.AdjustDays(-14);
        }
        // ref = end;
        ref = start;
        break;
    case SP_4WEEKS:
        start.AdjustDays(-28);
        break;
    case SP_MONTH:
        start.AdjustMonths(-1);
        break;
    case SP_QUARTER:
        start.AdjustMonths(-3);
        break;
    case SP_YTD:
        start.AdjustYears(-1);
        break;
    }
//    printf("Settings::SetPeriod(): end ref=%d/%d/%d : start=%d/%d/%d : end=%d/%d/%d\n",
//    ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year()); 

//    ref = start;

    return 0;
}

int Settings::OvertimeWeek(TimeInfo &ref, TimeInfo &start, TimeInfo &end)
{
    FnTrace("Settings::OvertimeWeek()");
    start = ref;
    start.Min(wage_week_start % 60);
    start.Hour((wage_week_start / 60) % 24);

    int wday = (wage_week_start / 1440) % 7;
    int ref_wday = ref.WeekDay();
    if (ref_wday == wday)
    {
        if (start > ref)
            start.AdjustDays(-7);
    }
    else if (ref_wday > wday)
        start.AdjustDays(wday - ref_wday);
    else
        start.AdjustDays(wday - (ref_wday + 7));

    end = start;
    end.AdjustDays(7);

    if (start > ref)
        printf("start wrong\n");
    if (end <= ref)
        printf("end wrong\n");
    return 0;
}

char* Settings::StoreNum(char* dest)
{
    FnTrace("Settings::StoreNum()");
    static char buffer[STRLONG] = "";

    if (dest == NULL)
        dest = buffer;

    sprintf(dest, "%d", store_code);

    return dest;
}

int Settings::FigureFoodTax(int amount, TimeInfo &timevar, Flt tax)
{
    FnTrace("Settings::FigureFoodTax()");
    Flt tax_amount = (tax >= 0) ? tax : tax_food;
    int total_tax = 0;

    Flt f = (Flt) amount * tax_amount;
    total_tax = (int) (f + .999999);  // round up

    return total_tax;
}

int Settings::FigureAlcoholTax(int amount, TimeInfo &timevar, Flt tax)
{
    FnTrace("Settings::FigureAlcoholTax()");
    Flt tax_amount = (tax >= 0) ? tax : tax_alcohol;
    int total_tax = 0;

    Flt f = (Flt) amount * tax_amount;
    total_tax = (int) (f + .999999);  // round up

    return total_tax;
}

int Settings::FigureGST(int amount, TimeInfo &timevar, Flt tax)
{
    FnTrace("Settings::FigureGST()");
    Flt tax_amount = (tax >= 0) ? tax : tax_GST;
    int total_tax = 0;

    Flt f = (Flt) amount * tax_amount;
    total_tax = (int) (f + .999999);  // round up

    return total_tax;
}

int Settings::FigurePST(int amount, TimeInfo &timevar, bool isBeverage, Flt tax)
{
    FnTrace("Settings::FigurePST()");
    Flt tax_amount = (tax >= 0) ? tax : tax_PST;
    int total_tax = 0;

    if ((Flt)amount > 399 || isBeverage == true)
    {
        Flt f = (Flt) amount * tax_amount;
        total_tax = (int) (f + .999999);  // round up
    }

    return total_tax;
}

int Settings::FigureHST(int amount, TimeInfo &timevar, Flt tax)
{
    FnTrace("Settings::FigureHST()");
    Flt tax_amount = (tax >= 0) ? tax : tax_HST;
    int total_tax = 0;

    Flt f = (Flt) amount * tax_amount;
    total_tax = (int) (f + .999999);  // round up

    return total_tax;
}

int Settings::FigureQST(int amount, int gst, TimeInfo &timevar, bool isBeverage, Flt tax)
{
    FnTrace("Settings::FigureQST()");
    Flt tax_amount = (tax >= 0) ? tax : tax_QST;
    int totalTax = 0;

    Flt f = (Flt) (amount + gst) * tax_amount;
    totalTax = (int) (f + .999999);  // round up

    return totalTax;
}

int Settings::FigureRoomTax(int amount, TimeInfo &timevar, Flt tax)
{
    FnTrace("Settings::FigureRoomTax()");
    Flt tax_amount = (tax >= 0) ? tax : tax_room;
    int total_tax = 0;

    Flt f = (Flt) amount * tax_amount;
    total_tax = (int) (f + .999999);  // round up

    return total_tax;
}

int Settings::FigureMerchandiseTax(int amount, TimeInfo &timevar, Flt tax)
{
    FnTrace("Settings::FigureMerchandiseTax()");
    Flt tax_amount = (tax >= 0) ? tax : tax_merchandise;
    int total_tax = 0;

    Flt f = (Flt) amount * tax_amount;
    total_tax = (int) (f + .999999);  // round up

    return total_tax;
}

int Settings::FigureVAT(int amount, TimeInfo &timevar, Flt tax)
{
    FnTrace("Settings::FigureVAT()");
    Flt tax_amount = (tax >= 0) ? tax : tax_VAT;
    int total_tax = 0;

    Flt f = (Flt) amount * tax_amount;
    total_tax = (int) (f + .9999999);

    return total_tax;
}

char* Settings::TenderName(int tender_type, int tender_id, genericChar* str)
{
    FnTrace("Settings::TenderName()");
    static const genericChar* name[] = {
        "Cash Received", "Check", "Gift Certificate", "House Account", "Overage",
        "Change", "Tip", "Payout", "Money Lost", "Gratuity", "Tips Paid",
        "ATM/Debit Card", "Credit Card Tip", "Expenses", "Cash", NULL};
    static int value[] = {
        TENDER_CASH, TENDER_CHECK, TENDER_GIFT, TENDER_ACCOUNT, TENDER_OVERAGE,
        TENDER_CHANGE, TENDER_CAPTURED_TIP, TENDER_PAYOUT, TENDER_MONEY_LOST,
        TENDER_GRATUITY, TENDER_PAID_TIP, TENDER_DEBIT_CARD,
        TENDER_CHARGED_TIP, TENDER_EXPENSE, TENDER_CASH_AVAIL, -1};
    const char* temp;
    static char buffer[STRLENGTH];

    // We should pass in a Terminal instance, I guess, so we can properly translate.
    // But I'm not going to require all of the calling functions to do that.  So
    // we'll do internal translation.  We'll call the last terminal because that
    // should always be the Server terminal, which is the only terminal most
    // guaranteed to always be present.
    Terminal *term = MasterControl->TermListEnd();
    char str2[STRLENGTH];

    if (str == NULL)
        str = buffer;

    if (tender_type == TENDER_CHARGE_ROOM)
    {
        if (tender_id <= 0)
            strcpy(str, "Room Charge");
        else
            sprintf(str, "Charge Room #%d", tender_id);
    }
    else if (tender_type == TENDER_CHARGE_CARD)
    {
        CreditCardInfo *cc = FindCreditCardByID(tender_id);
        if (cc)
            strcpy(str, cc->name.Value());
        else
            strcpy(str, "Unknown Credit Card");
    }
    else if (tender_type == TENDER_CREDIT_CARD)
    {
        temp = FindStringByValue(tender_id, CreditCardValue, CreditCardShortName);
        strcpy(str, temp);
    }
    else if (tender_type == TENDER_DEBIT_CARD)
    {
        temp = FindStringByValue(CARD_TYPE_DEBIT, CardTypeValue, CardTypeName);
        strcpy(str, temp);
    }
    else if (tender_type == TENDER_DISCOUNT)
    {
        DiscountInfo *ds = FindDiscountByID(tender_id);
        if (ds)
            strcpy(str, ds->name.Value());
        else
            strcpy(str, "Unknown Discount");
    }
    else if (tender_type == TENDER_COUPON)
    {
        CouponInfo *cp = FindCouponByID(tender_id);
        if (cp)
            strcpy(str, cp->name.Value());
        else
            strcpy(str, "Unknown Coupon");
    }
    else if (tender_type == TENDER_COMP)
    {
        CompInfo *cm = FindCompByID(tender_id);
        if (cm)
            strcpy(str, cm->name.Value());
        else
            strcpy(str, "Unknown Comp");
    }
    else if (tender_type == TENDER_EMPLOYEE_MEAL)
    {
        MealInfo *mi = FindMealByID(tender_id);
        if (mi)
            strcpy(str, mi->name.Value());
        else
            strcpy(str, "Unknown Employee Meal");
    }
    else
        strcpy(str, FindStringByValue(tender_type, value, name, UnknownStr));

    if (term != NULL)
    {
        strcpy(str2, term->Translate(str));
        strcpy(str, str2);
    }

    return str;
}

int Settings::Add(DiscountInfo *ds)
{
    FnTrace("Settings::Add(DiscountInfo)");
    if (ds == NULL)
        return 1;
    DiscountInfo *node = discount_list.Head();

    if (ds->id < 1)
    {
        if (node != NULL)
            ds->id = MediaFirstID(discount_list.Head(), 1);
        else
            ds->id = 1;
    }

    while (node != NULL)
    {
        if (ds->id < node->id)
        {
            discount_list.AddBeforeNode(node, ds);
            break;
        }
        else
            node = node->next;
    }
    if (node == NULL)
    {
        discount_list.AddToTail(ds);
    }

    return 0;
}

int Settings::Add(CouponInfo *cp)
{
    FnTrace("Settings::Add(CouponInfo)");
    if (cp == NULL)
        return 1;
    CouponInfo *node = coupon_list.Head();

    if (cp->id < 1)
    {
        if (node != NULL)
            cp->id = MediaFirstID(coupon_list.Head(), 1);
        else
            cp->id = 1;
    }

    while (node != NULL)
    {
        if (cp->id < node->id)
        {
            coupon_list.AddBeforeNode(node, cp);
            break;
        }
        else
            node = node->next;
    }
    if (node == NULL)
        coupon_list.AddToTail(cp);
    return 0;
}

int Settings::Add(CreditCardInfo *cc)
{
    FnTrace("Settings::Add(CreditCardInfo)");
    if (cc == NULL)
        return 1;
    CreditCardInfo *node = creditcard_list.Head();

    if (cc->id < 1)
    {
        if (node != NULL)
            cc->id = MediaFirstID(creditcard_list.Head(), 1);
        else
            cc->id = 1;
    }

    while (node != NULL)
    {
        if (cc->id < node->id)
        {
            creditcard_list.AddBeforeNode(node, cc);
            break;
        }
        else
            node = node->next;
    }

    if (node == NULL)
        creditcard_list.AddToTail(cc);

    return 0;
}

int Settings::Add(CompInfo *cm)
{
    FnTrace("Settings::Add(CompInfo)");
    if (cm == NULL)
        return 1;
    CompInfo *node = comp_list.Head();

    if (cm->id < 1)
    {
        if (node != NULL)
            cm->id = MediaFirstID(comp_list.Head(), 1);
        else
            cm->id = 1;
    }

    while (node != NULL)
    {
        if (cm->id < node->id)
        {
            comp_list.AddBeforeNode(node, cm);
            break;
        }
        else
            node = node->next;
    }
    if (node == NULL)
        comp_list.AddToTail(cm);
    return 0;
}

int Settings::Add(MealInfo *mi)
{
    FnTrace("Settings::Add(MealInfo)");
    if (mi == NULL)
        return 1;
    MealInfo *node = meal_list.Head();

    if (mi->id < 1)
    {
        if (node != NULL)
            mi->id = MediaFirstID(meal_list.Head(), 1);
        else
            mi->id = 1;
    }

    while (node != NULL)
    {
        if (mi->id < node->id)
        {
            meal_list.AddBeforeNode(node, mi);
            break;
        }
        else
            node = node->next;
    }
    if (node == NULL)
        meal_list.AddToTail(mi);
    return 0;
}

int Settings::HaveServerTerm()
{
    FnTrace("Settings::HaveServerTerm()");
    int retval = 0;
    TermInfo *ti = TermList();

    while (ti != NULL)
    {
        if (ti->IsServer())
            retval += 1;
        ti = ti->next;
    }

    return retval;
}

int Settings::Add(TermInfo *ti)
{
    FnTrace("Settings::Add(TermInfo)");
    return term_list.AddToTail(ti);
}

int Settings::AddFront(TermInfo *ti)
{
    FnTrace("Settings::AddFront(TermInfo)");
    return term_list.AddToHead(ti);
}

int Settings::Add(PrinterInfo *pr)
{
    FnTrace("Settings::Add(PrinterInfo)");
    return printer_list.AddToTail(pr);
}

int Settings::Add(MoneyInfo *my)
{
    FnTrace("Settings::Add(MoneyInfo)");
    return money_list.AddToTail(my);
}

int Settings::Add(TaxInfo *tx)
{
    FnTrace("Settings::Add(TaxInfo)");
    return tax_list.AddToTail(tx);
}

int Settings::Remove(DiscountInfo *ds)
{
    FnTrace("Settings::Remove(DiscountInfo)");
    return discount_list.Remove(ds);
}

int Settings::Remove(CouponInfo *cp)
{
    FnTrace("Settings::Remove(CouponInfo)");
    return coupon_list.Remove(cp);
}

int Settings::Remove(CreditCardInfo *cc)
{
    FnTrace("Settings::Remove(CreditCardInfo)");
    return creditcard_list.Remove(cc);
}

int Settings::Remove(CompInfo *cm)
{
    FnTrace("Settings::Remove(CompInfo)");
    return comp_list.Remove(cm);
}

int Settings::Remove(MealInfo *mi)
{
    FnTrace("Settings::Remove(MealInfo)");
    return meal_list.Remove(mi);
}

int Settings::Remove(TermInfo *ti)
{
    FnTrace("Settings::Remove(TermInfo)");
    return term_list.Remove(ti);
}

int Settings::Remove(PrinterInfo *pr)
{
    FnTrace("Settings::Remove(PrinterInfo)");
    return printer_list.Remove(pr);
}

int Settings::Remove(MoneyInfo *my)
{
    FnTrace("Settings::Remove(MoneyInfo)");
    return money_list.Remove(my);
}

int Settings::DiscountReport(Terminal *t, Report *r)
{
    FnTrace("Settings::DiscountReport()");
    if (r == NULL)
        return 1;

    int color = COLOR_DEFAULT;
    DiscountInfo *ds = discount_list.Head();
    if (ds == NULL)
    {
        r->TextC(t->Translate("No Discounts Defined Yet"));
        return 0;
    }

    genericChar str[STRLENGTH];
    while (ds)
    {
        if (ds->active)
        {
            if (ds->IsGlobal())
                color = COLOR_BLUE;
            else
                color = COLOR_DEFAULT;
            r->TextL(ds->name.Value(), color);
            if (debug_mode)
            {
                snprintf(str, STRLENGTH, "%d", ds->id);
                r->TextC(str, COLOR_RED);
            }
            if (ds->flags & TF_IS_PERCENT)
                sprintf(str, "%g%%", (Flt) ds->amount / 100.0);
            else
                t->FormatPrice(str, ds->amount, 1);
            r->TextR(str, color);      
            r->NewLine();
        }
        ds = ds->next;
    }
    return 0;
}

int Settings::CouponReport(Terminal *t, Report *r)
{
    FnTrace("Settings::CouponReport()");
    if (r == NULL)
        return 1;

    int color = COLOR_DEFAULT;
    CouponInfo *cp = coupon_list.Head();
    if (cp == NULL)
    {
        r->TextC(t->Translate("No Coupons Defined Yet"));
        return 0;
    }

    genericChar str[STRLENGTH];
    while (cp)
    {
        if (cp->active)
        {
            if (cp->IsGlobal())
                color = COLOR_BLUE;
            else
                color = COLOR_DEFAULT;
            r->TextL(cp->name.Value(), color);
            if (debug_mode)
            {
                snprintf(str, STRLENGTH, "%d", cp->id);
                r->TextC(str, COLOR_RED);
            }
            if (cp->flags & TF_IS_PERCENT)
                sprintf(str, "%g%%", (Flt) cp->amount / 100.0);
            else
                t->FormatPrice(str, cp->amount, 1);
            r->TextR(str, color);
            r->NewLine();
        }
        cp = cp->next;
    }
    return 0;
}

int Settings::CreditCardReport(Terminal *t, Report *r)
{
    FnTrace("Settings::CreditCardReport()");
    if (r == NULL)
        return 1;

    int color = COLOR_DEFAULT;
    CreditCardInfo *cc = creditcard_list.Head();
    if (cc == NULL)
    {
        r->TextC(t->Translate("No Credit Cards Defined Yet"));
        return 0;
    }

    while (cc)
    {
        if (cc->active)
        {
            if (cc->IsGlobal())
                color = COLOR_BLUE;
            else
                color = COLOR_DEFAULT;
            r->TextL(cc->name.Value(), color);
            if (debug_mode)
            {
                genericChar str[STRLENGTH];
                snprintf(str, STRLENGTH, "%d", cc->id);
                r->TextC(str, COLOR_RED);
            }
            r->NewLine();
        }
        cc = cc->next;
    }
    return 0;
}

int Settings::CompReport(Terminal *t, Report *r)
{
    FnTrace("Settings::CompReport()");
    if (r == NULL)
        return 1;

    int color = COLOR_DEFAULT;
    CompInfo *cm = comp_list.Head();
    if (cm == NULL)
    {
        r->TextC(t->Translate("No Whole Meal Comps Defined Yet"));
        return 0;
    }

    while (cm)
    {
        if (cm->active)
        {
            if (cm->IsGlobal())
                color = COLOR_BLUE;
            else
                color = COLOR_DEFAULT;
            r->TextL(cm->name.Value(), color);
            if (debug_mode)
            {
                genericChar str[STRLENGTH];
                snprintf(str, STRLENGTH, "%d", cm->id);
                r->TextC(str, COLOR_RED);
            }
            r->NewLine();
        }
        cm = cm->next;
    }
    return 0;
}

int Settings::MealReport(Terminal *t, Report *r)
{
    FnTrace("Settings::MealReport()");
    if (r == NULL)
        return 1;

    MealInfo *mi = meal_list.Head();
    if (mi == NULL)
    {
        r->TextC(t->Translate("No Employee Discounts Defined Yet"));
        return 0;
    }

    genericChar str[STRLENGTH];
    while (mi)
    {
        if (mi->active)
        {
            r->TextL(mi->name.Value());
            if (mi->flags & TF_IS_PERCENT)
                sprintf(str, "%g%%", (Flt) mi->amount / 100.0);
            else
                t->FormatPrice(str, mi->amount, 1);
            if (debug_mode)
            {
                snprintf(str, STRLENGTH, "%d", mi->id);
                r->TextC(str, COLOR_RED);
            }
            r->TextR(str);
            r->NewLine();
        }
        mi = mi->next;
    }
    return 0;
}

int Settings::RemoveInactiveMedia()
{
    FnTrace("Settings::RemoveInactiveMedia()");
    
    // Remove inactive discounts
    DiscountInfo *discount_node = discount_list.Head();
    while (discount_node != NULL)
    {
        if (discount_node->active == 0)
        {
            discount_list.Remove(discount_node);
            discount_node = discount_list.Head();
        }
        else
            discount_node = discount_node->next;
    }

    // Remove inactive coupons
    CouponInfo *coupon_node = coupon_list.Head();
    while (coupon_node != NULL)
    {
        if (coupon_node->active == 0)
        {
            coupon_list.Remove(coupon_node);
            coupon_node = coupon_list.Head();
        }
        else
            coupon_node = coupon_node->next;
    }

    // Remove inactive comps
    CompInfo *comp_node = comp_list.Head();
    while (comp_node != NULL)
    {
        if (comp_node->active == 0)
        {
            comp_list.Remove(comp_node);
            comp_node = comp_list.Head();
        }
        else
            comp_node = comp_node->next;
    }

    // Remove inactive credit cards
    CreditCardInfo *creditcard_node = creditcard_list.Head();
    while (creditcard_node != NULL)
    {
        if (creditcard_node->active == 0)
        {
            creditcard_list.Remove(creditcard_node);
            creditcard_node = creditcard_list.Head();
        }
        else
            creditcard_node = creditcard_node->next;
    }

    // Remove inactive meals
    MealInfo *meal_node = meal_list.Head();
    while (meal_node != NULL)
    {
        if (meal_node->active == 0)
        {
            meal_list.Remove(meal_node);
            meal_node = meal_list.Head();
        }
        else
            meal_node = meal_node->next;
    }

    Save();
    return 0;
}

int Settings::TermReport(Terminal *t, Report *r)
{
    FnTrace("Settings::TermReport()");
    if (r == NULL)
        return 1;

    r->update_flag = UPDATE_TERMINALS | UPDATE_USERS;
    TermInfo *ti = term_list.Head();
    if (ti == NULL)
    {
        r->TextC(t->Translate("No Terminals Defined Yet"));
        return 0;
    }

    while (ti)
    {
        r->TextL(ti->name.Value());
        if (ti->IsServer())
            r->TextPosL(22, "");
        else
            r->TextPosL(22, ti->display_host.Value());

        Terminal *term = ti->FindTerm(t->parent);
        if (term)
        {
            Employee *e = term->user;
            if (e)
                r->TextPosL(38, e->system_name.Value());
            else
                r->TextPosL(38, "---");

            if (term->page)
                r->TextPosL(58, t->Translate("Okay"), COLOR_GREEN);
            else
                r->TextPosL(58, t->Translate("Connecting"), COLOR_BLUE);
        }
        else
        {
            r->TextPosL(38, "---");
            r->TextPosL(58, t->Translate("Off Line"), COLOR_RED);
        }

        r->NewLine();
        ti = ti->next;
    }
    return 0;
}

int Settings::PrinterReport(Terminal *t, Report *r)
{
    FnTrace("Settings::PrinterReport()");
    if (r == NULL)
        return 1;
    genericChar buffer[STRLENGTH];

    r->update_flag = UPDATE_PRINTERS;
    PrinterInfo *pi = printer_list.Head();
    if (pi == NULL)
    {
        r->TextC(t->Translate("No Printers Defined"));
        return 0;
    }

    while (pi)
    {
        int idx = CompareList(pi->type, PrinterTypeValue);
        if (idx < 0)
            r->TextL(t->Translate("Unknown Type"));
        else
            r->TextL(PrinterTypeName[idx]);
        strncpy(buffer, pi->host.Value(), 15);
        if (strlen(pi->host.Value()) > 15)
        {
            buffer[15] = '\0';
            strcat(buffer, "...");
        }
        r->TextPosL(15, buffer);

        idx = CompareList(pi->port, PortValue);
        if (idx < 0)
            r->TextPosL(30, t->Translate("Unknown Interface"));
        else
            r->TextPosL(30, PortName[idx]);

        idx = CompareList(pi->model, PrinterModelValue);
        if (idx < 0)
            r->TextPosL(47, t->Translate("Unknown Model"));
        else
            r->TextPosL(47, PrinterModelName[idx]);

        Printer *p = pi->FindPrinter(t->parent);
        if (p)
            r->TextPosL(58, t->Translate("Okay"), COLOR_GREEN);
        else
            r->TextPosL(58, t->Translate("Off Line"), COLOR_RED);

        r->NewLine();
        pi = pi->next;
    }
    return 0;
}

DiscountInfo *Settings::FindDiscountByRecord(int record)
{
    FnTrace("Settings::FindDiscountByRecord()");
    int idx = 0;
    DiscountInfo *discount = discount_list.Head();
    DiscountInfo *retdiscount = NULL;
    while ((discount != NULL) && (retdiscount == NULL))
    {
        if (discount->active)
        {
            if (idx == record)
                retdiscount = discount;
            else
                idx += 1;
        }
        discount = discount->next;
    }
    return retdiscount;
}

DiscountInfo *Settings::FindDiscountByID(int id)
{
    FnTrace("Settings::FindDiscountByID()");
    for (DiscountInfo *ds = discount_list.Head(); ds != NULL; ds = ds->next)
    {
        if (ds->id == id)
            return ds;
    }
    return NULL;
}

CouponInfo *Settings::FindCouponByRecord(int record)
{
    FnTrace("Settings::FindCouponByRecord()");
    int idx = 0;
    CouponInfo *coupon = coupon_list.Head();
    CouponInfo *retcoupon = NULL;
    while ((coupon != NULL) && (retcoupon == NULL))
    {
        if (coupon->active)
        {
            if (idx == record)
                retcoupon = coupon;
            else
                idx += 1;
        }
        coupon = coupon->next;
    }
    return retcoupon;
}

CouponInfo *Settings::FindCouponByID(int id)
{
    FnTrace("Settings::FindCouponByID()");
    for (CouponInfo *cp = coupon_list.Head(); cp != NULL; cp = cp->next)
    {
        if (cp->id == id)
            return cp;
    }
    return NULL;
}

CouponInfo *Settings::FindCouponByItem(SalesItem *item, int aut)
{
    FnTrace("Settings::FindCouponByItem()");
    CouponInfo *retval = NULL;
    CouponInfo *coupon = coupon_list.Head();

    while (coupon != NULL)
    {
        if (coupon->Applies(item, aut))
        {
            retval = coupon;
            coupon = NULL;
        }
        else
            coupon = coupon->next;
    }

    return retval;
}

CompInfo *Settings::FindCompByRecord(int record)
{
    FnTrace("Settings::FindCompByRecord()");
    int idx = 0;
    CompInfo *comp = comp_list.Head();
    CompInfo *retcomp = NULL;
    while ((comp != NULL) && (retcomp == NULL))
    {
        if (comp->active)
        {
            if (idx == record)
                retcomp = comp;
            else
                idx += 1;
        }
        comp = comp->next;
    }
    return retcomp;
}

CompInfo *Settings::FindCompByID(int id)
{
    FnTrace("Settings::FindCompByID()");
    for (CompInfo *cm = comp_list.Head(); cm != NULL; cm = cm->next)
    {
        if (cm->id == id)
            return cm;
    }
    return NULL;
}

CreditCardInfo *Settings::FindCreditCardByRecord(int record)
{
    FnTrace("Settings::FindCreditCardByRecord()");
    int idx = 0;
    CreditCardInfo *creditcard = creditcard_list.Head();
    CreditCardInfo *retcreditcard = NULL;
    while ((creditcard != NULL) && (retcreditcard == NULL))
    {
        if (creditcard->active)
        {
            if (idx == record)
                retcreditcard = creditcard;
            else
                idx += 1;
        }
        creditcard = creditcard->next;
    }
    return retcreditcard;
}

CreditCardInfo *Settings::FindCreditCardByID(int id)
{
    FnTrace("Settings::FindCreditCardByID()");
    for (CreditCardInfo *cc = creditcard_list.Head(); cc != NULL; cc = cc->next)
    {
        if (cc->id == id)
            return cc;
    }
    return NULL;
}

MealInfo *Settings::FindMealByRecord(int record)
{
    FnTrace("Settings::FindMealByRecord()");
    int idx = 0;
    MealInfo *meal = meal_list.Head();
    MealInfo *retmeal = NULL;
    while ((meal != NULL) && (retmeal == NULL))
    {
        if (meal->active)
        {
            if (idx == record)
                retmeal = meal;
            else
                idx += 1;
        }
        meal = meal->next;
    }
    return retmeal;
}

MealInfo *Settings::FindMealByID(int id)
{
    FnTrace("Settings::FindMealByID()");
    for (MealInfo *mi = meal_list.Head(); mi != NULL; mi = mi->next)
    {
        if (mi->id == id)
            return mi;
    }
    return NULL;
}

TermInfo *Settings::FindServer(const genericChar* displaystr)
{
    FnTrace("Settings::FindServer()");
    TermInfo *retti = NULL;
    TermInfo *ti = term_list.Head();

    while (ti != NULL)
    {
        if (ti->IsServer() || (strcmp(displaystr, ti->display_host.Value()) == 0))
        {
            retti = ti;
            break;  // exit the loop
        }
        ti = ti->next;
    }

    if (retti == NULL)
    {
        retti = new TermInfo;
        retti->name.Set("Server");
        retti->display_host.Clear();
        retti->type = TERMINAL_NORMAL;
        retti->printer_model = 0;
        retti->printer_port = 0;
        retti->IsServer(1);
        AddFront(retti);
    }
    return retti;
}

TermInfo *Settings::FindTerminal(const char* displaystr)
{
    FnTrace("Settings:FindTermEntry()");
    TermInfo *retti = NULL;
    TermInfo *ti = term_list.Head();

    while (ti != NULL && retti == NULL)
    {
        if (strcmp(displaystr, ti->display_host.Value()) == 0)
            retti = ti;
        else
            ti = ti->next;
    }

    return retti;
}

TermInfo *Settings::FindTermByRecord(int record)
{
    FnTrace("Settings::FindTermByRecord()");
    return term_list.Index(record);
}

PrinterInfo *Settings::FindPrinterByRecord(int record)
{
    FnTrace("Settings::FindPrinterByRecord()");
    return printer_list.Index(record);
}

PrinterInfo *Settings::FindPrinterByType(int type)
{
    FnTrace("Settings::FindPrinterByType()");
    for (PrinterInfo *pi = printer_list.Head(); pi != NULL; pi = pi->next)
    {
        if (pi->type == type)
            return pi;
    }
    return NULL;
}

int Settings::GetDrawerFloatValue()
{
    FnTrace("Settings::GetDrawerFloatValue()");
    int floatval = 0;
    TimeInfo now;
    now.Set();
    int nowint = (now.Hour() * 60) + now.Min();

    if ((nowint >= drawer_day_start) &&
        (nowint < drawer_night_start))
    {
        floatval = drawer_day_float;
    }
    else
    {
        floatval = drawer_night_float;
    }
    return floatval;
}
