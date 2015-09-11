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
 * sales.cc - revision 133 (9/8/98)
 * Implementation of sale item classes
 */

#include "sales.hh"
#include "check.hh"
#include "terminal.hh"
#include "report.hh"
#include "zone.hh"
#include "data_file.hh"
#include "settings.hh"
#include "labels.hh"
#include "manager.hh"
#include "admission.hh"

#include <cctype>
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Module Data ****/
const char* SalesGroupName[] = {
    "Unused", "Food", "Beverage", "Beer", "Wine", "Alcohol",
    "Merchandise", "Room", NULL};
const char* SalesGroupShortName[] = {
    "", "Food", "Bev", "Beer", "Wine", "Alcohol", "Merchan", "Room", NULL};
int SalesGroupValue[] = {
    SALESGROUP_NONE, SALESGROUP_FOOD, SALESGROUP_BEVERAGE,
    SALESGROUP_BEER, SALESGROUP_WINE, SALESGROUP_ALCOHOL,
    SALESGROUP_MERCHANDISE, SALESGROUP_ROOM, -1};


/**** Ingredient Class ****/
// Constructor
Component::Component()
{
    next = NULL;
    fore = NULL;
    item_id = 0;
}

/**** SalesItem Class ****/
// Constructor
SalesItem::SalesItem(const char* name)
{
    FnTrace("SalesItem::SalesItem()");
    if (name)
        item_name.Set(name);

    next           = NULL;
    fore           = NULL;
    id             = 0;
    item_code.Set("");
    
    location = "";
    event_time = "January 1, 2015";
    total_tickets = "100";
    available_tickets = "100";
    price_label="Adult";
    cost           = 0;
    sub_cost       = 0;
    employee_cost  = 0;
    tax_id         = 0;
    takeout_cost   = 0;
    delivery_cost  = 0;
    takeout_tax_id = 0;
    type           = ITEM_NORMAL;
    call_order     = 0;
    printer_id     = PRINTER_DEFAULT;
    family         = 0;
    item_class     = 0;
    sales_type     = 0;
    period         = 0;
    quanity        = 0;
    has_zone       = 0;
    stocked        = 0;
    prepare_time   = 0;
    price_type     = PRICE_PERITEM;
    changed = 0;
    allow_increase = 1;
}

// Member Functions

int SalesItem::Copy(SalesItem *target)
{
    FnTrace("SalesItem::Copy()");
    int retval = 1;

    if (target != NULL)
    {
        target->item_name.Set(item_name);
        target->zone_name.Set(zone_name);
        target->print_name.Set(print_name);
        target->call_center_name.Set(call_center_name);
        target->id = id;
        target->item_code.Set(item_code);
    target->location.Set(location);
        target->event_time.Set(event_time);
        target->total_tickets.Set(total_tickets);
        target->available_tickets.Set(available_tickets);
    target->price_label.Set(price_label);
        target->cost = cost;
        target->sub_cost = sub_cost;
        target->employee_cost = employee_cost;
        target->takeout_cost = takeout_cost;
        target->delivery_cost = delivery_cost;
        target->tax_id = tax_id;
        target->takeout_tax_id = takeout_tax_id;
        target->type = type;
        target->call_order = call_order;
        target->printer_id = printer_id;
        target->family = family;
        target->item_class = item_class;
        target->sales_type = sales_type;
        target->stocked = stocked;
        target->has_zone = has_zone;
        target->period = period;
        target->prepare_time = prepare_time;
        target->quanity = quanity;
        target->changed = changed;
        target->price_type = price_type;
        target->allow_increase = allow_increase;
        retval = 0;
    }
    return retval;
}

int SalesItem::Add(Component *in)
{
    FnTrace("SalesItem::Add()");
    return component_list.AddToTail(in);
}

int SalesItem::Remove(Component *in)
{
    FnTrace("SalesItem::Remove()");
    return component_list.Remove(in);
}

int SalesItem::Purge()
{
    FnTrace("SalesItem::Purge()");
    component_list.Purge();
    return 0;
}

int SalesItem::Read(InputDataFile &df, int version)
{
    FnTrace("SalesItem::Read()");
    // VERSION NOTES
    // 8  (11/19/96) earliest supported version
    // 9  (12/2/97)  zone_name, takeout_cost, tax_id, takout_tax_id added
    //               prepare time & component list added
    // 10 (02/19/04) added employee_cost
    // 11 (11/24/04) added allow_increase
    // 12 (08/18/05) added call_center_name and delivery_cost
    // 13 (09/14/05) added item_code
    // 14 (04/30/15) added all properties relating to cinema mode.

    if (version < 8)
        return 1;

    df.Read(id);
    df.Read(item_name);
    if (version >= 9)
        df.Read(zone_name);
    df.Read(print_name);
    df.Read(type);
    if (version >= 14)
    {
    df.Read(location);
    df.Read(event_time);
    df.Read(total_tickets);
    df.Read(available_tickets);
    df.Read(price_label);
    }
    df.Read(cost);
    df.Read(sub_cost);
    if (version >= 10)
        df.Read(employee_cost);
    else
        employee_cost = cost;
    if (version >= 9)
    {
        df.Read(takeout_cost);
        df.Read(tax_id);
        df.Read(takeout_tax_id);
    }
    df.Read(call_order);
    df.Read(printer_id);
    int fam;
    df.Read(fam);
    if (fam == 999)
        fam = FAMILY_UNKNOWN;
    family = fam;
    df.Read(item_class);
    df.Read(sales_type);
    df.Read(period);
    df.Read(stocked);
    if (version >= 9)
    {
        int tmp;
        df.Read(tmp); // component count - should be zero for now
        df.Read(prepare_time);
    }
    if (version >= 11)
        df.Read(allow_increase);
    if (version >= 12)
    {
        df.Read(call_center_name);
        df.Read(delivery_cost);
    }
    if (version >= 13)
        df.Read(item_code);

    // Item property checks
    if (call_order < 0)
        call_order = 0;
    if (call_order > 4)
        call_order = 4;
    return 0;
}

int SalesItem::Write(OutputDataFile &df, int version)
{
    FnTrace("SalesItem::Write()");
    if (StringCompare(item_name.Value(), zone_name.Value()) == 0)
        zone_name.Clear();
    if (StringCompare(item_name.Value(), print_name.Value()) == 0)
        print_name.Clear();

    // Write out version 8-10
    int error = 0;
    error += df.Write(id);
    error += df.Write(item_name);

    if (version >= 9)
        error += df.Write(zone_name);

    error += df.Write(print_name);
    error += df.Write(type);
    
    if (version >= 14)
    {
    error += df.Write(location);
    error += df.Write(event_time);
    error += df.Write(total_tickets);
    error += df.Write(available_tickets);
    error += df.Write(price_label);
    }
    error += df.Write(cost);
    error += df.Write(sub_cost);
    if (version >= 10)
        error += df.Write(employee_cost);

    if (version >= 9)
    {
        error += df.Write(takeout_cost);
        error += df.Write(tax_id);
        error += df.Write(takeout_tax_id);
    }

    error += df.Write(call_order);
    error += df.Write(printer_id);
    error += df.Write(family);
    error += df.Write(item_class);
    error += df.Write(sales_type);
    error += df.Write(period);
    error += df.Write(stocked);
    if (version >= 9)
    {
        error += df.Write(0);  // component count - zero for now
        // this newline should NOT be here, but at the end of the record.
        //   (after the LAST field is written)
        error += df.Write(prepare_time, 1);
    }
    error += df.Write(allow_increase);
    error += df.Write(call_center_name);
    error += df.Write(delivery_cost);
    error += df.Write(item_code);

    return error;
}

const char* SalesItem::Family(Terminal *t)
{
    FnTrace("SalesItem::Family()");
    const char* s = FindStringByValue(family, FamilyValue, FamilyName, UnknownStr);
    return t->Translate(s);
}

const char* SalesItem::Printer(Terminal *t)
{
    FnTrace("SalesItem::Printer()");
    const char* s = FindStringByValue(printer_id, PrinterIDValue, PrinterIDName, UnknownStr);
    return t->Translate(s);
}

int SalesItem::Price(Settings *s, int qualifier)
{
    FnTrace("SalesItem::Price()");
    if (qualifier & QUALIFIER_NO)
        return 0;

    int c = cost;
    if (type == ITEM_SUBSTITUTE && (qualifier & QUALIFIER_SUB))
        c = sub_cost;

    if (qualifier & QUALIFIER_DOUBLE)
    {
        c *= s->double_mult;
        c += s->double_add;
    }

    if (c < 0)
        c = 0;
    return c;
}

const char* SalesItem::ZoneName()
{
    FnTrace("SalesItem::ZoneName()");
    if (zone_name.length > 0)
        return admission_filteredname(zone_name);
    else
    return admission_filteredname(item_name);
}

const char* SalesItem::PrintName()
{
    FnTrace("SalesItem::PrintName()");
    if (print_name.length > 0)
        return admission_filteredname(print_name);
    else
        return admission_filteredname(item_name);
}

const char* SalesItem::CallCenterName(Terminal *t)
{
    FnTrace("SalesItem::CallCenterName()");
    if (call_center_name.length > 0)
        return admission_filteredname(call_center_name);
    else
        return admission_filteredname(item_name);
}


/**** GroupItem Class ****/
// Constructor
GroupItem::GroupItem()
{
    next = NULL;
    fore = NULL;
}

// Member Functions
int GroupItem::Read(InputDataFile &df, int version)
{
    return 1;
}

int GroupItem::Write(OutputDataFile &df, int version)
{
    return 1;
}

/**** ItemDB Class ****/
// Constructor
ItemDB::ItemDB()
{
    last_id           = 0;
    changed           = 0;
    name_array        = NULL;
    array_size        = 0;
    merchandise_count = 0;
    merchandise_sales = 0;
    other_count       = 0;
    other_sales       = 0;
}

// Member Functions
int ItemDB::Load(const char* file)
{
    FnTrace("ItemDB::Load()");
    if (file)
        filename.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(filename.Value(), version))
        return 1;

    char str[256];
    if (version < 8 || version > SALES_ITEM_VERSION)
    {
        sprintf(str, "Unknown ItemDB version %d", version);
        ReportError(str);
        return 1;
    }

    int items = 0;
    df.Read(items);

    for (int i = 0; i < items; ++i)
    {
        SalesItem *si = new SalesItem;
        si->Read(df, version);
        Add(si);
    }
    return 0;
}

int ItemDB::Save()
{
    FnTrace("ItemDB::Save()");
    if (filename.length <= 0)
        return 1;

    BackupFile(filename.Value());

    OutputDataFile df;
    if (df.Open(filename.Value(), SALES_ITEM_VERSION))
        return 1;

    int error = 0;
    error += df.Write(ItemCount());

    for (SalesItem *si = ItemList(); si != NULL; si = si->next)
    {
        error += si->Write(df, SALES_ITEM_VERSION);
        si->changed = 0;
    }
    changed = 0;
    return error;
}

int ItemDB::Add(SalesItem *si)
{
    FnTrace("ItemDB::Add()");
    if (si == NULL)
        return 1;

    if (name_array != NULL)
    {
        delete [] name_array;
        name_array = NULL;
        array_size = 0;
    }

    // set item ID if it has none
    if (si->id <= 0)
    {
        changed = 1;
        si->id = ++last_id;
    }
    else if (si->id > last_id)
        last_id = si->id + 1;

    // start at end of list and work backwords
    const char* n = si->item_name.Value();
    SalesItem *ptr = ItemListEnd();
    while (ptr && StringCompare(n, ptr->item_name.Value()) < 0)
        ptr = ptr->fore;

    // Insert si after ptr
    return item_list.AddAfterNode(ptr, si);
}

int ItemDB::Remove(SalesItem *si)
{
    FnTrace("ItemDB::Remove()");
    if (si == NULL)
        return 1;

    if (name_array != NULL)
    {
        delete [] name_array;
        name_array = NULL;
        array_size = 0;
    }
    return item_list.Remove(si);
}

int ItemDB::Purge()
{
    FnTrace("ItemDB::Purge()");
    item_list.Purge();
    group_list.Purge();

    if (name_array != NULL)
    {
        delete [] name_array;
        name_array = NULL;
        array_size = 0;
    }
    return 0;
}

int ItemDB::ResetAdmissionItems()
{
    for(SalesItem* si=ItemList();si!=NULL;si=si->next)
    {
        if(si->type == ITEM_ADMISSION)
        {
            si->available_tickets.Set(si->total_tickets.IntValue());
        }
    }
    return 0;
}

SalesItem *ItemDB::FindByName(const char* name)
{
    FnTrace("ItemDB::FindByName()");
    if (name == NULL)
        return NULL;

    if (name_array == NULL)
        BuildNameArray();

    // use binary search to find item
    int l = 0;
    int r = array_size - 1;
    while (r >= l)
    {
        int x = (l + r) / 2;
        SalesItem *mi = name_array[x];

        int val = StringCompare(name, mi->item_name.Value());
        if (val < 0)
            r = x - 1;
        else if (val > 0)
            l = x + 1;
        else
            return mi;
    }
    return NULL;
}

SalesItem *ItemDB::FindByID(int id)
{
    FnTrace("ItemDB::FindByID()");
    if (id <= 0)
        return NULL;

    for (SalesItem *si = ItemList(); si != NULL; si = si->next)
        if (si->id == id)
            return si;
    return NULL;
}

SalesItem *ItemDB::FindByRecord(int record)
{
    FnTrace("ItemDB::FindByRecord()");
    if (record < 0)
        return NULL;
    if (name_array == NULL)
        BuildNameArray();
    if (record >= array_size)
        return NULL;
    return name_array[record];
}

SalesItem *ItemDB::FindByWord(const char* word, int &record)
{
    FnTrace("ItemDB::FindByWord()");
    if (name_array == NULL)
        BuildNameArray();

    int len = strlen(word);
    SalesItem *si;
    for (int i = 0; i < array_size; ++i)
    {
        si = name_array[i];
        if (si->item_name.length > 0 &&
            StringCompare(si->item_name.Value(), word, len) == 0)
        {
            record = i;
            return si;
        }
    }
    record = 0;
    return NULL;
}

SalesItem *ItemDB::FindByCallCenterName(const char* word, int &record)
{
    FnTrace("ItemDB::FindByCallCenterName()");
    if (name_array == NULL)
        BuildNameArray();
    SalesItem *retval = NULL;
    SalesItem *si = NULL;
    int len = strlen(word);
    int idx = 0;

    for (idx = 0; idx < array_size; idx += 1)
    {
        si = name_array[idx];
        if (si->item_name.length > 0 &&
            StringCompare(si->item_name.Value(), word, len) == 0)
        {
            record = idx;
            retval = si;
            idx = array_size;  // exit the for loop
        }
    }

    if (retval == NULL)
        record = 0;

    return retval;
}

SalesItem *ItemDB::FindByItemCode(const char* code, int &record)
{
    FnTrace("ItemDB::FindByItemCode()");
    if (name_array == NULL)
        BuildNameArray();
    SalesItem *retval = NULL;
    SalesItem *si = NULL;
    int idx = 0;

    for (idx = 0; idx < array_size; idx += 1)
    {
        si = name_array[idx];
        if (strcmp(si->item_code.Value(), code) == 0)
        {
            record = idx;
            retval = si;
            idx = array_size;
        }
    }

    return retval;
}

int ItemDB::BuildNameArray()
{
    FnTrace("ItemDB::BuildNameArray()");
    if (name_array != NULL)
    {
    free(name_array);
        name_array = NULL;
        array_size = 0;
    }

    // Build search array
    array_size = ItemCount();
    name_array = (SalesItem **)calloc(sizeof(SalesItem *), (array_size + 1));
    if (name_array == NULL)
    {
        array_size = 0;
        return 1;
    }

    int i = 0;
    for (SalesItem *si = ItemList(); si != NULL; si = si->next)
        name_array[i++] = si;

    return 0;
}

int ItemDB::DeleteUnusedItems(ZoneDB *zone_db)
{
    FnTrace("ItemDB::DeleteUnusedItems()");
    if (zone_db == NULL)
        return 1;

    // crossreference items with touchzones
    for (Page *p = zone_db->PageList(); p != NULL; p = p->next)
    {
        for (Zone *z = p->ZoneList(); z != NULL; z = z->next)
        {
            SalesItem *si = z->Item(this);
            if (si)
                ++si->has_zone;
        }
    }

    // delete items not used
    SalesItem *si = ItemList();
    while (si)
    {
        SalesItem *ptr = si->next;
        if (si->has_zone <= 0)
        {
            Remove(si);
            delete si;
        }
        else
            si->has_zone = 0;
        si = ptr;
    }
    return 0;
}

int ItemDB::ItemsInFamily(int family)
{
    FnTrace("ItemDB::ItemsInFamily()");
    int count = 0;
    SalesItem *item = ItemList();
    
    while (item != NULL)
    {
        if (item->family == family)
            count += 1;
        item = item->next;
    }

    return count;
}


/**** Functions ****/
int MergeQualifier(int &flag, int qualifier)
{
    FnTrace("MergeQualifier()");
    if ((qualifier & QUALIFIER_NO))
        flag = QUALIFIER_NO;
    else
    {
        int side = (flag & QUALIFIER_SIDE);
        int sub  = (flag & QUALIFIER_SUB);

        switch (qualifier)
        {
        case QUALIFIER_LITE:      flag = QUALIFIER_LITE;      break;
        case QUALIFIER_EXTRA:     flag = QUALIFIER_EXTRA;     break;
        case QUALIFIER_DOUBLE:    flag = QUALIFIER_DOUBLE;    break;
        case QUALIFIER_DRY:       flag = QUALIFIER_DRY;       break;
        case QUALIFIER_UNTOASTED: flag = QUALIFIER_UNTOASTED; break;
        case QUALIFIER_CRISPY:    flag = QUALIFIER_CRISPY;    break;
        case QUALIFIER_SOFT:      flag = QUALIFIER_SOFT;      break;
        case QUALIFIER_HARD:      flag = QUALIFIER_HARD;      break;
        case QUALIFIER_PLAIN:     flag = QUALIFIER_PLAIN;     break;
        case QUALIFIER_GRILLED:   flag = QUALIFIER_GRILLED;   break;
        case QUALIFIER_TOASTED:   flag = QUALIFIER_TOASTED;   break;
        case QUALIFIER_SIDE:
            if (flag != QUALIFIER_NO)
                flag |= QUALIFIER_SIDE;
            break;
        case QUALIFIER_SUB:
            if (flag != QUALIFIER_NO)
                flag |= QUALIFIER_SUB;
            break;
        case QUALIFIER_LEFT:     flag = QUALIFIER_LEFT;      break;
        case QUALIFIER_RIGHT:    flag = QUALIFIER_RIGHT;     break;
        case QUALIFIER_WHOLE:    flag = QUALIFIER_WHOLE;     break;
        }

        if (side)
            flag |= QUALIFIER_SIDE;
        if (sub)
            flag |= QUALIFIER_SUB;
    }

    return 0;
}

int PrintItem(char* buffer, int qualifier, const char* item)
{
    FnTrace("PrintItem()");
    char pre[64]  = "";
    char post[64] = "";
    if ((qualifier & QUALIFIER_SIDE))
        sprintf(post, " (on side)");

    if      ((qualifier & QUALIFIER_NO))        sprintf(pre, "NO ");
    else if ((qualifier & QUALIFIER_LITE))      sprintf(pre, "Lite ");
    else if ((qualifier & QUALIFIER_EXTRA))     sprintf(pre, "Extra ");
    else if ((qualifier & QUALIFIER_DOUBLE))    sprintf(pre, "Double ");
    else if ((qualifier & QUALIFIER_DRY))       sprintf(pre, "Dry ");
    else if ((qualifier & QUALIFIER_UNTOASTED)) sprintf(pre, "Untoasted ");
    else if ((qualifier & QUALIFIER_CRISPY))    sprintf(pre, "Crispy ");
    else if ((qualifier & QUALIFIER_SOFT))      sprintf(pre, "Soft ");
    else if ((qualifier & QUALIFIER_HARD))      sprintf(pre, "Hard ");
    else if ((qualifier & QUALIFIER_PLAIN))     sprintf(pre, "Plain ");
    else if ((qualifier & QUALIFIER_TOASTED))   sprintf(pre, "Toasted ");
    else if ((qualifier & QUALIFIER_GRILLED))   sprintf(pre, "Grilled ");
    else if ((qualifier & QUALIFIER_LEFT))      sprintf(pre, "LEFT: ");
    else if ((qualifier & QUALIFIER_RIGHT))     sprintf(pre, "RIGHT: ");
    else if ((qualifier & QUALIFIER_WHOLE))     sprintf(pre, "WHOLE: ");

    if ((qualifier & QUALIFIER_SUB))
        sprintf(buffer, "SUB: %s%s%s", pre, item, post);
    else
        sprintf(buffer, "%s%s%s", pre, item, post);
    return 0;
}

char* FilterName(char* name, char* str)
{
    FnTrace("FilterName()");
    static char buffer[1024];
    if (str == NULL)
        str = buffer;

    int space = 1;
    char* s = name, *d = str;
    while (*s)
    {
        if (*s == '\\' || isspace(*s))
        {
            if (space == 0)
            {
                space = 1;
                *d++ = ' ';
            }
        }
        else if (isprint(*s))
        {
            *d++ = *s;
            space = 0;
        }
        ++s;
    }
    if (space && s > name)
        --d;
    *d = '\0';
    return str;
}
