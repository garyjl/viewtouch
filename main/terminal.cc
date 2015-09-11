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
 * terminal.cc - revision 298 (10/13/98)
 * Implementation of base terminal class
 */

#include "archive.hh"
#include "check.hh"
#include "credit.hh"
#include "customer.hh"
#include "data_file.hh"
#include "debug.hh"
#include "dialog_zone.hh"
#include "drawer.hh"
#include "drawer_zone.hh"
#include "employee.hh"
#include "image_data.hh"
#include "inventory.hh"
#include "labels.hh"
#include "labor.hh"
#include "locale.hh"
#include "license_hash.hh"
#include "manager.hh"
#include "printer.hh"
#include "remote_link.hh"
#include "report.hh"
#include "sales.hh"
#include "settings.hh"
#include "system.hh"
#include "terminal.hh"
#ifdef USE_TOUCHSCREEN
#include "touch_screen.hh"
#endif
#include "utility.hh"
#include "zone.hh"

#include <ctype.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include <X11/Intrinsic.h>

#include <string>
#include <map>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Definitions ****/
// System windows
enum windows {
    WIN_MAIN = 1,
    WIN_TOOLBAR,
    WIN_PAGELIST,
    WIN_ZONEEDIT,
    WIN_MULTIZONEEDIT,
    WIN_PAGEEDIT
};

// Window Buttons
enum window_buttons {
    WB_NEWZONE = 1,
    WB_NEWPAGE,
    WB_ALL,
    WB_TOGGLE,
    WB_COPY,
    WB_MOVE,
    WB_INFO,
    WB_LIST,
    WB_PRIOR,
    WB_NEXT,
    WB_ICONIFY,
    WB_PRINTLIST
};

// Other
#define MODIFY_MOVE      1
#define MODIFY_RESIZE_TE 2   // Top Edge
#define MODIFY_RESIZE_BE 4   // Bottom Edge
#define MODIFY_RESIZE_LE 8   // Left Edge
#define MODIFY_RESIZE_RE 16  // Right Edge
#define GRAB_EDGE        16  // number of pixels for move/resize edge

#define SOCKET_FILE "/tmp/vt_term"

/**** Calback Functions ****/
void TermCB(XtPointer client_data, int *fid, XtInputId *id)
{

    FnTrace("TermCB()");
    Terminal *term = (Terminal *) client_data;
    Terminal *errterm = NULL;
    int val = term->buffer_in->Read(*fid);
    static int last_code = 0;

    if (val <= 0)
    {
        // If *fid doesn't equal term->socket_no, then we have a clone.
        // In that case, we need to find the clone so that we can process
        // it properly rather than the primary.
        if (*fid != term->socket_no)
        {
            Terminal *currterm = term->CloneList();
            while (currterm != NULL && errterm == NULL)
            {
                if (*fid == currterm->socket_no)
                    errterm = currterm;
                else
                    currterm = currterm->next;
            }
        }
        else
            errterm = term;

        // Upgrade the failure count and return unless we've hit the threshold.
        ++errterm->failure;
        if (errterm->failure < 8)
            return;

        // And now get rid of the terminal.
        Control *db = term->parent;
        if (errterm->socket_no > 0)
        {
            // close socket here instead of letting the destructor do it
            // (destructor tries to send kill message before closing)
            close(errterm->socket_no);
            errterm->socket_no = 0;
        }

        if (errterm != term)
        {
            term->RemoveClone(errterm);
            delete(errterm);
        }
        else if (db)
        {
            Printer *p = db->FindPrinter(term->printer_host.Value(),
                                         term->printer_port);
            db->KillPrinter(p, 1);
            db->KillTerm(term);
        }
        else
        {
            term->kill_me = 1; // best that can be done without parent pointer
        }

        return;
    }

    Settings *settings = term->GetSettings();
    term->failure = 0;
    genericChar str[STRLENGTH];

    while (term->buffer_in->size > 0)
    {
        int code = term->RInt8();
        term->buffer_in->SetCode("vt_main", code);
        switch (code)
        {
        case SERVER_TERMINFO:
            term->size   = term->RInt8();
            term->width  = term->RInt16();
            term->height = term->RInt16();
            term->depth  = term->RInt16();

            // Send initial settings
            term->WInt8(TERM_BLANKTIME);
            if (*fid == term->socket_no &&
                term->type != TERMINAL_KITCHEN_VIDEO &&
                term->type != TERMINAL_KITCHEN_VIDEO2)
            {
                term->WInt16(settings->screen_blank_time);
                term->allow_blanking = 1;
            }
            else
            {
                term->WInt16(0);
                term->allow_blanking = 0;
            }
            term->WInt8(TERM_STORENAME);
            term->WStr(settings->store_name.Value());

            if (term->zone_db == NULL)
                printf("ACK!!!! no zone_db\n");

        // for KDS terminals, default to kitchen page
            else if (term->type == TERMINAL_KITCHEN_VIDEO || term->type == TERMINAL_KITCHEN_VIDEO2)
            term->page = term->zone_db->FindByTerminal(term->type, -1, term->size);

            if (term->page)
                term->Jump(JUMP_STEALTH, term->page->id);  // Get new best size for page
            else
            {
                term->Jump(JUMP_STEALTH, PAGEID_LOGIN); // Show Login Page
                term->UpdateAllTerms(UPDATE_TERMINALS, NULL);
            }
            break;

        case SERVER_ERROR:
            sprintf(str, "TermError: %s", term->RStr());
            ReportError(str);
            break;

        case SERVER_TOUCH:
            term->time_out   = SystemTime;
            term->last_input = SystemTime;
            {
                int my_id = term->RInt16();
                int x = term->RInt16();
                int y = term->RInt16();
                if (my_id == WIN_MAIN)
                {
                    if (term->record_activity)
                        term->RecordTouch(x, y);
                    term->Touch(x, y);
                }
            }
            break;

        case SERVER_KEY:
        {
            term->RInt16(); // win id - ignored
            genericChar key = (genericChar) term->RInt16();
            int my_code = term->RInt32();
            int state = term->RInt32();  // shift, ctrl, alt, etc.
            if (term->record_activity)
                term->RecordKey(key, my_code, state);
            term->KeyboardInput(key, my_code, state);
        }
        break;

        case SERVER_MOUSE:
        {
            int my_id = term->RInt16();
            int my_code = term->RInt8();
            int x = term->RInt16();
            int y = term->RInt16();
            if (my_id == WIN_MAIN)
            {
                if (term->record_activity && (my_code & MOUSE_PRESS))
                    term->RecordMouse(my_code, x, y);
                term->MouseInput(my_code, x, y);
            }
            else if (my_id == WIN_TOOLBAR)
            {
                term->MouseToolbar(my_code, x, y);
            }
        }
        break;

        case SERVER_ZONEDATA:
            term->ReadZone(); break;

        case SERVER_ZONECHANGES:
            term->ReadMultiZone(); break;

        case SERVER_PAGEDATA:
            term->ReadPage(); break;

        case SERVER_KILLZONE:
            term->KillZone(); break;

        case SERVER_KILLPAGE:
            term->KillPage(); break;

        case SERVER_TRANSLATE:
        {
            int no = term->RInt8(); // translation count
            const genericChar* s1;
            const genericChar* s2;
            for (int i = 0; i < no; ++i)
            {
                s1 = term->RStr(str);
                s2 = term->RStr();
                MasterLocale->NewTranslation(s1, s2);
            }

            if (term->edit_zone)
            {
                term->edit_zone->Draw(term, 0);
                term->edit_zone = NULL;
            }
            else if (term->edit_page)
            {
                term->Draw(0);
                term->edit_page = NULL;
            }
        }
        break;
        case SERVER_LISTSELECT:
            term->JumpList(term->RInt32());
            break;
        case SERVER_SWIPE:
        {
            const char* s1 = term->RStr();
            if (strlen(s1) < STRLENGTH)
            {
                sprintf(str, "swipe %s", s1);
                term->Signal(str, 0);
            }
        }
        break;
        case SERVER_BUTTONPRESS:
            term->RInt16(); // layer id
            term->ButtonCommand(term->RInt16());
            break;
        case SERVER_SHUTDOWN:  // only allow easy exits on debug platforms
            if (term->user != NULL && (term->user->id == 1 || term->user->id == 2))
                EndSystem();  // superuser and developer can end system
            else if (debug_mode)
                EndSystem();  // anyone in debug mode can end system
            break;
        case SERVER_CC_PROCESSED:
            term->ReadCreditCard();
            if (term->admin_forcing == 3)
                term->Signal("adminforceauth4", 0);
            else
                term->Signal("ccprocessed", 0);
            break;
        case SERVER_CC_SETTLED:
            term->CC_GetSettlementResults();
            term->eod_failed = 0;
            break;
        case SERVER_CC_INIT:
            term->CC_GetInitResults();
            break;
        case SERVER_CC_TOTALS:
            term->CC_GetTotalsResults();
            break;
        case SERVER_CC_DETAILS:
            term->CC_GetDetailsResults();
            break;
        case SERVER_CC_SAFCLEARED:
            term->CC_GetSAFClearedResults();
            break;
        case SERVER_CC_SAFDETAILS:
            term->CC_GetSAFDetails();
            break;
        case SERVER_CC_SETTLEFAILED:
            term->cc_processing = 0;
            term->eod_failed = 1;
            if (term->GetSettings()->authorize_method == CCAUTH_MAINSTREET)
            {
                term->CC_Settle(NULL, 1);
                char errormsg[STRLENGTH];
                term->RStr(errormsg);
                term->system_data->cc_settle_results->Add(term, errormsg);
                term->Signal("ccsettledone", 0);
            }
            break;
        case SERVER_CC_SAFCLEARFAILED:
            term->cc_processing = 0;
            term->eod_failed = 1;
            break;
        default:
            snprintf(str, STRLENGTH, "Cannot process unknown code:  %d", code);
            ReportError(str);
            snprintf(str, STRLENGTH, "  Last code processed was %d", last_code);
            ReportError(str);
            printf("Terminating due to unforseen error....\n");
            EndSystem();
            break;
        } //end switch
        last_code = code;
    } //end while
}

void RedrawZoneCB(XtPointer client_data, XtIntervalId *timer_id)
{
    FnTrace("RedrawZoneCB()");
    Terminal *t = (Terminal *) client_data;
    t->redraw_id = 0;

    Zone *z = t->selected_zone;
    if (z)
    {
        t->selected_zone = NULL;
        z->Draw(t, 0);
    }
}

/**** Terminal Class ****/
// Constructor
Terminal::Terminal()
{
    next            = NULL;
    fore            = NULL;
    parent          = NULL;
    check           = NULL;
    customer        = NULL;
    seat            = 0;
    password_given  = 0;
    password_jump   = 0;
    drawer_count    = 0;
    kitchen         = 0;
    move_check      = 0;
    type            = TERMINAL_NORMAL;
    original_type   = type;
    sortorder       = CHECK_ORDER_NEWOLD;
    qualifier       = QUALIFIER_NONE;
    guests          = 0;
    archive         = NULL;
    order           = NULL;
    stock           = NULL;
    last_index      = INDEX_GENERAL;
    job_filter      = 0;
    printer_port    = 0;
    print_workorder = 1;
    cdu             = NULL;
    server          = NULL;
    expense_drawer  = NULL;
    record_activity = 0;
    record_fd       = -1;
    credit          = NULL;
    allow_blanking  = 1;

    //initialized through pointer in Control::Add() (in file manager.cc)
    system_data     = NULL; 

    buffer_in       = NULL;
    buffer_out      = NULL;

    // General Inits
    size      = 0;
    width     = 0;
    height    = 0;
    depth     = 0;
    grid_x    = GRID_X;
    grid_y    = GRID_Y;
    socket_no = 0;
    input_id  = 0;
    redraw_id = 0;
    message_set = 0;
    select_on = 0;
    select_x1 = 0;
    select_y1 = 0;
    select_x2 = 0;
    select_y2 = 0;
    last_x    = 0;
    last_y    = 0;
    zone_modify = 0;
    edit_page = NULL;
    edit_zone = NULL;
    failure = 0;
    last_page_type = -1;
    last_page_size = -1;
    is_bar_tab     = 0;
    force_jump     = 0;
    force_jump_source = 0;

    zone_db        = NULL;
    page           = NULL;
    org_page_id    = 0;
    user           = NULL;
    dialog         = NULL;
    next_dialog    = NULL;
    selected_zone  = NULL;
    previous_zone  = NULL;
    active_zone    = NULL;
    timeout        = 15;
    reload_zone_db = 0;
    edit           = 0;
    translate      = 0;
    is_server      = 0;
    kill_me        = 0;
    show_info      = 0;
    locale_main    = NULL;
    locale_default = NULL;
    size           = 0;
    time_out.Set();
    ClearPageStack();

    // report flags
    expand_labor    = 0;
    hide_zeros      = 0;
    show_family     = 1;
    expand_goodwill = 0;

    cc_credit_termid.Set("");
    cc_debit_termid.Set("");
    cc_processing  = 0;
    eod_processing = EOD_DONE;
    eod_failed     = 0;

    check_balanced = 0;
    has_payments   = 0;

    pending_subcheck = NULL;
    auth_amount    = 0;
    void_amount    = 0;
    auth_action    = CCAUTH_NOACTION;
    auth_swipe     = 0;
    auth_message   = NULL;
    auth_message2  = NULL;
    admin_forcing  = 0;

    curr_font_id   = -1;
    curr_font_width = -1;
}

// Destructor
Terminal::~Terminal()
{
    FnTrace("Terminal::~Terminal()");

    Terminal *currterm = clone_list.Head();
    while (currterm != NULL)
    {
        if (currterm->input_id)
            RemoveInputFn(currterm->input_id);
        currterm = currterm->next;
    }

    Drawer *drawer = system_data->DrawerList();
    while (drawer != NULL)
    {
        if (drawer->term == this)
            drawer->term = NULL;
        drawer = drawer->next;
    }

    if (input_id)
        RemoveInputFn(input_id);

    if (redraw_id)
        RemoveTimeOutFn(redraw_id);

    if (socket_no > 0)
    {
        WInt8(TERM_DIE);
        SendNow();
        close(socket_no);
    }

    if (buffer_in)
        delete buffer_in;

    if (buffer_out)
        delete buffer_out;

    if (dialog)
        delete dialog;

    if (zone_db)
        delete zone_db;

    if (cdu)
    {
        cdu->Clear();
        delete cdu;
    }
}

// Member Functions

/****
 * Terminal::TerminalError:  We need a way to show errors when we
 *   can't find the appropriate page.  This function will allow us
 *   to display errors on the black "Please Wait" screen.  When
 *   we have a page but can't find the page we want, we'll use
 *   dialogs.
 ****/
int Terminal::TerminalError(const genericChar* message)
{
    FnTrace("Terminal::TerminalError()");
    fprintf(stderr, "%s",message);  //comprehensive coverage
    fprintf(stderr, "\n");

    if (page)
    {
        SimpleDialog *d = new SimpleDialog(message);
        d->Button("Okay", "okay");
        OpenDialog(d);
    }
    else
    {
        RenderText(message, 0, 0, COLOR_RED, FONT_HELV_34B);
        WInt8(TERM_FLUSH);
        SendNow();  //Force sending the message
    }
    return 0;
}

int Terminal::Initialize()
{
    FnTrace("Terminal::Initialize()");
    int retval = 0;
    Settings *settings = GetSettings();

    SendTranslations(FamilyName);
    SetCCTimeout(settings->cc_connect_timeout);
    SetIconify(settings->allow_iconify);

    return retval;
}

int Terminal::AllowBlanking(int allow)
{
    FnTrace("Terminal::AllowBlanking()");
    int retval = 0;
    static int last_allow = -1;
    int blank_time = 0;
    int settings_blank_time = GetSettings()->screen_blank_time;

    if (allow != last_allow)
    {
        blank_time = (allow && allow_blanking) ? settings_blank_time : 0;
        WInt8(TERM_BLANKTIME);
        WInt16(blank_time);
        SendNow();
        last_allow = allow;
    }

    return retval;
}

/****
 * SendTranslations:  We need to let the dialogs know about translations.  This is
 *  primarily for family names at the moment.
 ****/
int Terminal::SendTranslations(const char* *name_list)
{
    FnTrace("Terminal::SendTranslations()");
    int retval = 0;
    int idx = 0;
    int count = 0;

    for (idx = 0; name_list[idx] != NULL; idx += 1)
        count += 1;

    if (count > 0)
    {
        WInt8(TERM_TRANSLATIONS);
        WInt8(count);
        for (idx = 0; name_list[idx] != NULL; idx += 1)
        {
            WStr(name_list[idx]);
            WStr(MasterLocale->Translate(name_list[idx]));
        }
    }
    
    return retval;
}

int Terminal::Draw(int update_flag)
{
    FnTrace("Terminal::Draw()");
    if (page)
    {
        RenderBlankPage();
        page->Render(this, update_flag);
        UpdateAll();
    }
    return 0;
}

int Terminal::Draw(int update_flag, int x, int y, int w, int h)
{
    FnTrace("Terminal::Draw(x,y,w,h)");
    if (page)
    {
        SetClip(x, y, w, h);
        RenderBackground();
        page->Render(this, update_flag, x, y, w, h);
        UpdateAll();
    }
    return 0;    
}

int Terminal::Jump(int jump_type, int jump_id)
{
    FnTrace("Terminal::Jump()");
    if (zone_db == NULL)
        return 1;

    check_balanced = 0;
    if (check != NULL)
    {
        SubCheck *sc = check->FirstOpenSubCheck();
        if (sc == NULL || sc->balance == 0)
            check_balanced = 1;
    }
    Settings *settings = GetSettings();

    switch (jump_type)
    {
    case JUMP_NONE:
        return 0;
    case JUMP_RETURN:
        /** fall through **/
    case JUMP_SCRIPT: // JUMP_SCRIPT acts like JUMP_RETURN for now
        jump_type = JUMP_STEALTH;
        jump_id   = PopPage();
        break;
    case JUMP_HOME:
        jump_type = JUMP_STEALTH;
        jump_id   = HomePage();
        break;
    case JUMP_INDEX:
        if (page && page->type != PAGE_ITEM &&
            page->type != PAGE_SCRIPTED && 
            page->type != PAGE_SCRIPTED2 &&
            page->type != PAGE_SCRIPTED3)
        {
            return 1;
        }
        else
            return JumpToIndex(last_index);
    case JUMP_PASSWORD:
        if (user == NULL)
            return 1;
        if (user->UsePassword(settings) && (! password_given))
        {
            password_jump = jump_id;
            OpenDialog(new PasswordDialog(user->password.Value()));
            return 0;
        }
        else
            password_given = 1;
        jump_type = JUMP_NORMAL;
        break;
    default:
        break;
    } //end switch()

    if (force_jump)
    {
        if (page != NULL && page->id != force_jump_source)
        {
            jump_type = JUMP_STEALTH;
            jump_id = force_jump;
            force_jump = 0;
            force_jump_source = 0;
        }
    }

    if (jump_id == 0)
    {
        return 1;  // Invalid page id
    }

    Page *targetPage = zone_db->FindByID(jump_id, size);
    if (targetPage == NULL)
    {
        genericChar buffer[STRLENGTH];
        snprintf(buffer, STRLENGTH, "Unable to find jump target (%d, %d) for %s",
                 jump_id, size, name.Value());
        TerminalError(buffer);
        return 1;
    }

    if (jump_id == -1)
    {
        is_bar_tab = 0;
        type = original_type;
    }

    if (page && (jump_id != page->id) && jump_type != JUMP_STEALTH)
        PushPage(page->id);

    return ChangePage(targetPage);
}

int Terminal::JumpToIndex(int idx)
{
    FnTrace("Terminal::JumpToIndex()");
    if (zone_db == NULL)
        return 1;

    Settings *settings = GetSettings();

    // hack for SunWest
    if (settings->store == STORE_SUNWEST)
    {
        if (check == NULL)
            return 1;
        if (check->EntreeCount(seat) <= 0)
            return Jump(JUMP_STEALTH, 200);
        else
            return Jump(JUMP_STEALTH, 206);
    }

    Page *p = zone_db->FindByType(PAGE_INDEX, idx, size);
    if (p == NULL)
    {
        // No matching p type found, so provide meaningfull error message
        // then bail out 

        int cl = CompareList(idx, IndexValue);
        if (cl < 0)
            ReportError("Unknown index - can't jump");
        else
        {
            genericChar str[64];
            sprintf(str, "'%s' Index doesn't exist - can't jump", IndexName[cl]);
            ReportError(str);
        }
        return 1;
    }
    else
        return ChangePage(p);
}

int Terminal::RunScript(const genericChar* script, int jump_type, int jump_id)
{
    FnTrace("Terminal::RunScript()");
    // FIX - parsing script (hack -- should redo)
    Settings *s = GetSettings();
    int j[16];
    int jump_count = 0;
    if (script == NULL)
        jump_count = 0;
    else
        jump_count = sscanf(script, "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
                            &j[ 0], &j[ 1], &j[ 2], &j[ 3], &j[ 4], &j[ 5], &j[ 6], &j[ 7],
                            &j[ 8], &j[ 9], &j[10], &j[11], &j[12], &j[13], &j[14], &j[15]);

    if (jump_count > 0)
    {
        switch (jump_type)
        {
        case JUMP_NONE:
            PushPage(page->id);
            break;
        case JUMP_NORMAL:
        case JUMP_STEALTH:
        case JUMP_PASSWORD:
            PushPage(jump_id);
            break;
        case JUMP_RETURN:
        case JUMP_SCRIPT:
            break;
        case JUMP_HOME:
            PushPage(HomePage());
            break;
        case JUMP_INDEX:
            if (s->store == STORE_SUNWEST)
            {
                // hack for SunWest
                if (check->EntreeCount(seat) <= 0)
                    PushPage(200);
                else
                    PushPage(206);
            }
            else
            {
                Page *p = zone_db->FindByType(PAGE_INDEX, last_index, size);
                if (p)
                    PushPage(p->id);
            }
            break;
        }

        for (int i = jump_count - 1; i >= 0; --i)
            PushPage(j[i]);

        Jump(JUMP_RETURN);
    }
    else
        Jump(jump_type, jump_id);
    return 0;
}

int Terminal::ChangePage(Page *targetPage)
{
    FnTrace("Terminal::ChangePage()");

    if (targetPage == NULL)
    {
        return 1; // Error
    }

    if (targetPage->type == PAGE_INDEX)
        last_index = targetPage->index;

    KillDialog();

    int no_parent_flag = 0;
    if (page &&
        page->IsTable() && 
        targetPage->IsTable() && 
        (page->size == targetPage->size))
    {
        no_parent_flag = 1;
    }
    else
        selected_zone = NULL;

    page = targetPage;

    if (page)
        AllowBlanking(page->IsKitchen() == 0);

    RenderBlankPage();

    page->Render(this, RENDER_NEW, no_parent_flag);

    UpdateAll();

    return 0;
}

int Terminal::PushPage(int my_page_id)
{
    FnTrace("Terminal::PushPage()");
    if (my_page_id == 0)
        return 1;  // not a valid page for the stack

    if (page_stack_size >= PAGE_STACK_SIZE)
    {
        ReportError("ALERT: Page stack size exceeded");
        for (int i = 0; i < (PAGE_STACK_SIZE - 1); ++i)
            page_stack[i] = page_stack[i + 1];
        page_stack_size = PAGE_STACK_SIZE - 1;
    }

    page_stack[page_stack_size] = my_page_id;
    ++page_stack_size;
    return 0;
}

int Terminal::PopPage()
{
    FnTrace("Terminal::PopPage()");
    if (page_stack_size <= 0)
        return PAGEID_LOGIN;
    else
        return page_stack[--page_stack_size];
}

int Terminal::ClearPageStack()
{
    FnTrace("Terminal::ClearPageStack()");
    page_stack_size = 0;
    return 0;
}

int Terminal::NextTablePage()
{
    FnTrace("Terminal::NextTablePage()");
    Page *p = page;
    if (p == NULL || zone_db == NULL)
        return 1;

    for (int l = 0; l < 2; ++l)
    {
        while (p)
        {
            if (p->id > 0 && p->IsTable() && p->id != page->id)
            {
                // set the user's current starting page
                if (user != NULL)
                    user->SetStartingPage(p->id);                

                // and jump us there
                Jump(JUMP_STEALTH, p->id);
                return 0;
            }
            p = p->next;
        }
        p = zone_db->PageList();
    }

    // no table pages - jump to check list page
    p = zone_db->FindByType(PAGE_CHECKS, -1, size);
    if (p)
    {
        Jump(JUMP_STEALTH, p->id);
        return 0;
    }
    return 1;
}

int Terminal::PriorTablePage()
{
    FnTrace("Terminal::PriorTablePage()");
    Page *p = page;
    if (p == NULL || zone_db == NULL)
        return 1;

    for (int l = 0; l < 2; ++l)
    {
        while (p)
        {
            if (p->id > 0 && p->IsTable() && p->id != page->id)
            {
                Jump(JUMP_STEALTH, p->id);
                return 0;
            }
            p = p->fore;
        }
        p = zone_db->PageListEnd();
    }

    // no table pages - jump to check list page
    p = zone_db->FindByType(PAGE_CHECKS, -1, size);
    if (p)
    {
        Jump(JUMP_STEALTH, p->id);
        return 0;
    }
    return 1;
}

int Terminal::FastStartLogin()
{
    FnTrace("Terminal::FastStartLogin()");
    Settings *settings = GetSettings();
    int retval = 0;
    int mealindex = 0;
    int target = 0;

    Drawer *drawer = FindDrawer();
    if (drawer == NULL)
    {
        DialogZone *diag = new SimpleDialog("No drawer available for payments");
        diag->Button("Okay");
        return OpenDialog(diag);
    }

    mealindex = settings->MealPeriod(SystemTime);
    target = IndexValue[mealindex];

    QuickMode(CHECK_FASTFOOD);
    JumpToIndex(target);

    return retval;
}

int Terminal::OpenTab(int phase, const char* message)
{
    FnTrace("Terminal::OpenTab()");
    int retval = 0;

    if (phase == TABOPEN_START)
    {
        QuickMode(CHECK_BAR);
        if (check != NULL && check->customer != NULL)
        {
            OpenTabDialog *otd = new OpenTabDialog(check->customer);
            OpenDialog(otd);
        }
    }
    else if (phase == TABOPEN_AMOUNT)
    {
        is_bar_tab = 1;
        Jump(JUMP_STEALTH, PAGE_ID_TABSETTLE);
    }
    else if (phase == TABOPEN_CARD)
    {
        if (check != NULL)
        {
            SubCheck *sc =check->current_sub;
            if (sc == NULL)
                sc = check->NewSubCheck();
            if (message != NULL)
                auth_amount = atoi(&message[11]);
            else
                auth_amount = 5000;  // $50.00
            auth_action = AUTH_PREAUTH;
            CreditCardDialog *ccd = new CreditCardDialog(this, sc, NULL);
            ccd->ClosingAction(ACTION_SUCCESS, ACTION_JUMPINDEX, INDEX_BAR);
            ccd->ClosingAction(ACTION_CANCEL, ACTION_SIGNAL, "opentabfailed");
            OpenDialog(ccd);
        }
    }
    else if (phase == TABOPEN_CANCEL)
    {
        if (check != NULL && check->IsEmpty())
            delete check;
        check = NULL;
    }

    return retval;
}

int Terminal::ContinueTab(int serial_number)
{
    FnTrace("Terminal::ContinueTab()");
    int retval = 0;
    Check *currcheck = system_data->CheckList();
    
    if (serial_number > 0)
    {
        while (currcheck != NULL)
        {
            if (currcheck->serial_number == serial_number)
            {
                check = currcheck;
                currcheck = NULL;
                is_bar_tab = 1;
                JumpToIndex(INDEX_BAR);
            }
            else
                currcheck = currcheck->next;
        }
    }
    else
        OpenTabList("continuetab2");

    return retval;
}

int Terminal::CloseTab(int serial_number)
{
    FnTrace("Terminal::CloseTab()");
    int retval = 0;
    Check *currcheck = system_data->CheckList();
    
    if (serial_number > 0)
    {
        while (currcheck != NULL)
        {
            if (currcheck->serial_number == serial_number)
            {
                check = currcheck;
                currcheck = NULL;
                Jump(JUMP_STEALTH, PAGE_ID_SETTLEMENT);
            }
            else
                currcheck = currcheck->next;
        }
    }
    else
        OpenTabList("closetab2");

    return retval;
}

int Terminal::OpenTabList(const char* message)
{
    FnTrace("Terminal::OpenTabList()");
    int retval = 0;
    SimpleDialog *sd = NULL;
    Check *currcheck = system_data->CheckList();
    SubCheck *subcheck = NULL;
    Payment *payment = NULL;
    char fname[STRLENGTH];
    char four[STRLENGTH];
    char btitle[STRLENGTH];
    char bmesg[STRLENGTH];
    int count = 0;

    sd = new SimpleDialog(Translate("Select a Bar Tab"), 2);
    while (currcheck != NULL)
    {
        if (currcheck->type == CHECK_BAR &&
            currcheck->customer != NULL &&
            currcheck->HasOpenTab())
        {
            count += 1;
            four[0] = '\0';
            strcpy(fname, currcheck->customer->FirstName());
            subcheck = currcheck->SubList();
            while (subcheck != NULL)
            {
                payment = subcheck->PaymentList();
                while (payment != NULL)
                {
                    if (payment->credit != NULL && payment->credit->IsPreauthed())
                    {
                        payment->credit->LastFour(four);
                        payment = NULL;
                    }
                    else
                        payment = payment->next;
                }
                subcheck = subcheck->next;
            }
            snprintf(btitle, STRLENGTH, "%s\\%s", fname, four);
            snprintf(bmesg, STRLENGTH, "%s %d", message, currcheck->serial_number);
            sd->Button(btitle, bmesg);
        }
        currcheck = currcheck->next;
    }
    if (count)
        sd->Button("Cancel");
    else
    {
        sd->SetTitle("There are no open tabs.");
        sd->Button("Okay");
    }
    OpenDialog(sd);

    return retval;
}

SignalResult Terminal::Signal(const genericChar* message, int group_id)
{
    FnTrace("Terminal::Signal()");
    SimpleDialog *sd = NULL;
    char msg[STRLONG] = "";
    static const char* commands[] = {
        "logout", "next archive", "prior archive", "open drawer",
        "shutdown", "systemrestart", "calibrate", "wagefilterdialog",
        "servernext", "serverprior", "serverview", "licensecheck",
        "ccqterminate", "ccaddbatch ", "lpdrestart", "adminforceauth1",
        "adminforceauth2", "adminforceauth3 ", "adminforceauth4",
        "faststartlogin", "opentab", "opentabcancel", "opentabamount",
        "opentabcard ", "opentabpay ", "continuetab", "continuetab2 ",
        "closetab", "closetab2 ", "forcereturn ", NULL};
    //for handy reference to the indices in the signal handler
    enum comms  { LOGOUT, NEXT_ARCHIVE, PRIOR_ARCHIVE, OPEN_DRAWER,
                  SHUTDOWN, SYSTEM_RESTART, CALIBRATE, WAGE_FILTER_DIALOG,
                  SERVER_NEXT, SERVER_PREV, SERVER_VIEW, LICENSE_CHECK,
                  CCQ_TERMINATE, CC_ADDBATCH, LPD_RESTART, ADMINFORCE1,
                  ADMINFORCE2, ADMINFORCE3, ADMINFORCE4,
                  FASTSTARTLOGIN, OPENTAB, OPENTABCANCEL, OPENTABAMOUNT,
                  OPENTABCARD, OPENTABPAY, CONTINUETAB, CONTINUETAB2,
                  CLOSETAB, CLOSETAB2, FORCERETURN};

    if (dialog)
    {
        // dialog intercepts all signals
        Zone *z = dialog;
        SignalResult sig = dialog->Signal(this, message);
        if (sig == SIGNAL_TERMINATE && dialog == z)
            KillDialog();
    }

    if (page == NULL)
        return SIGNAL_IGNORED;

    same_signal = 0;
    if (system_data->eod_term == NULL)
    {
        SignalResult sig = page->Signal(this, message, group_id);
        if (sig != SIGNAL_IGNORED)
            return sig;
    }

    int idx = CompareListN(commands, message);  
    switch (idx)
    {
    case LOGOUT:
        LogoutUser();
        return SIGNAL_OKAY;
    case NEXT_ARCHIVE:  // next archive
        if (archive == NULL)
            return SIGNAL_IGNORED;
        archive = archive->next;
        Update(UPDATE_ARCHIVE, NULL);
        return SIGNAL_OKAY;

    case PRIOR_ARCHIVE:  // prior archive
        if (archive == NULL)
            archive = system_data->ArchiveListEnd();
        else if (archive->fore)
            archive = archive->fore;
        else
            return SIGNAL_IGNORED;
        Update(UPDATE_ARCHIVE, NULL);
        return SIGNAL_OKAY;

    case OPEN_DRAWER:  // open drawer
    {
        Drawer *d = FindDrawer();
        if (d)
            OpenDrawer(d->position);
        break;
    }
    case SHUTDOWN:  // shutdown
        EndSystem();
        break;

    case SYSTEM_RESTART:  // systemrestart
        RestartSystem();
        break;

    case CALIBRATE:  // calibrate
        CalibrateTS();
        break;

    case WAGE_FILTER_DIALOG: // wagefilterdialog
        OpenDialog(new JobFilterDialog);
        break;

    case SERVER_NEXT: // servernext
        if (server)
            server = system_data->user_db.NextUser(this, server);
        else
            server = user;
        Update(UPDATE_SERVER, NULL);
        return SIGNAL_OKAY;

    case SERVER_PREV: // serverprior
        if (server)
            server = system_data->user_db.ForeUser(this, server);
        else
            server = user;
        Update(UPDATE_SERVER, NULL);
        return SIGNAL_OKAY;

    case SERVER_VIEW: // serverview
        if (server)
            server = NULL;
        else
            server = user;
        Update(UPDATE_SERVER, NULL);
        return SIGNAL_OKAY;
    case LICENSE_CHECK:  // licensecheck
        CheckLicense(GetSettings(), 1);
        Draw(1);
        return SIGNAL_OKAY;
    case CCQ_TERMINATE:
        strcpy(msg, "killall vt_ccq_pipe");
        system(msg);
        msg[0] = '\0';
        strcat(msg, "Connection reset.\\");
        strcat(msg, "Please wait 60 seconds\\");
        strcat(msg, "and try again.");
        sd = new SimpleDialog(msg);
        sd->Button(Translate("Okay"));
        OpenDialog(sd);
        return SIGNAL_OKAY;
    case CC_ADDBATCH:
        CC_Settle(&message[10]);
        return SIGNAL_OKAY;
    case LPD_RESTART:
        system(VIEWTOUCH_PATH "/bin/lpd-restart");
        return SIGNAL_OKAY;
    case ADMINFORCE1:
        if (admin_forcing == 0)
        {
            CreditCardVoiceDialog *ccvd = new CreditCardVoiceDialog("Enter TTID", "adminforceauth2");
            OpenDialog(ccvd);
            admin_forcing = 1;
        }
        return SIGNAL_OKAY;
    case ADMINFORCE2:
        if (admin_forcing == 1)
        {
            TenKeyDialog *tkd = new TenKeyDialog("Enter Final Amount", "adminforceauth3", 0, 1);
            if (dialog != NULL)
                NextDialog(tkd);
            else
                OpenDialog(tkd);
            admin_forcing = 2;
            return SIGNAL_OKAY;
        }
    case ADMINFORCE3: // adminforceauth3
        if (admin_forcing == 2)
        {
            int ttid   = atoi(auth_voice.Value());
            int amount = atoi(&message[16]);
            credit = new Credit();
            credit->SetTTID(ttid);
            credit->Amount(amount);
            CC_GetFinalApproval();
            admin_forcing = 3;
        }
        return SIGNAL_OKAY;
    case ADMINFORCE4:
        if (admin_forcing == 3)
        {
            if (credit != NULL)
                credit->PrintAuth();
            admin_forcing = 0;
        }
    case FASTSTARTLOGIN:
        FastStartLogin();
        return SIGNAL_OKAY;
    case OPENTAB:
        OpenTab();
        return SIGNAL_OKAY;
    case OPENTABCANCEL:
        OpenTab(TABOPEN_CANCEL);
        return SIGNAL_OKAY;
    case OPENTABAMOUNT:
        OpenTab(TABOPEN_AMOUNT);
        return SIGNAL_OKAY;
    case OPENTABCARD:
        OpenTab(TABOPEN_CARD, message);
        return SIGNAL_OKAY;
    case CONTINUETAB:
        ContinueTab();
        return SIGNAL_OKAY;
    case CONTINUETAB2:
        ContinueTab(atoi(&message[13]));
        return SIGNAL_OKAY;
    case CLOSETAB:
        CloseTab();
        return SIGNAL_OKAY;
    case CLOSETAB2:
        CloseTab(atoi(&message[10]));
        return SIGNAL_OKAY;
    case FORCERETURN:
        force_jump = atoi(&message[12]);
        if (page)
            force_jump_source = page->id;
        else
            force_jump_source = 0;
    }

    return SIGNAL_IGNORED;
}

SignalResult Terminal::Touch(int x, int y)
{
    FnTrace("Terminal::Touch()");

    if (system_data->eod_term != NULL)
        return SIGNAL_IGNORED;

    if (dialog)
    {
        // dialog intercepts all touches
        SignalResult sig = SIGNAL_IGNORED;
        Zone *z = dialog;
        if (dialog->IsPointIn(x, y))
        {
            if (host.length > 0)
                Bell();
            sig = dialog->Touch(this, x, y);
        }
        if (sig == SIGNAL_TERMINATE && dialog == z)
            KillDialog();
        return sig;
    }

    if (page == NULL)
        return SIGNAL_IGNORED;

    int touch;
    int select_flag = 0;
    int selected;
    Zone *z = page->FindZone(this, x, y);
    if (z)
    {
        SetFocus(z);
        touch = 1;
        if (z->ZoneStates() > 1)
        {
            if (selected_zone == z)
                select_flag = 1;

            ClearSelectedZone();
            selected = select_flag;
            switch (z->behave)
            {
            case BEHAVE_TOGGLE:
                selected ^= 1; break;
            case BEHAVE_BLINK:
            case BEHAVE_SELECT:
                selected  = 1; break;
            case BEHAVE_DOUBLE:
                touch     = selected;
                selected ^= 1;
                break;
            }

            if (selected)
            {
                selected_zone = z;
                z->Draw(this, 0);
            }
        }

        if (touch && z->active)
        {
            if (host.length > 0)
                Bell();
            SetFocus(z);
            return z->Touch(this, x, y);
        }
    }
    return SIGNAL_IGNORED;
}

/****
 * Terminal::Mouse
 * //NOTE BAK->Called by Terminal::MouseInput when not in edit mode.
 ****/
SignalResult Terminal::Mouse(int action, int x, int y)
{
    FnTrace("Terminal::Mouse()");

    if (system_data->eod_term != NULL)
        return SIGNAL_IGNORED;

    if (dialog)
    {
        // dialog intercepts all mouse actions
        SignalResult sig = SIGNAL_IGNORED;
        Zone *z = dialog;
        if (dialog->IsPointIn(x, y))
            sig = dialog->Mouse(this, action, x, y);
        if (sig == SIGNAL_TERMINATE && z == dialog)
            KillDialog();
        return sig;
    }

    if (page == NULL)
    {
        return SIGNAL_IGNORED;
    }

    Zone *z = page->FindZone(this, x, y);
    if (z)
    {
        int touch = 1;
        if ((z->ZoneStates() > 1) && (action & MOUSE_PRESS))
        {
            int select_flag = 0;
            if (selected_zone == z)
                select_flag = 1;

            ClearSelectedZone();
            int selected = select_flag;
            switch (z->behave)
            {
            case BEHAVE_TOGGLE:
                selected ^= 1;
                break;
            case BEHAVE_BLINK:
            case BEHAVE_SELECT:
                selected  = 1;
                break;
            case BEHAVE_DOUBLE:
                touch     = selected;
                selected ^= 1;
                break;
            }

            if (selected)
            {
                selected_zone = z;
                z->Draw(this, 0);
            }
        }

        if (touch)
        {
            if (action & MOUSE_PRESS)
                SetFocus(z);
            return z->Mouse(this, action, x, y);
        }
    }
    return SIGNAL_IGNORED;
}

SignalResult Terminal::Keyboard(int key, int state)
{
    FnTrace("Terminal::Keyboard()");

    if (system_data->eod_term != NULL)
        return SIGNAL_IGNORED;

    if (dialog)
    {
        // dialog intercepts all keyboard actions
        Zone *z = dialog;
        SignalResult sig = dialog->Keyboard(this, key, state);
        if (sig == SIGNAL_TERMINATE && z == dialog)
            KillDialog();
        return sig;
    }

    if (page == NULL)
        return SIGNAL_IGNORED;

    return page->Keyboard(this, key, state);
}

/****
 * OpenRecordFile:  Opens the recording file for writing.  Returns 0
 *  on success, 1 otherwise.  The file's name is partly  based on
 *  the current terminal's name.  This is for running macros that will
 *  automate logins and that sort of thing, very similar to macros
 *  in MS Windows 3.1.  It is primarily intended for debugging.
 ****/
int Terminal::OpenRecordFile()
{
    int retval = 0;
    char filename[STRLENGTH];
    char buffer[STRLENGTH];

    snprintf(filename, STRLENGTH, ".record_%s.macro", name.Value());
    record_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (record_fd < 0)
    {
        snprintf(buffer, STRLENGTH, "OpenRecordFile Error %d opening %s",
                 errno, filename);
        ReportError(buffer);
        retval = 1;
    }

    return retval;
}

/****
 * RecordTouch:
 ****/
int Terminal::RecordTouch(int x, int y)
{
    int retval = 0;
    genericChar recordstr[STRLENGTH];

    if (record_fd > 0)
    {
        snprintf(recordstr, STRLENGTH, "Touch: %d %d\n", x, y);
        write(record_fd, recordstr, strlen(recordstr));
    }
    return retval;
}

/****
 * RecordKey:
 ****/
int Terminal::RecordKey(int key, int my_code, int state)
{
    int retval = 0;
    genericChar recordstr[STRLENGTH];

    if (my_code == XK_F3 || my_code == XK_F4)
        retval = 1;
    else if (record_fd > 0)
    {
        snprintf(recordstr, STRLENGTH, "Key: %d %d %d\n", key, my_code, state);
        write(record_fd, recordstr, strlen(recordstr));
    }
    return retval;
}

/****
 * RecordMouse:
 ****/
int Terminal::RecordMouse(int my_code, int x, int y)
{
    int retval = 0;
    genericChar recordstr[STRLENGTH];

    if (record_fd > 0)
    {
        snprintf(recordstr, STRLENGTH, "Mouse: %d %d %d\n", my_code, x, y);
        write(record_fd, recordstr, strlen(recordstr));
    }
    return retval;
}

/****
 * ReadRecordFile:  See OpenRecordFile() for a discussion of the record
 *  files.  Short Version:  they are macro files for automating logins
 *  and that sort of thing.  At the moment, each terminal can have only
 *  one macro file.
 ****/
int Terminal::ReadRecordFile()
{
    int retval = 0;
    genericChar filename[STRLENGTH];
    genericChar key[STRLENGTH];
    genericChar value[STRLENGTH];
    int idx;
    int keyval;
    int x;
    int y;
    int my_code;
    int state;
    KeyValueInputFile infile;

    snprintf(filename, STRLENGTH, ".record_%s.macro", name.Value());
    if (infile.Open(filename))
    {
        while (infile.Read(key, value, STRLENGTH))
        {
            idx = 0;
            if (strcmp(key, "Touch") == 0)
            {
                NextInteger(&x, value, ' ', &idx);
                NextInteger(&y, value, ' ', &idx);
                Touch(x, y);
            }
            else if (strcmp(key, "Mouse") == 0)
            {
                NextInteger(&my_code, value, ' ', &idx);
                NextInteger(&x, value, ' ', &idx);
                NextInteger(&y, value, ' ', &idx);
                Mouse(my_code, x, y);
            }
            else if (strcmp(key, "Key") == 0)
            {
                NextInteger(&keyval, value, ' ', &idx);
                NextInteger(&my_code, value, ' ', &idx);
                NextInteger(&state, value, ' ', &idx);
                Keyboard(keyval, state);
            }
        }
        infile.Close();
    }

    return retval;
}

/****
 * SetFocus:  Determines whether one zone loses focus and another gains focus.
 *   This is primarily so that, for example, the SearchZone can redraw when
 *   it is no longer in use.
 ****/
int Terminal::SetFocus(Zone *newzone)
{
    FnTrace("Terminal::SetFocus()");
    if (newzone == NULL || edit != 0)
        return 1;

    if ((newzone != previous_zone) && newzone->GainFocus(this, previous_zone))
    {
        if (previous_zone != NULL)
            previous_zone->LoseFocus(this, newzone);
        previous_zone = newzone;
    }
    return 0;
}

int Terminal::LoginUser(Employee *employee, bool home_page)
{
    FnTrace("Terminal::LoginUser()");

    if (AllowLogins == 0)
        return 1;

    if (employee == NULL || (user != employee && parent->IsUserOnline(employee)))
        return 1;  // User already online on another terminal

    if (user != NULL && user != employee)
        LogoutUser(0);

    Settings *settings    = GetSettings();
    timeout               = settings->delay_time1;

    user                  = employee;
    employee->current_job = system_data->labor_db.CurrentJob(employee);
    employee->last_job    = employee->current_job;

    if (((home_page == true) && employee->current_job) > 0)
    {
        int homepage = HomePage();
        Jump(JUMP_STEALTH, homepage);
    }

    UpdateOtherTerms(UPDATE_USERS, NULL);
    return 0;
}

int Terminal::LogoutUser(int update)
{
    FnTrace("Terminal::LogoutUser()");
    if (parent == NULL)
        return 1;

    int error = StoreCheck(0);
    if (translate)
        TranslateTerm();
    if (edit)
        EditTerm();

    // Reset terminal values
    Settings *s = GetSettings();
    timeout        = s->delay_time1;
    archive        = NULL;
    stock          = NULL;
    server         = NULL;
    password_given = 0;
    last_index     = INDEX_GENERAL;
    job_filter     = 0;
    expand_labor   = 0;
    hide_zeros     = 0;
    show_family    = 1;
    type           = original_type;

    if (user)
    {
        user->current_job = 0;
        user = NULL;
        if (update)
        {
            if (error)
                UpdateOtherTerms(UPDATE_USERS, NULL);
            else
                UpdateOtherTerms(UPDATE_USERS | UPDATE_CHECKS, NULL);
        }
    }

    Jump(JUMP_STEALTH, PAGEID_LOGIN);  // Jump to login page
    ClearPageStack();
    return 0;
}

int Terminal::GetCheck(const genericChar* label, int customer_type)
{
    FnTrace("Terminal::GetCheck()");
    if (user == NULL ||
        (customer_type != CHECK_RESTAURANT && customer_type != CHECK_HOTEL))
        return 1;  // No current user

    Settings *settings = GetSettings();
    Check *thisCheck = system_data->FindOpenCheck(label, user);
    if (thisCheck == NULL)
    {
        // Create new thisCheck
        if (system_data->LicenseExpired())
        {
            SimpleDialog *d = new SimpleDialog("New checks cannot be created until the license is renewed.");
            d->Button("Continue");
            OpenDialog(d);
            return 1;
        }

        thisCheck = new Check(settings, customer_type, user);
        if (thisCheck == NULL)
            return 1;

        thisCheck->Table(label);

        if (type == TERMINAL_BAR || type == TERMINAL_BAR2 || type == TERMINAL_FASTFOOD)
            thisCheck->Guests(1);

        system_data->Add(thisCheck);
    }
    else if (thisCheck->user_current > 0 && thisCheck->user_current != user->id)
        // thisCheck in use by another user
        return 1;

    return SetCheck(thisCheck, 0);
}

int Terminal::NewTakeOut(int customer_type)
{
    FnTrace("Terminal::NewTakeOut()");

    //break if undefined user or incorrect mode
    if (user == NULL || 
        (customer_type != CHECK_TAKEOUT && 
         customer_type != CHECK_DELIVERY && 
         customer_type != CHECK_RETAIL))
        return 1;

    if (system_data->LicenseExpired())
    {
        SimpleDialog *my_dialog = new SimpleDialog("New checks cannot be created until the license is renewed.");
        my_dialog->Button("Continue");
        OpenDialog(my_dialog);

        return 1;
    }

    Settings *settings = GetSettings();
    Check *thisCheck = new Check(settings, customer_type, user);
    if (thisCheck == NULL)
        return 1;

    thisCheck->Guests(0);  // No guests for takeout
    system_data->Add(thisCheck);

    return SetCheck(thisCheck);
}

int Terminal::NewFastFood(int customer_type)
{
    FnTrace("Terminal::NewFastFood()");
    if (user == NULL || (customer_type != CHECK_FASTFOOD))
        return 1;

    if (system_data->LicenseExpired())
    {
        SimpleDialog *d = new SimpleDialog("New checks cannot be created until the license is renewed.");
        d->Button("Continue");
        OpenDialog(d);
        return 1;
    }

    Settings *settings = GetSettings();
    Check *thisCheck = new Check(settings, customer_type, user);
    if (thisCheck == NULL)
        return 1;

    thisCheck->Guests(0);  // No guest
    system_data->Add(thisCheck);
    return SetCheck(thisCheck);
}

/****
 * QuickMode:  this method is intended to replace methods ::NewTakeOut() and
 *   ::NewFastFood().
 ****/
int Terminal::QuickMode(int customer_type)
{
    FnTrace("Terminal::QuickMode()");
    if (user == NULL || 
        ((customer_type != CHECK_FASTFOOD) && 
         (customer_type != CHECK_RETAIL) &&
         (customer_type != CHECK_DELIVERY) &&
         (customer_type != CHECK_CATERING) &&
         (customer_type != CHECK_TAKEOUT) &&
         (customer_type != CHECK_CALLIN) &&
         (customer_type != CHECK_DINEIN) &&
         (customer_type != CHECK_TOGO) &&
         (customer_type != CHECK_BAR)))
    {
        return 1;
    }

    if (system_data->LicenseExpired())
    {
        SimpleDialog *d = new SimpleDialog("New checks cannot be created until the license is renewed.");
        d->Button("Continue");
        OpenDialog(d);
        return 1;
    }

    if (customer != NULL)
        customer->Save();

    Settings *settings = GetSettings();
    Check *thisCheck = new Check(settings, customer_type, user);
    if (thisCheck->customer != NULL)
    {
        customer = thisCheck->customer;
        if (thisCheck->IsTraining())
            customer->IsTraining(1);
    }
    else
        customer = NULL;

    if (customer_type == CHECK_FASTFOOD ||
        customer_type == CHECK_BAR ||
        (settings->fast_takeouts &&
         (customer_type == CHECK_TAKEOUT  ||
          customer_type == CHECK_DELIVERY ||
          customer_type == CHECK_CALLIN   ||
          customer_type == CHECK_DINEIN   ||
          customer_type == CHECK_TOGO     ||
          customer_type == CHECK_CATERING)))
    {
        type = TERMINAL_FASTFOOD;
    }

    if (thisCheck == NULL)
        return 1;

    thisCheck->Guests(0);  // No guest
    thisCheck->date.Set();
    system_data->Add(thisCheck);

    return SetCheck(thisCheck);
}

int Terminal::SetCheck(Check *currCheck, int update_local)
{
    FnTrace("Terminal::SetCheck()");
    if (user == NULL || currCheck == NULL || (user->training && !currCheck->IsTraining()))
        return 1;  // can't set currCheck
    SubCheck *currsub = NULL;

    if (currCheck->user_current > 0 && currCheck->user_current != user->id)
        return 1;  // someone else is holding the currCheck

    // update the member data
    if (check != currCheck)
    {
        StoreCheck(0);
        currCheck->user_current = user->id;
        check = currCheck;
        customer = check->customer;
        seat  = 0;
    }

    Settings *settings = GetSettings();
    guests = currCheck->Guests();

    // set check_balanced
    currsub = currCheck->current_sub;
    if (currsub == NULL)
        currsub = currCheck->FirstOpenSubCheck();
    if (currsub != NULL)
        check_balanced = (currsub->balance == 0) ? 1 : 0;

    if (settings->drawer_mode == DRAWER_SERVER &&
        user->CanSettle(settings) && (! user->CanEdit()))
    {
        system_data->GetServerBank(user); // make serverbank if none
    }

    if (update_local)
        Update(UPDATE_PAYMENTS | UPDATE_ORDERS | UPDATE_CHECKS | UPDATE_TABLE, currCheck->Table());

    UpdateOtherTerms(UPDATE_TABLE | UPDATE_CHECKS, currCheck->Table());

    return 0;
}

int Terminal::StoreCheck(int update)
{
    FnTrace("Terminal::StoreCheck()");
    if (check == NULL)
        return 1;  // No current check

    // Save (or delete) check
    Str table(check->Table());
    check->Update(GetSettings());
    if (check->IsEmpty() && (check->Guests() <= 0 || check->IsTakeOut() || check->IsFastFood()))
    {
        system_data->DestroyCheck(check);
    }
    else
    {
        check->current_sub  = NULL;
        check->user_current = 0;
        check->Save();
    }

    order      = NULL;
    check      = NULL;
    seat       = 0;
    move_check = 0;
    guests     = 0;
    if (update)
    {
        Update(UPDATE_ORDERS | UPDATE_PAYMENTS | UPDATE_TABLE | UPDATE_CHECKS,
               table.Value());
        UpdateOtherTerms(UPDATE_TABLE | UPDATE_CHECKS, table.Value());
    }
    else
        UpdateAllTerms(UPDATE_TABLE, table.Value());

    return 0;
}

int Terminal::NextPage()
{
    FnTrace("Terminal::NextPage()");
    Page *currPage = page;
    if (currPage == NULL)
        return 1;

    int flag = !(user->CanEditSystem() || translate);
    do
        currPage = currPage->next;
    while (currPage && currPage->id < 0 && flag);

    if (currPage == NULL)
    {
        currPage = zone_db->PageList();
        while (currPage && currPage->id < 0 && flag)
            currPage = currPage->next;

        if (currPage == NULL)
            return 1;  // No valid pages to jump to
    }
    return ChangePage(currPage);
}

int Terminal::ForePage()
{
    FnTrace("Terminal::ForePage()");
    Page *currPage = page;
    if (currPage == NULL)
        return 1;

    int flag = !(user->CanEditSystem() || translate);
    do
        currPage = currPage->fore;
    while (currPage && currPage->id < 0 && flag);

    if (currPage == NULL)
    {
        currPage = zone_db->PageListEnd();
        while (currPage && currPage->id < 0 && flag)
            currPage = currPage->fore;

        if (currPage == NULL)
            return 1;  // No valid pages to jump to
    }
    return ChangePage(currPage);
}

int Terminal::Update(int update_message, const genericChar* value)
{
    FnTrace("Terminal::Update()");
    if (page == NULL)
        return 1;

    if (update_message & UPDATE_MINUTE)
        DrawTitleBar();
    return page->Update(this, update_message, value);
}

Drawer *Terminal::FindDrawer()
{
    FnTrace("Terminal::FindDrawer()");
    if (user == NULL || user->training)
        return NULL;

    Settings *settings = GetSettings();
    int dm = settings->drawer_mode;

    // Find Physical Drawers
    Drawer *d = system_data->FirstDrawer();
    while (d)
    {
        if (d->IsOpen() && d->term == this)
        {
            switch (dm)
            {
            default:
            case DRAWER_NORMAL:
            case DRAWER_SERVER:
                return d;
            case DRAWER_ASSIGNED:
                if (d->owner_id == user->id)
                    return d;
                break;
            }
        }
        d = d->next;
    }

    if (dm == DRAWER_SERVER)
        return system_data->GetServerBank(user);

    return NULL;
}

int Terminal::OpenDrawer(int position)
{
    FnTrace("Terminal::OpenDrawer()");
    if (user == NULL || drawer_count <= 0 || parent == NULL)
        return 1;  // No drawer to open

    Printer *p = parent->FindPrinter(name.Value());
    if (p == NULL)
        return 1;

    p->OpenDrawer(position);
    return 0;
}

int Terminal::NeedDrawerBalanced(Employee *e)
{
    FnTrace("Terminal::NeedDrawerBalanced()");
    int retval = 0;
    Settings *settings = GetSettings();
    Drawer *drawer = system_data->FirstDrawer();

    if (settings->drawer_mode == DRAWER_SERVER && settings->require_drawer_balance == 1)
    {
        while (drawer != NULL)
        {
            if (drawer->owner_id == e->id && drawer->IsEmpty() == 0)
            {
                retval = 1;
                drawer = NULL;  // exit loop
            }
            else
                drawer = drawer->next;
        }
    }

    return retval;
}

int Terminal::CanSettleCheck()
{
    FnTrace("Terminal::CanSettleCheck()");
    Settings *s = GetSettings();
    if (user == NULL || !user->CanSettle(s) || check == NULL)
        return 0;  // no
    else if (user->training)
        return 1;  // yes
    else if (type == TERMINAL_ORDER_ONLY)
        return 0;  // no

    if (user->IsSupervisor(s))
        return 1;

    Drawer *d = FindDrawer();
    if (d && d->IsServerBank() && check->user_owner != user->id)
        return 0;  // no
    return (d != NULL);
}

int Terminal::StackCheck(int customer_type)
{
    FnTrace("Terminal::StackCheck()");
    if (user == NULL || check == NULL)
        return 1;

    int my_depth = system_data->NumberStacked(check->Table(), user);
    if (my_depth >= 2)
        return 1;

    Settings *s = GetSettings();
    Check *c = new Check(s, customer_type, user);
    if (c == NULL)
        return 1;

    c->Table(check->Table());
    system_data->Add(c);
    SetCheck(c);
    UpdateAllTerms(UPDATE_CHECKS | UPDATE_TABLE, c->Table());

    return 0;
}

int Terminal::OpenDialog(Zone *currZone)
{
    FnTrace("Terminal::OpenDialog()");

    if (currZone == NULL || page == NULL)
        return 1;

    if (dialog)
    {
        return NextDialog(currZone);
    }

    currZone->RenderInit(this, RENDER_NEW);
    int page_max = page->width - 32;

    if (currZone->w > page_max)
        currZone->w = page_max;

    page_max = page->height - 48;

    if (currZone->h > page_max)
        currZone->h = page_max;

    currZone->x = (page->width  - currZone->w) / 2;
    currZone->y = (page->height - currZone->h) / 2;
    currZone->update = 1;

    RegionInfo r(currZone);
    r.w += currZone->shadow;
    r.h += currZone->shadow;

    dialog = currZone;
    Draw(0, r.x, r.y, r.w, r.h);

    return 0;
}

int Terminal::OpenDialog(const genericChar* message)
{
    FnTrace("Terminal::OpenDialog()");
    return OpenDialog(new MessageDialog(message));
}

int Terminal::NextDialog(Zone *currZone)
{
    FnTrace("Terminal::NextDialog()");
    int retval = 0;

    if (next_dialog != NULL)
        delete next_dialog;
    next_dialog = currZone;

    return retval;
}

int Terminal::KillDialog()
{
    FnTrace("Terminal::KillDialog()");
    int jump_index = 0;
    char next_signal[STRLENGTH];

    if (dialog == NULL)
        return 1;

    if (selected_zone == dialog)
        selected_zone = NULL;

    jump_index = ((DialogZone *)dialog)->target_index;
    strcpy(next_signal, ((DialogZone *)dialog)->target_signal);
    RegionInfo r(dialog);
    r.w += dialog->shadow;
    r.h += dialog->shadow;
    delete dialog;
    dialog = NULL;
    
    Draw(1);
    UpdateAll();

    if (jump_index != 0)
        JumpToIndex(jump_index);
    if (next_signal[0] != '\0')
        Signal(next_signal, 0);

    if (next_dialog)
    {
        Draw(1);
        OpenDialog(next_dialog);
        next_dialog = NULL;
    }

    return 0;
}

int Terminal::HomePage()
{
    FnTrace("Terminal::HomePage()");
    char not_allowed[] = "User not allowed in system.";
    Settings *settings = GetSettings();
    Page *currPage = NULL;
    SimpleDialog *sd = NULL;

    if (user == NULL || (! user->CanEnterSystem(settings)))
    {
        fprintf(stderr, "%s\n", not_allowed);
        sd = new SimpleDialog(not_allowed);
        sd->Button("Okay");
        OpenDialog(sd);
        return PAGEID_LOGIN;
    }

    // First look for a page associated with Terminal Type.  This is deprecated.
    // In the future, use the employee's job starting page to determine the
    // appropriate action (the "expedite" argument of LoginZone::Start() will
    // be deprecated as well).
    if (type == TERMINAL_KITCHEN_VIDEO || type == TERMINAL_KITCHEN_VIDEO2 ||
        type == TERMINAL_BAR || type == TERMINAL_BAR2)
    {
        if ((currPage = zone_db->FindByTerminal(type, -1, size)) == NULL)
            fprintf(stderr, "Could not find page for terminal %s\n", name.Value());
    }

    // If we didn't get a page from Terminal Type, get one normally.
    if (currPage == NULL)
    {
        int start = user->StartingPage();
        if (start > 0)
        {
            if ((currPage = zone_db->FindByID(start, size)) == NULL)
                fprintf(stderr, "Could not find start page\n");
        }
        else if (start == 0)
        {
            if ((currPage = zone_db->FindByType(PAGE_CHECKS, -1, size)) == NULL)
                fprintf(stderr, "Could not find checks page\n");
        }
        else
        {
            if ((currPage = zone_db->FindByID(start, size)) == NULL)
                fprintf(stderr, "Could not find start page\n");
            if (currPage == NULL || currPage->IsStartPage() == 0)
            {
                if ((currPage = zone_db->FirstTablePage(size)) == NULL)
                    fprintf(stderr, "Could not find table page\n");
            }
        }
        
        if (currPage && currPage->id == page->id)
        {
            if (page->type == PAGE_CHECKS)
                currPage = zone_db->FirstTablePage(size);
            else
                currPage = zone_db->FindByType(PAGE_CHECKS, -1, size);
        }
    }

    if (currPage)
        return currPage->id;
    else
        return PAGEID_LOGIN;
}

int Terminal::UpdateAllTerms(int update_message, const genericChar* value)
{
    FnTrace("Terminal::UpdateAllTerms()");
    if (parent)
        return parent->UpdateAll(update_message, value);
    else
        return Update(update_message, value);
}

int Terminal::UpdateOtherTerms(int update_message, const genericChar* value)
{
    FnTrace("Terminal::UpdateOtherTerms()");
    if (parent)
        return parent->UpdateOther(this, update_message, value);
    else
        return 0;
}

int Terminal::TermsInUse()
{
    FnTrace("Terminal::TermsInUse()");
    int count = 0;
    for (Terminal *t = parent->TermList(); t != NULL; t = t->next)
    {
        if (t->user)
            ++count;
    }
    return count;
}

int Terminal::OtherTermsInUse(int no_kitchen)
{
    FnTrace("Terminal::OtherTermsInUse()");
    int count = 0;
    for (Terminal *t = parent->TermList(); t != NULL; t = t->next)
    {
        if (t != this)
        {
            if (no_kitchen == 0 || t->page == NULL || t->page->IsKitchen() == 0)
            {
                if (t->user)
                    ++count;
            }
        }
    }
    return count;
}

int Terminal::EditTerm(int save_data)
{
    FnTrace("Terminal::EditTerm()");
    if (parent == NULL)
        return 1;
    if (user == NULL || !user->CanEdit())
        return 1;

    Settings *settings = GetSettings();

    previous_zone = NULL;

    if (edit)
    {
        // Stop editing term - save changed
        parent->SetAllCursors(CURSOR_WAIT);
        WInt8(TERM_KILLWINDOW);
        WInt16(WIN_TOOLBAR);
        SendNow();
        edit = 0;
        show_info = 0;
        if (save_data)
        {
            parent->SetAllMessages("Saving...");
            parent->zone_db = zone_db->Copy();
            system_data->menu.DeleteUnusedItems(zone_db);
            system_data->menu.Save();
            system_data->inventory.ScanItems(&(system_data->menu));
            parent->ClearAllFocus();
            parent->UpdateAll(UPDATE_MENU, NULL);
            if (zone_db != NULL)
                zone_db->ClearEdit(this);
            Draw(RENDER_NEW);
            if (user && user->CanEditSystem())
                SaveSystemData();
            parent->SaveMenuPages();
            parent->SaveTablePages();
            if (system_data->user_db.changed)
                system_data->user_db.Save();
            settings->Save();
        }
        else
        {
            parent->SetAllMessages("Restoring...");
            org_page_id = page->id;
            UpdateZoneDB(parent);
        }
        // make sure order entry window are saved
        Terminal *currTerm = parent->TermList();
        while (currTerm != NULL)
        {
            currTerm->ClearMessage();
            currTerm->reload_zone_db = 1;
            currTerm = currTerm->next;
        }
        reload_zone_db = 0;
        parent->SetAllCursors(CURSOR_POINTER);
        return 0;
    }

    // The Else block.  Here's what happens when we switch into
    // edit mode.
    if (translate)
        TranslateTerm();

    if (system_data->LicenseExpired())
    {
        SimpleDialog *d = new SimpleDialog("Edit Mode is disabled");
        d->Button("Continue");
        OpenDialog(d);
        return 0;
    }

    for (Terminal *t = parent->TermList(); t != NULL; t = t->next)
    {
        if (t->edit || t->translate)
        {
            SimpleDialog *d = new SimpleDialog("Someone else is already in Edit Mode");
            d->Button("Okay");
            OpenDialog(d);
            return 1;  // another terminal already being edited
        }
    }

    // Start editing term
    edit = 1;
    Draw(RENDER_NEW);

    // Create Edit Tool Bar
    WInt8(TERM_NEWWINDOW);
    WInt16(WIN_TOOLBAR);
    WInt16(64);  // x
    WInt16(64);  // y
    WInt16(120); // width
    WInt16(300); // height
    WInt8(WINFRAME_BORDER | WINFRAME_TITLE | WINFRAME_MOVE);
    WStr("Edit ToolBar");

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_NEWZONE);
    WInt16(0); WInt16(0); WInt16(60); WInt16(60);
    WStr("New\\Button");
    WInt8(FONT_HELV_18); WInt8(COLOR_DK_BLUE); WInt8(COLOR_LT_BLUE);

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_NEWPAGE);
    WInt16(60); WInt16(0); WInt16(60); WInt16(60);
    WStr("New\\Page");
    WInt8(FONT_HELV_18); WInt8(COLOR_DK_GREEN); WInt8(COLOR_GREEN);

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_ALL);
    WInt16(0); WInt16(60); WInt16(60); WInt16(60);
    WStr("Select\\All");
    WInt8(FONT_HELV_14); WInt8(COLOR_DK_TEAL); WInt8(COLOR_TEAL);

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_TOGGLE);
    WInt16(60); WInt16(60); WInt16(60); WInt16(60);
    WStr("Toggle\\Selected");
    WInt8(FONT_HELV_14); WInt8(COLOR_DK_MAGENTA); WInt8(COLOR_MAGENTA);

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_COPY);
    WInt16(0); WInt16(120); WInt16(60); WInt16(60);
    WStr("Copy\\Selected");
    WInt8(FONT_HELV_14); WInt8(COLOR_DK_GREEN); WInt8(COLOR_GREEN);

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_MOVE);
    WInt16(60); WInt16(120); WInt16(60); WInt16(60);
    WStr("Move\\Selected");
    WInt8(FONT_HELV_14); WInt8(COLOR_DK_BLUE); WInt8(COLOR_LT_BLUE);

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_INFO);
    WInt16(0); WInt16(180); WInt16(60); WInt16(60);
    WStr("Show\\Button\\Info");
    WInt8(FONT_HELV_14); WInt8(COLOR_GRAY); WInt8(COLOR_WHITE);

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_LIST);
    WInt16(60); WInt16(180); WInt16(60); WInt16(60);
    WStr("Show\\Page\\List");
    WInt8(FONT_HELV_14); WInt8(COLOR_BROWN); WInt8(COLOR_ORANGE);

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_PRIOR);
    WInt16(0); WInt16(240); WInt16(60); WInt16(60);
    WStr("Prior\\Page");
    WInt8(FONT_HELV_18); WInt8(COLOR_DK_RED); WInt8(COLOR_RED);

    WInt8(TERM_PUSHBUTTON);
    WInt16(WB_NEXT);
    WInt16(60); WInt16(240); WInt16(60); WInt16(60);
    WStr("Next\\Page");
    WInt8(FONT_HELV_18); WInt8(COLOR_DK_RED); WInt8(COLOR_RED);

    // Show Edit Tool Bar
    WInt8(TERM_SHOWWINDOW);
    WInt16(WIN_TOOLBAR);
    SendNow();
    return 0;
}

int Terminal::TranslateTerm()
{
    FnTrace("Terminal::TranslateTerm()");
    if (parent == NULL)
        return 1;
    if (user == NULL || !user->CanEdit())
        return 1;

    if (translate)
    {
        translate = 0;
        MasterLocale->Save();
        Draw(RENDER_NEW);
        return 0;
    }

    if (edit)
        EditTerm();

    if (system_data->LicenseExpired())
    {
        SimpleDialog *d = new SimpleDialog("Translate Mode is disabled");
        d->Button("Continue");
        OpenDialog(d);
        return 0;
    }

    for (Terminal *t = parent->TermList(); t != NULL; t = t->next)
        if (t->edit || t->translate)
            return 1;  // another terminal already being edited

    // Start editing term
    translate = 1;
    Draw(RENDER_NEW);
    return 0;
}

/****
 * UpdateZoneDB:  Read the notes on Control::NewZoneDB() for information
 *  about how zone_db works.  Here we want to grab a new copy from
 *  the control object and then try to find our way back to the original
 *  page.  Only Terminal::EditTerm() sets org_page_id, and only for the
 *  Undo operation.  If that is not set, or if we can't find the specified
 *  page (maybe the Undo deleted the page the user was on) we'll just go
 *  back to the login page.  If the login page doesn't exist, we're in big
 *  trouble, but that always indicates an incomplete or corrupted install.
 ****/
int Terminal::UpdateZoneDB(Control *con)
{
    FnTrace("Terminal::UpdateZoneDB()");
    if (con == NULL)
        return 1;

    parent = con;
    if (user && zone_db && !org_page_id)
        LogoutUser(0);
    KillDialog();

    if (zone_db)
        delete zone_db;

    reload_zone_db = 0;
    zone_db = con->NewZoneDB();
    if (zone_db == NULL)
        return 1;

    if (page)
    {
        // invalid page pointer - Return to the login page.
        page = NULL;
        if (org_page_id)
            page = zone_db->FindByID(org_page_id, size);
        if (page == NULL)
            page = zone_db->FindByID(PAGEID_LOGIN, size);
        if (page == NULL)
        {
            genericChar buffer[STRLONG];
            snprintf(buffer, STRLONG, "Can't Find Login Page (%d) for %s",
                     PAGEID_LOGIN, name.Value());
            ReportError(buffer);
            page = NULL;
        }
        Draw(1);
    }
    org_page_id = 0;

    // SERVER_TERMINFO command from term will cause jump to login page
    return 0;
}

/****
 * ReplaceSymbols:  Chop through the symbol list, extracting the
 *  inidividual elements and respond to each symbol accordingly 
 *  (str contains a list of 'symbols' contained within '{ }'
 *  and terminated by a null character)
 ****/
const genericChar* Terminal::ReplaceSymbols(const genericChar* str)
{
    FnTrace("Terminal::ReplaceSymbols()");
    static const genericChar* symbols[] = {
        "release", "time", "date", "name", "termname", "machineid",
        "machinekey", "licensedays", "creditid", "debitid",
        "merchantid", NULL
    };


    static genericChar buffer[1024];

    if (edit || str == NULL)
        return (genericChar*)str;       //TODO: what to do in edit mode?  This is impossible to do correctly.  Can cause a stack overlflow

    genericChar* rawBuffer = buffer;
    if (str)
    {
        genericChar tmp[STRLENGTH];
        const genericChar* thisStr = str;
        while (*thisStr)
        {
            if (*thisStr != '{')
            {
                *rawBuffer++ = *thisStr++;
            }
            else
            {
                ++thisStr;
                genericChar* t = tmp;

                // fill tmp with chars from str until '}'
                while (*thisStr && *thisStr != '}')
                    *t++ = *thisStr++;

                // terminate the genericChar array
                *t = '\0';

                int idx = CompareList(tmp, symbols);
                switch (idx)
                {
                case 0:  // release
                    sprintf(tmp, "POS %s - Copyright Gene Mosher 1986", BuildNumber);
                    break;
                case 1:  // time
                    TimeDate(tmp, SystemTime, TD_TIME);
                    break;
                case 2:  // date
                    TimeDate(tmp, SystemTime, TD_DATE);
                    break;
                case 3:  // name
                    if (user)
                        strcpy(tmp, user->system_name.Value());
                    else
                        strcpy(tmp, "User");
                    break;
                case 4:  // termname
                    strcpy(tmp, name.Value());
                    break;
                case 5:  // machineid
                    GetMacAddress(tmp, STRLENGTH);
                    break;
                case 6:  // machinekey
                    GetMachineDigest(tmp, 20);
                    tmp[20] = '\0';
                    break;
                case 7:  // licensedays
                    sprintf(tmp, "%d", GetSettings()->expire_days);
                    break;
                case 8:  // creditid
                    strcpy(tmp, cc_credit_termid.Value());
                    break;
                case 9:  // debitid
                    strcpy(tmp, cc_debit_termid.Value());
                    break;
                case 10:  // merchantid
                    strcpy(tmp, GetSettings()->cc_merchant_id.Value());
                    break;
                default:
                    tmp[0] = '\0';
                    break;
                }

                t = tmp;

                while (*t)
                    *rawBuffer++ = *t++;

                if (*thisStr)
                    ++thisStr;
            }
        }
    }
    *rawBuffer = '\0';
    return Translate(buffer);
}

Printer *Terminal::FindPrinter(int printer_id)
{
    FnTrace("Terminal::FindPrinter()");
    if (parent == NULL)
        return NULL;

    Printer *currPrinter = parent->FindPrinter(printer_host.Value(), printer_port);

    // redirect bar printing
    Settings *settings = GetSettings();
    if (currPrinter && (type == TERMINAL_BAR2 ||
                        (type == TERMINAL_BAR &&
                         (printer_id == PRINTER_BAR1 || printer_id == PRINTER_BAR2))))
        printer_id = PRINTER_RECEIPT;

    for (;;)
    {
        if (printer_id == PRINTER_RECEIPT)
        {
            if (currPrinter)
                return currPrinter;
        }
        else
        {
            PrinterInfo *pi = settings->FindPrinterByType(printer_id);
            if (pi)
                return pi->FindPrinter(parent);
        }

        switch (printer_id)
        {
        case PRINTER_REPORT:
            return currPrinter;
        case PRINTER_RECEIPT:
            printer_id = PRINTER_REPORT;
            break;
        case PRINTER_KITCHEN2:
            printer_id = PRINTER_KITCHEN1;
            break;
        case PRINTER_KITCHEN3:
            printer_id = PRINTER_KITCHEN1;
            break;
        case PRINTER_KITCHEN4:
            printer_id = PRINTER_KITCHEN3;
            break;
        case PRINTER_BAR1:
            printer_id = PRINTER_KITCHEN1;
            break;
        case PRINTER_BAR2:
            printer_id = PRINTER_BAR1;
            break;
        case PRINTER_EXPEDITER:
            printer_id = PRINTER_KITCHEN1;
            break;
        case PRINTER_CREDITRECEIPT:
            printer_id = PRINTER_RECEIPT;
            break;
        default:
            return NULL;
        }
    }
}

int Terminal::FrameBorder(int frame, int shape)
{
    FnTrace("Terminal::FrameBorder()");
    int b = 3;
    if (page->size <= SIZE_800x600)
        b = 2;
    int offset = b;
    if (shape != SHAPE_RECTANGLE)
        offset = b * 3;

    if (frame == ZF_DEFAULT)
        frame = page->default_frame[0];

    switch (frame)
    {
    case ZF_HIDDEN: case ZF_NONE:
        return offset;

    case ZF_DOUBLE: case ZF_DOUBLE1: case ZF_DOUBLE2: case ZF_DOUBLE3:
        /** fall through **/
    case ZF_BORDER: case ZF_CLEAR_BORDER: case ZF_SAND_BORDER:
        /** fall through **/
    case ZF_LIT_SAND_BORDER: case ZF_INSET_BORDER: case ZF_PARCHMENT_BORDER:
        return offset + (b * 3);

    case ZF_DOUBLE_BORDER: case ZF_LIT_DOUBLE_BORDER:
        return offset + (b * 5);

    default:
        return offset + b;
    }
}

int Terminal::TextureTextColor(int texture)
{
    FnTrace("Terminal::TextureTextColor()");
    if (texture == IMAGE_DEFAULT)
        texture = page->default_texture[0];
    if (texture == IMAGE_CLEAR)
        texture = page->image;

    switch (texture)
    {
    case IMAGE_SAND:
        /** fall through **/
    case IMAGE_LIT_SAND:
        /** fall through **/
    case IMAGE_LITE_WOOD:
        /** fall through **/
    case IMAGE_PARCHMENT:
        /** fall through **/
    case IMAGE_PEARL:
        /** fall through **/
    case IMAGE_SMOKE:
        /** fall through **/
    case IMAGE_LEATHER:
        /** fall through **/
    case IMAGE_GRADIENT:
        /** fall through **/
    case IMAGE_CANVAS:
        return COLOR_BLACK;

    default:
        return COLOR_WHITE;
    }
}

int Terminal::FontSize(int font_id, int &w, int &h)
{
    FnTrace("Terminal::FontSize()");
    if (font_id == FONT_DEFAULT)
        font_id = page->default_font;
    return GetFontSize(font_id, w, h);
}

int Terminal::TextWidth(const genericChar* my_string, int len, int font_id)
{
    FnTrace("Terminal::TextWidth()");

    if (font_id < 0)
        font_id = curr_font_id;
    if (font_id == FONT_DEFAULT)
        font_id = page->default_font;
    if (font_id < 0)
        return 1;

    if (len < 0)
        len = strlen(my_string);

    return GetTextWidth(my_string, len, font_id);
}

int Terminal::IsUserOnline(Employee *e)
{
    FnTrace("Terminal::IsUserOnline()");
    if (e == NULL)
        return 0;

    if (parent)
        return parent->IsUserOnline(e);
    else
        return (user == e);
}

int Terminal::FinalizeOrders()
{
    FnTrace("Terminal::FinalizeOrders()");
    int jump_target = PAGE_ID_SETTLEMENT;
    char str[STRLENGTH];
    if (check == NULL)
        return 1;

    SubCheck *sc = check->current_sub;
    if (sc == NULL)
        return 1;

    seat  = 0;
    order = NULL;
    qualifier = QUALIFIER_NONE;
    check->Save();
    check->FinalizeOrders(this);
    Update(UPDATE_ORDERS | UPDATE_CHECKS, NULL);
    UpdateOtherTerms(UPDATE_CHECKS, NULL);
    check->current_sub = check->FirstOpenSubCheck();

    if (is_bar_tab)
    {
        is_bar_tab = 0;
        Jump(JUMP_HOME);
    }
    else
    {
        Settings *settings = GetSettings();
        switch (type)
        {
        case TERMINAL_BAR:
            /** fall through **/
        case TERMINAL_BAR2:
            if (Jump(JUMP_NORMAL, PAGEID_LOGIN))
                ReportError("Couldn't jump to Login page");
            break;
        case TERMINAL_FASTFOOD:
            if (FindDrawer() == NULL &&
                (user == NULL || user->training == 0))
                jump_target = -1;        
            if (Jump(JUMP_NORMAL, jump_target))
            {
                snprintf(str, STRLENGTH, "Couldn't jump to page %d", PAGE_ID_SETTLEMENT);
                ReportError(str);
            }
            break;
        default:
            timeout = settings->delay_time2;  // super short timeout
            Jump(JUMP_HOME);
            break;
        }
    }
    return 0;
}

const genericChar* Terminal::PageNo(int current, int page_max, int lang)
{
    FnTrace("Terminal::PageNo()");
    static genericChar buffer[32];
    return MasterLocale->Page(current, page_max, lang, buffer);
}

const genericChar* Terminal::UserName(int user_id)
{
    FnTrace("Terminal::UserName(int)");
    Employee *e = system_data->user_db.FindByID(user_id);
    if (e)
        return e->system_name.Value();
    else
        return Translate(UnknownStr);
}

genericChar* Terminal::UserName(genericChar* str, int user_id)
{
    FnTrace("Terminal::UserName(str, int)");
    strcpy(str, UserName(user_id));
    return str;
}

genericChar* Terminal::FormatPrice(int price, int sign)
{
    FnTrace("Terminal::FormatPrice(int, int)");
    return PriceFormat(GetSettings(), price, sign, 1);
}

genericChar* Terminal::FormatPrice(genericChar* str, int price, int sign)
{
    FnTrace("Terminal::FormatPrice(str, int, int)");
    return PriceFormat(GetSettings(), price, sign, 1, str);
}

genericChar* Terminal::SimpleFormatPrice(int price)
{
    FnTrace("Terminal::SimpleFormatPrice(int)");
    return PriceFormat(GetSettings(), price, 0, 0);
}

genericChar* Terminal::SimpleFormatPrice(genericChar* str, int price)
{
    FnTrace("Terminal::SimpleFormatPrice(str, int)");
    return PriceFormat(GetSettings(), price, 0, 0, str);
}

int Terminal::PriceToInteger(const genericChar* price)
{
    FnTrace("Terminal::PriceToInteger()");
    int intprice = 0;
    genericChar buffer[STRLENGTH];
    int bidx = 0;
    int pidx = 0;
    
    while (price[pidx] != '\0')
    {
        if (isdigit(price[pidx]))
        {
            buffer[bidx] = price[pidx];
            bidx += 1;
        }
        pidx += 1;
    }
    buffer[bidx] = '\0';
    intprice = atoi(buffer);
    return intprice;
}

const genericChar* Terminal::Translate(const genericChar* str, int lang, int clear)
{
    FnTrace("Terminal::Translate()");
    return MasterLocale->Translate(str, lang, clear);
}

const genericChar* Terminal::TimeDate(TimeInfo &timevar, int format, int lang)
{
    FnTrace("Terminal::TimeDate(timeinfo, int, int)");
    return MasterLocale->TimeDate(GetSettings(), timevar, format, lang);
}

const genericChar* Terminal::TimeDate(genericChar* buffer, TimeInfo &timevar, int format, int lang)
{
    FnTrace("Terminal::TimeDate(char, timeinfo, int, int)");
    return MasterLocale->TimeDate(GetSettings(), timevar, format, lang, buffer);
}

int Terminal::UserInput()
{
    FnTrace("Terminal::UserInput()");
    time_out   = SystemTime;
    last_input = SystemTime;
    return 0;
}

int Terminal::ClearSelectedZone()
{
    FnTrace("Terminal::ClearSelectedZone()");
    if (redraw_id)
    {
        RemoveTimeOutFn(redraw_id);
        redraw_id = 0;
    }

    Zone *z = selected_zone;
    if (z)
    {
        selected_zone = NULL;
        z->Draw(this, 0);
    }
    return 0;
}

int Terminal::DrawTitleBar()
{
    FnTrace("Terminal::DrawTitleBar()");
    if (page)
    {
        if (!edit && !record_activity)
        {
            WInt8(TERM_TITLEBAR);
            WStr(TimeDate(SystemTime, TD0));
        }
        Draw(0, 0, 0, page->width, TITLE_HEIGHT);
    }
    return 0;
}

int Terminal::RenderBlankPage()
{
    FnTrace("Terminal::RenderBlankPage()");
    if (page == NULL)
        return 1;

    int mode = MODE_NONE;
    if (system_data->LicenseExpired())
        mode = MODE_EXPIRED;
    else if (record_activity)
        mode = MODE_MACRO;
    else if (edit)
        mode = MODE_EDIT;
    else if (translate)
        mode = MODE_TRANSLATE;
    else if (user && user->training)
        mode = MODE_TRAINING;

    WInt8(TERM_BLANKPAGE);
    WInt8(mode);
    WInt8(page->image);
    WInt8(page->title_color);
    WInt8(page->size);

    if (page->IsTable())
    {
        if (page->size == SIZE_640x480 ||
            page->size == SIZE_800x600 
      //    || page->size == SIZE_800x480
            )
        {
            WInt16(160);
        }
        else
            WInt16(204);
        if (page->IsTable() &&
            (last_page_type == PAGE_TABLE || last_page_type == PAGE_TABLE2) &&
            page->size == last_page_size)
        {
            WInt8(1);
        }
        else
        {
            WInt8(0);
        }
    }
    else
    {
        WInt16(0);
        WInt8(0);
    }

    last_page_type = page->type;
    last_page_size = page->size;

    // FIX - 
    const genericChar* pn = Translate(ReplaceSymbols(page->name.Value()));
    genericChar str[STRLENGTH];
    if (edit)
    {
        if (user && (page->id >= 0 || user->CanEditSystem()))
        {
            int count = 0;
            int list[6], ref = zone_db->References(page, list, 6, count);
            genericChar ref_list[STRLENGTH] = "";
            if (ref > 0)
            {
                int i = 0;
                while (i < 6 && i < ref)
                {
                    if (i == 0)
                        sprintf(str, ": %d", list[i]);
                    else
                        sprintf(str, ",%d", list[i]);
                    strcat(ref_list, str);
                    ++i;
                }
                if (ref > 6)
                    strcat(ref_list, "...");
            }

            sprintf(str, "%d %s (refs %d%s)", page->id, pn, count, ref_list);
            WStr(str);

            count = 0;
            int pt = page->type;
            int s1 = CompareList(pt, PageTypeValue, 0);
            int s2 = CompareList(page->index, IndexValue, 0);
            Zone *z = page->ZoneList();

            while (z)
            {
                ++count;
                z = z->next;
            }

            if ( pt == PAGE_INDEX )
                sprintf(str, "%-13s  %-14s  %2d", PageTypeName[s1],
                        IndexName[s2], count);
            else
                sprintf(str, "%s  %2d", PageTypeName[s1], count);
            WStr(str);
        }
        else
        {
            WStr(pn);
            WStr(Translate("System Page - Can't Edit"));
        }
    }
    else
    {
        WStr(pn);
        WStr(TimeDate(SystemTime, TD0));
    }

    return Send();
}

int Terminal::RenderBackground()
{
    FnTrace("Terminal::RenderBackground()");
    WInt8(TERM_BACKGROUND);
    return Send();
}

int Terminal::RenderText(const genericChar* str, int x, int y, int color, int font,
                         int align, int max_pixel_width, int mode)
{
    FnTrace("Terminal::RenderText()");
    if (str == NULL)
        return 1;
    int len = strlen(str);
    if (len <= 0)
        return 1;

    if (color == COLOR_PAGE_DEFAULT || color == COLOR_DEFAULT)
        color = page->default_color[0];
    if (color == COLOR_CLEAR || str == NULL || len <= 0)
        return 0;

    if (font == FONT_DEFAULT)
        font = page->default_font;
    if (mode & PRINT_BOLD)
    {
        switch (font)
        {
        case FONT_HELV_14:  font = FONT_HELV_14B; break;
        case FONT_HELV_18:  font = FONT_HELV_18B; break;
        case FONT_HELV_20:  font = FONT_HELV_20B; break;
        case FONT_HELV_24:  font = FONT_HELV_24B; break;
        case FONT_HELV_34:  font = FONT_HELV_34B; break;
        case FONT_HELV_14B: font = FONT_HELV_14;  break;
        case FONT_HELV_18B: font = FONT_HELV_18;  break;
        case FONT_HELV_20B: font = FONT_HELV_20;  break;
        case FONT_HELV_24B: font = FONT_HELV_24;  break;
        case FONT_HELV_34B: font = FONT_HELV_34;  break;
        }
    }
    if (mode & PRINT_UNDERLINE)
        font |= FONT_UNDERLINE;

    if (align == ALIGN_LEFT)
        WInt8(TERM_TEXTL);
    else if (align == ALIGN_CENTER)
        WInt8(TERM_TEXTC);
    else
        WInt8(TERM_TEXTR);

    WStr(str, len);
    WInt16(x);
    WInt16(y);
    WInt8(color);
    WInt8(font);
    WInt16(max_pixel_width);
    return Send();
}

int Terminal::RenderTextLen(const genericChar* str, int len, int x, int y, int color,
                            int font, int align, int max_pixel_width, int mode)
{
    FnTrace("Terminal::RenderTextLen()");
    if (str == NULL || len <= 0)
        return 1;

    if (color == COLOR_PAGE_DEFAULT || color == COLOR_DEFAULT)
        color = page->default_color[0];
    if (color == COLOR_CLEAR || str == NULL || len <= 0)
        return 0;

    if (font == FONT_DEFAULT)
        font = page->default_font;
    if (mode & PRINT_BOLD)
    {
        switch (font)
        {
        case FONT_HELV_14:  font = FONT_HELV_14B; break;
        case FONT_HELV_18:  font = FONT_HELV_18B; break;
        case FONT_HELV_20:  font = FONT_HELV_20B; break;
        case FONT_HELV_24:  font = FONT_HELV_24B; break;
        case FONT_HELV_34:  font = FONT_HELV_34B; break;
        case FONT_HELV_14B: font = FONT_HELV_14B; break;
        case FONT_HELV_18B: font = FONT_HELV_18;  break;
        case FONT_HELV_20B: font = FONT_HELV_20;  break;
        case FONT_HELV_24B: font = FONT_HELV_24;  break;
        case FONT_HELV_34B: font = FONT_HELV_34;  break;
        }
    }
    if (mode & PRINT_UNDERLINE)
        font |= FONT_UNDERLINE;

    if (align == ALIGN_LEFT)
        WInt8(TERM_TEXTL);
    else if (align == ALIGN_CENTER)
        WInt8(TERM_TEXTC);
    else
        WInt8(TERM_TEXTR);

    WStr(str, len);
    WInt16(x);
    WInt16(y);
    WInt8(color);
    WInt8(font);
    WInt16(max_pixel_width);
    return Send();
}

int Terminal::RenderZoneText(const genericChar* str, int x, int y, int w, int h, int color, int font)
{
    FnTrace("Terminal::RenderZoneText");
    if (w <= 0 || h <= 0 || str == NULL)
        return 0;
    if (color == COLOR_PAGE_DEFAULT || color == COLOR_DEFAULT)
        color = page->default_color[0];
    if (color == COLOR_CLEAR)
        return 0;

    if (font == FONT_DEFAULT)
        font = page->default_font;

    WInt8(TERM_ZONETEXTC);
    WStr(str);
    WInt16(x);
    WInt16(y);
    WInt16(w);
    WInt16(h);
    WInt8(color);
    WInt8(font);
    return Send();
}

int Terminal::RenderHLine(int x, int y, int len, int color, int lw)
{
    FnTrace("Terminal::RenderHLine()");
    if (lw <= 0 || len <= 0)
        return 1;
    if (color == COLOR_PAGE_DEFAULT || color == COLOR_DEFAULT)
    {
        color = page->default_color[0];
        if (color == COLOR_DEFAULT)
            color = COLOR_WHITE;
    }
    if (color == COLOR_CLEAR)
        return 0;

    WInt8(TERM_HLINE);
    WInt16(x);
    WInt16(y);
    WInt16(len);
    WInt8(lw);
    WInt8(color);
    return Send();
}

int Terminal::RenderVLine(int x, int y, int len, int color, int lw)
{
    FnTrace("Terminal::RenderVLine()");
    if (lw <= 0 || len <= 0)
        return 1;
    if (color == COLOR_PAGE_DEFAULT || color == COLOR_DEFAULT)
    {
        color = page->default_color[0];
        if (color == COLOR_DEFAULT)
            color = COLOR_WHITE;
    }
    if (color == COLOR_CLEAR)
        return 0;

    WInt8(TERM_VLINE);
    WInt16(x);
    WInt16(y);
    WInt16(len);
    WInt8(lw);
    WInt8(color);
    return Send();
}

int Terminal::RenderRectangle(int x, int y, int w, int h, int image)
{
    FnTrace("Terminal::RenderRectangle()");
    if (w <= 0 || h <= 0)
        return 0;

    WInt8(TERM_RECTANGLE);
    WInt16(x);
    WInt16(y);
    WInt16(w);
    WInt16(h);
    WInt8(image);
    return Send();
}

int Terminal::RenderFrame(int x, int y, int w, int h, int fw, int flags)
{
    FnTrace("Terminal::RenderFrame()");
    if (w <= 0 || h <= 0 || fw <= 0)
        return 0;

    WInt8(TERM_FRAME);
    WInt16(x);
    WInt16(y);
    WInt16(w);
    WInt16(h);
    WInt8(fw);
    WInt8(flags);
    return Send();
}

int Terminal::RenderFilledFrame(int x, int y, int w, int h, int fw,
                                int texture, int flags)
{
    FnTrace("Terminal::RenderFilledFrame()");
    if (w <= 0 || h <= 0)
        return 0;

    WInt8(TERM_FILLEDFRAME);
    WInt16(x);
    WInt16(y);
    WInt16(w);
    WInt16(h);
    WInt8(fw);
    WInt8(texture);
    WInt8(flags);
    return Send();
}

int Terminal::RenderStatusBar(Zone *z, int bar_color, const genericChar* text,
                              int text_color)
{
    FnTrace("Terminal::RenderStatusBar()");
    WInt8(TERM_STATUSBAR);
    WInt16(z->x + z->border);
    WInt16(z->y + z->h - z->border - 22);
    WInt16(z->w - (z->border * 2));
    WInt16(24);
    WInt8(bar_color);
    WStr(text);
    WInt8(FONT_HELV_20);
    WInt8(text_color);
    return Send();
}

int Terminal::RenderZone(Zone *z)
{
    FnTrace("Terminal::RenderZone()");
    int state = z->State(this);
    int zf = z->frame[state];
    int zt = z->texture[state];

    if (z == selected_zone && !z->stay_lit)
    {
        if (z->behave == BEHAVE_BLINK)
            redraw_id = AddTimeOutFn((TimeOutFn) RedrawZoneCB, 500, this);
        else if (z->behave == BEHAVE_DOUBLE)
            redraw_id = AddTimeOutFn((TimeOutFn) RedrawZoneCB, 1000, this);
    }

    if (zf == ZF_DEFAULT)
    {
        zf = page->default_frame[state];
        if (zf == ZF_DEFAULT)
            zf = ZF_RAISED;
    }
    if (zt == IMAGE_DEFAULT)
    {
        zt = page->default_texture[state];
        if (zt == IMAGE_DEFAULT)
            zt = IMAGE_SAND;
    }

    if (zf == ZF_HIDDEN || (zf == ZF_NONE && zt == IMAGE_CLEAR))
        return 0;

    WInt8(TERM_ZONE);
    WInt16(z->x);
    WInt16(z->y);
    WInt16(z->w);
    WInt16(z->h);
    WInt8(zf);
    WInt8(zt);
    WInt8(z->shape);
    return Send();
}

int Terminal::RedrawZone(Zone *z, int timeint)
{
    FnTrace("Terminal::RedrawZone()");
    if (redraw_id)
        RemoveTimeOutFn(redraw_id);

    selected_zone = z;
    redraw_id = AddTimeOutFn((TimeOutFn) RedrawZoneCB, timeint, this);
    return 0;
}

int Terminal::RenderEditCursor(int x, int y, int w, int h)
{
    FnTrace("Terminal::RenderEditCursor()");
    WInt8(TERM_EDITCURSOR);
    WInt16(x);
    WInt16(y);
    WInt16(w);
    WInt16(h);
    return Send();
}

int Terminal::RenderButton(int x, int y, int w, int h,
                           int frame, int texture, int shape)
{
    FnTrace("Terminal::RenderButton()");

    if (w <= 0 || h <= 0)
        return 0;

    if (frame == ZF_DEFAULT)
        frame = page->default_frame[0];
    if (texture == IMAGE_DEFAULT)
        texture = page->default_texture[0];
    if (frame == ZF_HIDDEN || frame == ZF_DEFAULT || texture == IMAGE_DEFAULT)
        return 0;

    WInt8(TERM_ZONE);
    WInt16(x);
    WInt16(y);
    WInt16(w);
    WInt16(h);
    WInt8(frame);
    WInt8(texture);
    WInt8(shape);
    return Send();
}

int Terminal::RenderShadow(int x, int y, int w, int h, int shade, int shape)
{
    FnTrace("Terminal::RenderShadow()");
    if (w <= 0 || h <= 0)
        return 0;

    if (shade < 0 || shade > 255)
        shade = page->default_shadow;

    if (shade <= 0 || shade > 255)
        return 1;

    WInt8(TERM_SHADOW);
    WInt16(x);
    WInt16(y);
    WInt16(w);
    WInt16(h);
    WInt8(shade);
    WInt8(shape);

    return Send();
}

int Terminal::UpdateAll()
{
    FnTrace("Terminal::UpdateAll()");
    WInt8(TERM_UPDATEALL);
    return SendNow();
}

int Terminal::UpdateArea(int x, int y, int w, int h)
{
    FnTrace("Terminal::UpdateArea()");
    if (w <= 0 || h <= 0)
        return 0;

    WInt8(TERM_UPDATEAREA);
    WInt16(x);
    WInt16(y);
    WInt16(w);
    WInt16(h);
    return SendNow();
}

int Terminal::SetClip(int x, int y, int w, int h)
{
    FnTrace("Terminal::SetClip()");
    WInt8(TERM_SETCLIP);
    WInt16(x);
    WInt16(y);
    WInt16(w);
    WInt16(h);
    return Send();
}

int Terminal::SetCursor(int cursor_type)
{
    FnTrace("Terminal::SetCursor()");
    WInt8(TERM_CURSOR);
    WInt16(cursor_type);
    return SendNow();
}

int Terminal::Bell()
{
    FnTrace("Terminal::Bell()");
    WInt8(TERM_BELL);
    WInt16(-90);
    return SendNow();
}

int Terminal::CalibrateTS()
{
    FnTrace("Terminal::CalibrateTS()");
    WInt8(TERM_CALIBRATE_TS);
    return SendNow();
}

int Terminal::SetMessage(const genericChar* message)
{
    FnTrace("Terminal::SetMessage()");
    if (message == NULL)
        return 1;

    message_set = 1;
    WInt8(TERM_SETMESSAGE);
    WStr(message);
    DrawTitleBar();
    return 0;
}

int Terminal::ClearMessage()
{
    FnTrace("Terminal::ClearMessage()");
    if (message_set == 0)
        return 0;

    message_set = 0;
    WInt8(TERM_CLEARMESSAGE);
    DrawTitleBar();
    return 0;
}

int Terminal::SetIconify(int iconify)
{
    FnTrace("Terminal::SetIconify()");
    WInt8(TERM_SET_ICONIFY);
    WInt8(iconify);
    return 0;
}

int Terminal::WInt8(int val)
{
    FnTrace("Terminal::WInt8(int)");
    return buffer_out->Put8(val);
}

int Terminal::WInt8(int *val)
{
    FnTrace("Terminal::WInt8(int *)");
    if (val == NULL)
        return WInt8(0);
    else
        return WInt8(*val);
}

int Terminal::RInt8(int *val)
{
    FnTrace("Terminal::RInt8(int *)");
    int v = buffer_in->Get8();

    if (val)
        *val = v;

    return v;
}

int Terminal::WInt16(int val)
{
    FnTrace("Terminal::WInt16(int)");
    return buffer_out->Put16(val);
}

int Terminal::WInt16(int *val)
{
    FnTrace("Terminal::WInt16(int *)");
    if (val == NULL)
        return WInt16(0);
    else
        return WInt16(*val);
}

int Terminal::RInt16(int *val)
{
    FnTrace("Terminal::RInt16(int *)");
    int v = buffer_in->Get16();
    if (val)
        *val = v;
    return v;
}

int Terminal::WInt32(int val)
{
    FnTrace("Terminal::WInt32(int)");
    return buffer_out->Put32(val);
}

int Terminal::WInt32(int *val)
{
    FnTrace("Terminal::WInt32(int *)"); 
    if (val == NULL)
        return WInt32(0);
    else
        return WInt32(*val);
}

int Terminal::RInt32(int *val)
{
    FnTrace("Terminal::RInt32(int *)");
    int v = buffer_in->Get32();
    if (val)
        *val = v;
    return v;
}

long Terminal::WLong(long val)
{
    FnTrace("Terminal::WLong(long)");

    return buffer_out->PutLong(val);
}

long Terminal::WLong(long *val)
{
    FnTrace("Terminal::WLong(long *)");

    if (val == NULL)
        return WLong((long)0);
    else
        return WLong(*val);
}

long Terminal::RLong(long *val)
{
    FnTrace("Terminal::RLong(long *)");

    long v = buffer_in->GetLong();
    if (val)
        *val = v;
    return v;
}

long long Terminal::WLLong(long long val)
{
    FnTrace("Terminal::WLLong(long long)");

    return buffer_out->PutLLong(val);
}

long long Terminal::WLLong(long long *val)
{
    FnTrace("Terminal::WLLong(long long *)");

    if (val == NULL)
        return WLLong((long long)0);
    else
        return WLLong(*val);
    
}

long long Terminal::RLLong(long long *val)
{
    FnTrace("Terminal::RLLong()");

    long v = buffer_in->GetLLong();
    if (val)
        *val = v;
    return v;
}

int Terminal::WFlt(Flt val)
{
    FnTrace("Terminal::WFlt(flt)");
    return buffer_out->Put32((int) (val * 100.0));
}

int Terminal::WFlt(Flt *val)
{
    FnTrace("Terminal::WFlt(flt *)");
    if (val == NULL)
        return WFlt(0.0);
    else
        return WFlt(*val);
}

Flt Terminal::RFlt(Flt *val)
{
    FnTrace("Terminal::RFlt()");
    int v = buffer_in->Get32();
    Flt f = (Flt) v / 100.0;
    if (val)
        *val = f;
    return f;
}

int Terminal::WStr(const genericChar* s, int len)
{
    FnTrace("Terminal::WStr(const char* , len)");
    if (s == NULL)
        return buffer_out->PutString("", 0);
    else
        return buffer_out->PutString(s, len);
}

int Terminal::WStr(Str *s)
{
    FnTrace("Terminal::WStr(str)");
    if (s == NULL)
        return buffer_out->PutString("", 0);
    else
        return buffer_out->PutString(s->Value(), s->length);
}

genericChar* Terminal::RStr(genericChar* s)
{
    FnTrace("Terminal::RStr(const char* )");
    static genericChar buffer[1024];
    if (s == NULL)
        s = buffer;

    if (buffer_in->GetString(s))
    {
        s[0] = '\0';
        s = NULL;
    }

    return s;
}

genericChar* Terminal::RStr(Str *s)
{
    FnTrace("Terminal::RStr(str)");
    static genericChar str[1024] = "";
    RStr(str);
    if (s)
        s->Set(str);
    return str;
}

int Terminal::Send()
{
    FnTrace("Terminal::Send()");
    if (buffer_out->size <= buffer_out->send_size)
        return 0;

    return buffer_out->Write(socket_no);
}

/****
 * SendNow:  Returns the result of a final write(), so -1 on error,
 *  number of bytes written otherwise.
 ****/
int Terminal::SendNow()
{
    FnTrace("Terminal::SendNow()");
    Terminal *currterm = clone_list.Head();

    while (currterm != NULL)
    {
        buffer_out->Write(currterm->socket_no, 0);
        currterm = currterm->next;
    }

    return buffer_out->Write(socket_no);
}

#define MOVE_RIGHT  5
#define MOVE_LEFT  -5
#define MOVE_DOWN   5
#define MOVE_UP    -5
int Terminal::KeyboardInput(genericChar key, int my_code, int state)
{
    FnTrace("Terminal::KeyboardInput()");
    time_out   = SystemTime;
    last_input = SystemTime;
    int edit_system = 0;
    int edit_user   = 0;
    SimpleDialog *sd = NULL;

    switch (my_code)
    {
    case XK_F1:  // edit mode
        if (state & ShiftMask)
            return EditTerm(0);  // Exit edit without saving, if we're in edit mode
        else
            return EditTerm(1);  // EditTerm defaults to 1 anyway
    case XK_F2:  // translate term
        return TranslateTerm();
    case XK_F3:  // record activity
        if (debug_mode)
        {
            if (record_activity)
            {
                WInt8(TERM_TITLEBAR);
                WStr(TimeDate(SystemTime, TD0));
                Draw(0, 0, 0, page->width, TITLE_HEIGHT);
                close(record_fd);
                record_fd = -1;
                record_activity = 0;
                Draw(RENDER_NEW);
            }
            else if (OpenRecordFile() == 0)
            {
                record_activity = 1;
                Draw(RENDER_NEW);
            }
        }
        return 0;
    case XK_F4:
        if (debug_mode)
            ReadRecordFile();
        return 0;
    case XK_F6:
        if (debug_mode)
            Signal("adminforceauth1", 0);
        return 0;
    case XK_F7:
        if (user != NULL && page != NULL && edit == 0)
        {
            edit_system = (page->id < 0 && user->CanEditSystem());
            edit_user   = (page->id >= 0 && user->CanEdit());
            if (edit_system || edit_user)
                MasterControl->zone_db->ExportPage(page);
        }
        else if (edit)
        {
            sd = new SimpleDialog("Cannot export pages while in edit mode.");
            sd->Button("Okay");
            OpenDialog(sd);
        }
        return 0;
    case XK_F11:  // exit edit without save
        return EditTerm(0);
    }

    if (edit || translate)
    {
        switch (my_code)
        {
        case XK_Page_Up:
            ForePage(); return 0;
        case XK_Page_Down:
            NextPage(); return 0;
        }
     }
    if (edit == 0)
    {
        // fudge:  convert XK_ISO_Left_Tab to standard Tab.
        // state should reflect shift status.  I don't know that
        // this will ever cause problems, but if someone wants
        // left tab, we won't be sending it.  Of course, we weren't
        // sending it anyway.
        if (my_code == XK_ISO_Left_Tab)
            key = '\t';
        if (key != '\0')
            return Keyboard(key, state);
        else
            return 0;
    }

    switch (my_code)
    {
    case XK_Up:
    case XK_KP_8:
        if (state & ControlMask)
            zone_db->CopyEdit(this, 0, MOVE_UP);
        else
            zone_db->PositionEdit(this, 0, -grid_y);
        break;
    case XK_KP_9:
        if (state & ControlMask)
            zone_db->CopyEdit(this, MOVE_RIGHT, MOVE_UP);
        else
            zone_db->PositionEdit(this, grid_x, -grid_y);
        break;
    case XK_Right:
    case XK_KP_6:
        if (state & ControlMask)
            zone_db->CopyEdit(this, MOVE_RIGHT, 0);
        else
            zone_db->PositionEdit(this, grid_x, 0);
        break;
    case XK_KP_3:
        if (state & ControlMask)
            zone_db->CopyEdit(this, MOVE_RIGHT, MOVE_DOWN);
        else
            zone_db->PositionEdit(this, grid_x, grid_y);
        break;
    case XK_Down:
    case XK_KP_2:
        if (state & ControlMask)
            zone_db->CopyEdit(this, 0, MOVE_DOWN);
        else 
            zone_db->PositionEdit(this, 0, grid_y);
        break;
    case XK_KP_1:
        if (state & ControlMask)
            zone_db->CopyEdit(this, MOVE_LEFT, MOVE_DOWN);
        else
            zone_db->PositionEdit(this, -grid_x, grid_y);
        break;
    case XK_Left:
    case XK_KP_4:
        if (state & ControlMask)
            zone_db->CopyEdit(this, MOVE_LEFT, 0);
        else
            zone_db->PositionEdit(this, -grid_x, 0);
        break;
    case XK_KP_7:
        if (state & ControlMask)
            zone_db->CopyEdit(this, MOVE_LEFT, MOVE_UP);
        else
            zone_db->PositionEdit(this, -grid_x, -grid_y);
        break;
    case XK_w:
        zone_db->SizeEdit(this, grid_x, 0);
        break;
    case XK_W:
        zone_db->SizeEdit(this, -grid_x, 0);
        break;
    case XK_h:
        zone_db->SizeEdit(this, 0, grid_y);
        break;
    case XK_H:
        zone_db->SizeEdit(this, 0, -grid_y);
        break;
    case XK_r:
        SizeToMouse();
        break;
    case XK_a: // toggle all
        ButtonCommand(WB_TOGGLE);
        break;
    case XK_A: // mark all
        ButtonCommand(WB_ALL);
        break;
    case XK_c: // copy zones
    case XK_C:
        zone_db->CopyEdit(this);
        break;
    case XK_d: // delete zone(s)
    case XK_D:
        zone_db->DeleteEdit(this);
        break;
    case XK_i: // cycle info mode
    case XK_I:
        show_info ^= 1;
        Draw(0);
        break;
    case XK_j: // open jump list
    case XK_J:
        ShowPageList();
        break;
    case XK_m: // move zones
    case XK_M:
        zone_db->RelocateEdit(this);
        break;
    case XK_n: // new item zone
    case XK_N:
        EditZone(NULL);
        break;
    case XK_p: // new page
    case XK_P:
        EditPage(NULL);
        break;
    }
    return 0;
}

int Terminal::MouseInput(int action, int x, int y)
{
    FnTrace("Terminal::MouseInput()");
    time_out   = SystemTime;
    last_input = SystemTime;

    Zone *zone = NULL;
    if (translate)
    {
        zone = page->FindTranslateZone(this, x, y);
        if ((action & MOUSE_RIGHT) && (action & MOUSE_PRESS))
        {
            if (zone == NULL && y < 32)
                return TranslatePage(page);
            else
                return TranslateZone(zone);
        }
    }

    if (edit == 0)
    {
        return Mouse(action, x, y);
    }

    // Keep track of the current mouse position in edit mode.  This can
    // be used later for resizing and whatnot.
    mouse_x = x;
    mouse_y = y;

    if ((action & MOUSE_PRESS) && user)
    {
        zone = page->FindEditZone(this, x, y);
        last_x = x - (x % grid_x);
        last_y = y - (y % grid_y);
        zone_modify = 0;
        if (zone == NULL)
        {
            if (select_on)
                WInt8(TERM_SELECTOFF);
            WInt8(TERM_SELECTUPDATE);
            WInt16(x); WInt16(y);
            SendNow();
            select_x1 = x;
            select_y1 = y;
            select_x2 = x;
            select_y2 = y;
            select_on = 1;
        }
        else
            select_on = 0;

        if (action & MOUSE_RIGHT)
        {
            if (zone)
            {
                if (zone->edit == 0)
                {
                    if ((action & MOUSE_SHIFT) == 0)
                        zone_db->ClearEdit(this);
                    zone->edit = 1;
                    zone->Draw(this, 0);
                }
                EditMultiZone(page);
            }
            else if (y <= 32 && x >= 0 && x < page->width)
                EditPage(page);

            return 0;
        }

        if ((action & MOUSE_SHIFT) == 0 && (zone == NULL || zone->edit == 0))
            zone_db->ClearEdit(this);

        if (zone == NULL)
            return 0;

        if (action & MOUSE_LEFT)
        {
            if (y < (zone->y + GRAB_EDGE) || x < (zone->x + GRAB_EDGE) ||
                y > (zone->y + zone->h - GRAB_EDGE) || x > (zone->x + zone->w - GRAB_EDGE))
                zone_modify = MODIFY_MOVE;
        }
        else if (action & MOUSE_MIDDLE)
        {
            if (y < (zone->y + GRAB_EDGE))
                zone_modify = MODIFY_RESIZE_TE;
            else if (y > (zone->y + zone->h - GRAB_EDGE))
                zone_modify = MODIFY_RESIZE_BE;

            if (x < (zone->x + GRAB_EDGE))
                zone_modify |= MODIFY_RESIZE_LE;
            else if (x > (zone->x + zone->w - GRAB_EDGE))
                zone_modify |= MODIFY_RESIZE_RE;
        }

        if (action & MOUSE_SHIFT)
            zone->edit ^= 1;
        else
            zone->edit = 1;
        zone->Draw(this, 0);
        if (zone->edit == 0)
            zone_modify = 0;
    }
    else if (action & MOUSE_DRAG)
    {
        if (select_on)
        {
            WInt8(TERM_SELECTUPDATE);
            WInt16(x); WInt16(y);
            SendNow();
            select_x2 = x;
            select_y2 = y;
        }

        if (zone_modify == 0)
            return 0;

        int current_x = x - (x % grid_x);
        int current_y = y - (y % grid_y);
        int dir_x = current_x - last_x;
        int dir_y = current_y - last_y;
        last_x = current_x;
        last_y = current_y;

        if (zone_modify & MODIFY_MOVE)
            zone_db->PositionEdit(this, dir_x, dir_y);
        else
        {
            int dw = 0, dh = 0, move_x = 0, move_y = 0;
            if (zone_modify & MODIFY_RESIZE_BE)
                dh = dir_y;
            else if (zone_modify & MODIFY_RESIZE_TE)
            {
                dh = -dir_y;
                move_y = 1;
            }

            if (zone_modify & MODIFY_RESIZE_RE)
                dw = dir_x;
            else if (zone_modify & MODIFY_RESIZE_LE)
            {
                dw = -dir_x;
                move_x = 1;
            }
            zone_db->SizeEdit(this, dw, dh, move_x, move_y);
        }
    }
    else if (action & MOUSE_RELEASE)
    {
        int rx = Min(select_x1, select_x2);
        int ry = Min(select_y1, select_y2);
        int rw = Abs(select_x1 - select_x2);
        int rh = Abs(select_y1 - select_y2);
        zone_db->ToggleEdit(this, (action & MOUSE_SHIFT), rx, ry, rw, rh);
        select_on = 0;
        select_x1 = 0;
        select_y1 = 0;
        select_x2 = 0;
        select_y2 = 0;
    }
    return 0;
}

/****
 * MouseToolbar:  Handle mouse activity in the toolbar window.
 *  We don't do much here, but in the event that the keyboard is shot,
 *  it would still be nice to get out of edit mode, thus saving any
 *  changes that have been made.
 ****/
int Terminal::MouseToolbar(int action, int x, int y)
{
    FnTrace("Terminal::MouseToolbar()");
    if ((action & MOUSE_MIDDLE) && (action & MOUSE_SHIFT))
    {
        EditTerm();
        EndSystem();
    }
    else if (action & MOUSE_RIGHT)
    {
        return EditTerm();
    }
    return 0;
}

int Terminal::ButtonCommand(int command)
{
    FnTrace("Terminal::ButtonCommand()");

    switch (command)
    {
    case WB_ICONIFY:
        WInt8(TERM_ICONIFY);
        SendNow();
        break;
    }

    if (edit == 0)
        return 0;

    switch (command)
    {
    case WB_NEWZONE:
        EditZone(NULL);
        break;
    case WB_NEWPAGE:
        EditPage(NULL);
        break;
    case WB_ALL:
        zone_db->ToggleEdit(this, 0);
        break;
    case WB_TOGGLE:
        zone_db->ToggleEdit(this, 1);
        break;
    case WB_COPY:
        zone_db->CopyEdit(this);
        break;
    case WB_MOVE:
        zone_db->RelocateEdit(this);
        break;
    case WB_PRIOR:
        ForePage();
        break;
    case WB_NEXT:
        NextPage();
        break;
    case WB_INFO:
        show_info ^= 1;
        Draw(0);
        break;
    case WB_LIST:
        ShowPageList();
        break;
    case WB_PRINTLIST:
    {
        Printer *printer = FindPrinter(PRINTER_RECEIPT);
        if (printer)
        {
            Report r;
            zone_db->PageListReport(this, user->CanEditSystem(), &r);
            r.CreateHeader(this, printer, user);
            r.FormalPrint(printer);
        }
    }
    break;
    }
    return 0;
}

int Terminal::SizeToMouse()
{
    FnTrace("Terminal::SizeToMouse()");
    int retval = 1;
    Zone *currzone = NULL;
    Zone *sizezone = NULL;
    Page *currpage = NULL;
    int count = 0;

    // won't resize if we have more than one selected zone
    currpage = page;
    while (currpage && count < 2)
    {
        currzone = currpage->ZoneList();
        while (currzone && count < 2)
        {
            if (currzone->edit)
            {
                sizezone = currzone;
                count += 1;
            }
            currzone = currzone->next;
        }
        currpage = currpage->next;
    }

    if (count == 1 && sizezone != NULL)
    {
        int x1 = sizezone->x;
        int x2 = sizezone->x + sizezone->w;
        int y1 = sizezone->y;
        int y2 = sizezone->y + sizezone->h;
        int change_w = 0;
        int change_h = 0;
        int move_x = 0;
        int move_y = 0;

        if (mouse_x < x1)
        {
            change_w = x1 - mouse_x;
            move_x = 1;
        }
        else
        {
            change_w = mouse_x - x2;
        }

        if (mouse_y < y1)
        {
            change_h = y1 - mouse_y;
            move_y = 1;
        }
        else
        {
            change_h = mouse_y - y2;
        }

        zone_db->SizeEdit(this, change_w, change_h, move_x, move_y);

        retval = 0;
    }

    return retval;
}

int Terminal::EditMultiZone(Page *currPage)
{
    FnTrace("Terminal::EditMultiZone()");
    if (currPage == NULL || user == NULL)
        return 1;

    int behave = 1;
    int font = 1;
    int shape = 1;
    int shadow = 1;
    int frame1 = 1;
    int frame2 = 1;
    int tex1 = 1;
    int tex2 = 1;
    int color1 = 1;
    int color2 = 1;
    int count = 0;

    Zone *last = NULL;
    while (currPage)
    {
        for (Zone *currZone = currPage->ZoneList(); currZone != NULL; currZone = currZone->next)
            if (currZone->edit && currZone->CanEdit(this))
            {
                if (last)
                {
                    if (currZone->behave != last->behave)         behave = 0;
                    if (currZone->font != last->font)             font = 0;
                    if (currZone->shape != last->shape)           shape = 0;
                    if (currZone->shadow != last->shadow)         shadow = 0;
                    if (currZone->frame[0] != last->frame[0])     frame1 = 0;
                    if (currZone->texture[0] != last->texture[0]) tex1 = 0;
                    if (currZone->color[0] != last->color[0])     color1 = 0;
                    if (currZone->frame[1] != last->frame[1])     frame2 = 0;
                    if (currZone->texture[1] != last->texture[1]) tex2 = 0;
                    if (currZone->color[1] != last->color[1])     color2 = 0;
                }
                last = currZone;
                ++count;
            }
        currPage = currPage->parent_page;
    }

    if (count == 1)
    {
        return EditZone(last);
    }
    else if (count <= 0)
    {
        return 0;
    }

    edit_zone = NULL;
    edit_page = currPage;
    WInt8(TERM_EDITMULTIZONE);
    WInt8(user->CanEditSystem());

    if (behave) WInt16(last->behave);     else WInt16(-1);
    if (font)   WInt16(last->font);       else WInt16(-1);
    if (frame1) WInt16(last->frame[0]);   else WInt16(-1);
    if (tex1)   WInt16(last->texture[0]); else WInt16(-1);
    if (color1) WInt16(last->color[0]);   else WInt16(-1);
    if (frame2) WInt16(last->frame[1]);   else WInt16(-1);
    if (tex2)   WInt16(last->texture[1]); else WInt16(-1);
    if (color2) WInt16(last->color[1]);   else WInt16(-1);
    if (shape)  WInt16(last->shape);      else WInt16(-1);
    if (shadow) WInt16(last->shadow);     else WInt16(-1);

    return SendNow();
}

int Terminal::ReadMultiZone()
{
    FnTrace("Terminal::ReadMultiZone()");

    int behave = RInt16();
    int font   = RInt16();
    int frame1 = RInt16();
    int tex1   = RInt16();
    int color1 = RInt16();
    int frame2 = RInt16();
    int tex2   = RInt16();
    int color2 = RInt16();
    int shape  = RInt16();
    int shadow = RInt16();

    Page *currPage = edit_page;
    if (currPage == NULL)
        currPage = page;
    while (currPage)
    {
        for (Zone *currZone = page->ZoneList(); currZone != NULL; currZone = currZone->next)
            if (currZone->edit && currZone->CanEdit(this))
            {
                if (behave != -1) currZone->behave     = behave;
                if (font   != -1) currZone->font       = font;
                if (frame1 != -1) currZone->frame[0]   = frame1;
                if (tex1   != -1) currZone->texture[0] = tex1;
                if (color1 != -1) currZone->color[0]   = color1;
                if (frame2 != -1) currZone->frame[1]   = frame2;
                if (tex2   != -1) currZone->texture[1] = tex2;
                if (color2 != -1) currZone->color[1]   = color2;
                if (shape  != -1) currZone->shape      = shape;
                if (shadow != -1) currZone->shadow     = shadow;
            }
        currPage = currPage->parent_page;
    }

    edit_page = NULL;
    Draw(RENDER_NEW);
    UserInput();

    return 0;
}

int Terminal::EditZone(Zone *currZone)
{
    FnTrace("Terminal::EditZone()");

    if (user == NULL)
        return 1;

    edit_zone = currZone;
    SalesItem *currItem = NULL;

    WInt8(TERM_EDITZONE);
    WInt8(user->CanEditSystem());
    if (currZone)
    {
        WInt8(currZone->Type());
        WStr(currZone->name.Value());
        if (edit_zone->page)
            WInt32(edit_zone->page->id);
        else
            WInt32(page->id);
        WInt8(currZone->group_id);
        WInt8(currZone->behave);
        WInt8(currZone->Confirm());
        WStr(currZone->ConfirmMsg());
        WInt8(currZone->font);
        WInt8(currZone->ZoneStates());
        for (int i = 0; i < 3; ++i)
        {
            WInt8(currZone->frame[i]);
            WInt8(currZone->texture[i]);
            WInt8(currZone->color[i]);
            WInt8(currZone->image[i]);
        }
        WInt8(currZone->shape);
        WInt16(currZone->shadow);
        WInt16(currZone->key);
        WStr(currZone->Expression());
        WStr(currZone->Message());
        WStr(currZone->FileName());
        WInt8(currZone->TenderType());
        int tmp = 0;
        if (currZone->TenderAmount())
            tmp = *currZone->TenderAmount();
        WStr(SimpleFormatPrice(tmp));
        WInt8(currZone->ReportType());
        WInt8(currZone->CheckDisplayNum());
        WInt8(currZone->VideoTarget());
        WInt8(currZone->ReportPrint());
        WStr(currZone->Script());
        WFlt(currZone->Spacing());
        WInt32(currZone->QualifierType());
        WInt32(currZone->Amount());
        WInt8(currZone->SwitchType());
        WInt8(currZone->JumpType());
        WInt32(currZone->JumpID());
        currItem = currZone->Item(&(system_data->menu));
        WInt16(currZone->CustomerType());
        WInt8(currZone->DrawerZoneType());
    }
    else
    {
        // Defaults for new zone
        WInt8(ZONE_SIMPLE);             // Type
        WStr("");                       // Name
        WInt32(page->id);               // Page
        WInt8(0);                       // Group ID
        WInt8(BEHAVE_BLINK);            // Behavior
        WInt8(0);                       // Confirm
        WStr("");                       // Confirmation Message
        WInt8(FONT_DEFAULT);            // Font
        WInt8(2);                       // Number of states
        for (int i = 0; i < 3; ++i)
        {
            WInt8(ZF_DEFAULT);          // Font
            WInt8(IMAGE_DEFAULT);       
            WInt8(COLOR_DEFAULT);
            WInt8(0);
        }
        WInt8(SHAPE_RECTANGLE);         // Shape
        WInt16(256);                    // Shadow
        WInt16(0);                      // Key
        WStr("");                       // Expression
        WStr("");                       // Message
        WStr("");                       // Filename
        WInt8(0);                       // Tender Type
        WStr(SimpleFormatPrice(0));     // Tender Amount
        WInt8(0);                       // Report Type
        WInt8(0);                       // Check Display Number
        WInt8(PRINTER_DEFAULT);         // Video Target
        WInt8(0);                       // Report Print
        WStr("");                       // Script
        WFlt(1.0);                      // Spacing
        WInt32(0);                      // Qualifier Type
        WInt32(0);                      // Amount
        WInt8(0);                       // Switch Type
        WInt8(0);                       // Jump Type
        WInt32(0);                      // Jump ID
        WInt16(0);                      // Customer Type
        WInt8(DRAWER_ZONE_BALANCE);
    }

    if (currItem)
    {
        WStr(currItem->item_name.Value());
        WStr(currItem->print_name.Value());
        if (currItem->zone_name.length <= 0)
            WStr(currZone->name.Value());
        else
            WStr(currItem->zone_name.Value());
        WInt8(currItem->type);
        WStr(currItem->location.Value());
        WStr(currItem->event_time.Value());              // item event time
        WStr(currItem->total_tickets.Value());          // item total tickets
        WStr(currItem->available_tickets.Value());      // item available tickets
        WStr(currItem->price_label.Value());
        WStr(SimpleFormatPrice(currItem->cost));
        WStr(SimpleFormatPrice(currItem->sub_cost));
        WStr(SimpleFormatPrice(currItem->employee_cost));
        WInt8(currItem->family);
        WInt8(currItem->sales_type);
        WInt8(currItem->printer_id);
        WInt8(currItem->call_order);
    }
    else
    {
        WStr("");          // item name
        WStr("");          // item printed name
        WStr("");          // item zone name
        WInt8(0);          // item type
        WStr("");      //location
        WStr("");          // item event time
        WStr("");          // item total tickets
        WStr("");          // item available tickets
        WStr("");      // item price_label
        
        WStr(SimpleFormatPrice(0)); // item price
        WStr(SimpleFormatPrice(0)); // item sub price
        WStr(SimpleFormatPrice(0)); // employee price
        WInt8(0);          // item family
        WInt8(0);          // item sales type
        WInt8(0);          // item printer
        WInt8(0);          // item call order
    }

    return SendNow();
}

int Terminal::TranslateZone(Zone *z)
{
    FnTrace("Terminal::TranslateZone()");
    if (z == NULL)
        return 1;

    edit_zone = z;
    const genericChar* k = z->TranslateString(this);
    if (k == NULL || strlen(k) <= 0)
        return 1;

    const genericChar* v = MasterLocale->Translate(k);
    WInt8(TERM_TRANSLATE);
    WInt8(1);
    WStr(k);
    if (v != k)
        WStr(v);
    else
        WStr("");
    return SendNow();
}

int Terminal::TranslatePage(Page *p)
{
    FnTrace("Terminal::TranslatePage()");
    if (p == NULL)
        return 1;

    edit_page = p;
    const genericChar* k = p->name.Value();
    const genericChar* v = MasterLocale->Translate(k);

    WInt8(TERM_TRANSLATE);
    WInt8(1);
    WStr(k);

    if (v != k)
        WStr(v);
    else
        WStr("");

    return SendNow();
}

int Terminal::ReadZone()
{
    FnTrace("Terminal::ReadZone()");
    Zone *newZone = NewPosZone(RInt8());

    if (edit_zone)
        edit_zone->CopyZone(newZone);

    // NOTE:    the following initializations
    //              MUST be done in the order in which they
    //              are currently listed
    newZone->name.Set(RStr());
    int my_page_id  = RInt32();
    newZone->group_id = RInt8();
    newZone->behave   = RInt8();
    RInt8(newZone->Confirm());
    RStr(newZone->ConfirmMsg());
    newZone->font     = RInt8();

    for (int i = 0; i < 3; ++i)
    {
        newZone->frame[i]   = RInt8();
        newZone->texture[i] = RInt8();
        newZone->color[i]   = RInt8();
        newZone->image[i]   = RInt8();
    }

    newZone->shape  = RInt8();
    newZone->shadow = RInt16();
    newZone->key    = RInt16();

    RStr(newZone->Expression());
    RStr(newZone->Message());
    RStr(newZone->FileName());
    RInt8(newZone->TenderType());
    ParsePrice(RStr(), newZone->TenderAmount());
    RInt8(newZone->ReportType());
    RInt8(newZone->CheckDisplayNum());
    RInt8(newZone->VideoTarget());
    RInt8(newZone->ReportPrint());
    RStr(newZone->Script());
    RFlt(newZone->Spacing());
    RInt32(newZone->QualifierType());
    RInt32(newZone->Amount());
    RInt8(newZone->SwitchType());
    RInt8(newZone->JumpType());
    RInt32(newZone->JumpID());
    RInt16(newZone->CustomerType());
    RInt8(newZone->DrawerZoneType());

    if (newZone->ItemName())
    {
        Str tempstr;  // try to prevent buffer overflows
        genericChar str[STRLENGTH];
        genericChar iname[STRLONG];
        const genericChar* n;
        tempstr.Set(RStr());  // get item name and copy it into str
        strncpy(str, tempstr.Value(), STRLENGTH - 1);
        if (tempstr.length >= STRLENGTH)  // make sure we don't get buffer overflow
            str[STRLENGTH - 1] = '\0';
        iname[0] = '\0';
        if (strlen(str) > 0)
        {
            n = FilterName(str);
            strncpy(iname, n, STRLONG);
        }
        else if (newZone->name.length > 0)
        {
            n = FilterName(newZone->name.Value());
            strncpy(iname, n, STRLONG);
        }

        // Try to find the existing item.  This will only match if the name has
        // not changed.
        SalesItem *si = system_data->menu.FindByName(iname);
        if (si == NULL)
        {
            // We don't have a match, so create new menu item
            si = new SalesItem(iname);
            system_data->menu.Add(si);
            // Now see if we have an old copy of the item so we can move its
            // members over and delete the old item.  The old item must be
            // deleted because SalesItem::FindByName() does a binary search;
            // thus the members must be internally sorted by item_name, which
            // has changed.
            if (edit_zone != NULL && edit_zone->Type() == ZONE_ITEM)
            {
                SalesItem *olditem = system_data->menu.FindByName(edit_zone->ItemName()->Value());
                if (olditem != NULL)
                {
                    olditem->Copy(si);
                    // only remove the old item if this is a copy.
                    if (newZone->iscopy == 1)
                        newZone->iscopy = 1;
                    else
                        system_data->menu.Remove(olditem);
                    // set the item_name again (the Copy() overwrote it).
                    si->item_name.Set(iname);
                }
            }
        }

        newZone->ItemName()->Set(iname);
        if (si != NULL)
        {
            // DO NOT forget to also modify the "else throw away" block below
            // if you make any changes to the number or types of items read.
            si->print_name.Set(FilterName(RStr()));
            si->zone_name.Set(RStr());
            si->type          = RInt8();
        si->location.Set(RStr());
        si->event_time.Set(RStr());
        si->total_tickets.Set(RStr());
        si->available_tickets.Set(RStr());
        si->price_label.Set(RStr());
            si->cost          = ParsePrice(RStr());
            si->sub_cost      = ParsePrice(RStr());
            si->employee_cost = ParsePrice(RStr());
            si->family        = RInt8();
            si->sales_type    = RInt8();
            si->printer_id    = RInt8();
            si->call_order    = RInt8();
        }
    }
    else
    {
        RStr();   // item name
        RStr();   // item printed name
        RStr();   // item zone name
        RInt8();  // item type
    RStr();   // item location
    RStr();   // item event time
    RStr();   // item total tickets
    RStr();   // item available tickets
    RStr();   // item price_label
        RStr();   // item price
        RStr();   // item subprice
        RStr();   // employee price
        RInt8();  // item family
        RInt8();  // item sales type
        RInt8();  // item printer
        RInt8();  // item call order
    }

    int page_size = page->size;
    if (edit_zone && edit_zone->page)
        page_size = edit_zone->page->size;

    Page *currPage = zone_db->FindByID(my_page_id, page_size);
    if (currPage == NULL)
    {
        currPage = page;
        if (edit_zone && edit_zone->page)
            currPage = edit_zone->page;
    }

    if (edit_zone)
    {
        if (edit_zone->page)
            edit_zone->page->Remove(edit_zone);
        if (selected_zone == edit_zone)
            selected_zone = NULL;
        if (active_zone == edit_zone)
            active_zone = NULL;
        delete edit_zone;
        edit_zone = NULL;
    }

    if (newZone->Type() == ZONE_COMMENT)
        currPage->AddFront(newZone); // make sure comment zones are always on top
    else
        currPage->Add(newZone);

    Draw(RENDER_NEW);
    UserInput();

    return 0;
}

int Terminal::KillZone()
{
    FnTrace("Terminal::KillZone()");
    if (edit_zone)
    {
        if (edit_zone->page)
            edit_zone->page->Remove(edit_zone);
        delete edit_zone;

        edit_zone = NULL;
        Draw(RENDER_NEW);
    }
    UserInput();

    return 0;
}

int Terminal::EditPage(Page *p)
{
    FnTrace("Terminal::EditPage()");
    if (user == NULL || !user->CanEdit())
        return 1;

    if (p && p->id < 0 && !user->CanEditSystem())
        return 1;

    int edit_system = user->CanEditSystem();
    edit_page = p;

    WInt8(TERM_EDITPAGE);
    WInt8(edit_system);
    if (p)
    {
        WInt8(p->size);
        WInt8(p->type);
        WStr(p->name.Value());
        WInt32(p->id);
        WInt8(p->title_color);
        WInt8(p->image);
        WInt8(p->default_font);

        for (int i = 0; i < 3; ++i)
        {
            WInt8(p->default_frame[i]);
            WInt8(p->default_texture[i]);
            WInt8(p->default_color[i]);
        }

        WInt8(p->default_spacing);
        WInt16(p->default_shadow);
        WInt32(p->parent_id);
        WInt8(p->index);
    }
    else
    {
        WInt8(SIZE_1024x768);
        if (edit_system)
            WInt8(PAGE_SYSTEM);
        else
            WInt8(PAGE_ITEM);
        WStr("");
        WInt32(0);
        WInt8(COLOR_BLUE);
        WInt8(IMAGE_GRAY_MARBLE);
        WInt8(FONT_HELV_24);
        WInt8(ZF_RAISED);
        WInt8(IMAGE_SAND);
        WInt8(COLOR_BLACK);
        WInt8(ZF_RAISED);
        WInt8(IMAGE_LIT_SAND);
        WInt8(COLOR_BLACK);
        WInt8(ZF_HIDDEN);
        WInt8(IMAGE_SAND);
        WInt8(COLOR_BLACK);
        WInt8(2);
        WInt16(0);
        WInt32(0);
        WInt8(INDEX_GENERAL);
    }

    return SendNow();
}

int Terminal::ReadPage()
{
    FnTrace("Terminal::ReadPage()");

    Page *currPage = edit_page;
    edit_page = NULL;

    if (currPage == NULL)
    {
        currPage = NewPosPage();
        zone_db->Add(currPage);
    }

    currPage->size = RInt8();
    currPage->type = RInt8();
    currPage->name.Set(RStr());
    int my_id = RInt32();
    currPage->title_color = RInt8();
    currPage->image = RInt8();
    currPage->default_font = RInt8();

    for (int i = 0; i < 3; ++i)
    {
        currPage->default_frame[i] = RInt8();
        currPage->default_texture[i] = RInt8();
        currPage->default_color[i] = RInt8();
    }

    currPage->default_spacing = RInt8();
    currPage->default_shadow = RInt16();
    currPage->parent_id = RInt32();
    currPage->index = RInt8();

    if (my_id < 0 && !user->CanEditSystem())
        my_id = 0;

    if (currPage->id != my_id && zone_db->IsPageDefined(my_id, currPage->size) == 0)
    {
        zone_db->ChangePageID(currPage, my_id);
        system_data->user_db.ChangePageID(currPage->id, my_id);
    }

    currPage->Init(zone_db);
    ChangePage(currPage);
    UserInput();

    return 0;
}

int Terminal::KillPage()
{
    FnTrace("Terminal::KillPage()");
    Page *currPage = edit_page;
    edit_page = NULL;

    Page *jump = currPage->next;
    if (jump == NULL)
    {
        jump = currPage->fore;
        if (jump == NULL)
            return 1;
    }

    zone_db->Remove(currPage);
    delete currPage;

    zone_db->Init();
    page = NULL;
    ChangePage(jump);
    UserInput();

    return 0;
}

int Terminal::ShowPageList()
{
    FnTrace("Terminal::ShowPageList()");
    WInt8(TERM_LISTSTART);

    genericChar str[256];
    int edit_system = user->CanEditSystem(), last_id = 0;

    for (Page *p = zone_db->PageList(); p != NULL; p = p->next)
        if ((p->id != last_id || p->id == 0) &&
            (p->id >= 0 || edit_system))
        {
            last_id = p->id;
            WInt8(TERM_LISTITEM);
            sprintf(str, "%4d %s", p->id, Translate(p->name.Value()));
            WStr(str);
            Send();
        }

    WInt8(TERM_LISTEND);
    return SendNow();
}

int Terminal::JumpList(int selected)
{
    FnTrace("Terminal::JumpList()");
    if (!edit && !translate)
        return 1;

    int edit_system = user->CanEditSystem();
    int last_id = 0;

    for (Page *currPage = zone_db->PageList(); currPage != NULL; currPage = currPage->next)
    {
        if ((currPage->id != last_id || currPage->id == 0) && (currPage->id >= 0 || edit_system))
        {
            --selected;
            if (selected <= 0)
                return Jump(JUMP_STEALTH, currPage->id);
            last_id = currPage->id;
        }
    }

    UserInput();
    return 0;
}

Settings *Terminal::GetSettings()
{
    FnTrace("Terminal::GetSettings()");
    if (system_data == NULL)
        return NULL;

    return &(system_data->settings);
}

/****
 * EndDay: Originally, EndDay() was purely a System method.  But now
 * we have the potential for loops where we need to ClearSAF() and
 * Settle() for each terminal before finally running all of the EndDay
 * stuff.  Plus, we should probably be updating the EndDay dialog
 * periodically to let the user know the system hasn't stalled.  So,
 * to begin an EndDay, set system_data->term to the terminal that
 * should do the processing, then set that term's eod_processing
 * member to EOD_BEGIN.  manager.cc's UpdateSystemCB() will handle it
 * from there.
 *
 * This method returns 1 if we're still processing End of Day, 0 if
 * we're done.
 ****/
int Terminal::EndDay()
{
    FnTrace("Terminal::EndDay()");
    int retval = 1;
    int auth;

    if (eod_failed)
    {
        system_data->eod_term = NULL;
        system_data->non_eod_settle = 0;
        eod_processing = EOD_DONE;
        eod_failed = 0;
        CC_ClearSAF(1);
        CC_Settle(NULL, 1);
        Signal("enddayfailed", 0);
        retval = 0;
    }

    system_data->non_eod_settle = 0;
    if (eod_processing == EOD_BEGIN)
    {
        auth = GetSettings()->authorize_method;
        if (auth == CCAUTH_MAINSTREET)
            eod_processing = EOD_SETTLE;
        else if (auth == CCAUTH_CREDITCHEQ)
            eod_processing = EOD_SAF;
        else
            eod_processing = EOD_FINAL;
    }

    // Clear SAF Transactions
    if (eod_processing == EOD_SAF &&
        cc_processing == 0 &&
        CC_ClearSAF() >= 0)
    {
        eod_processing = EOD_SETTLE;
    }
    
    // settle all credit/debit transactions.
    if (eod_processing == EOD_SETTLE &&
        cc_processing == 0 &&
        CC_Settle() >= 0)
    {
        eod_processing = EOD_FINAL;
    }

    // final processing
    if (eod_processing == EOD_FINAL)
    {
        system_data->EndDay();

        system_data->eod_term = NULL;
        system_data->non_eod_settle = 0;
        eod_processing = EOD_DONE;
        Signal("enddaydone", 0);
        retval = 0;
    }

    return retval;
}

int Terminal::WriteCreditCard(int amount)
{
    FnTrace("Terminal::WriteCreditCard()");
    int retval = 1;
    Settings *settings = GetSettings();

    if (credit != NULL)
    {
        WStr(settings->cc_server.Value());
        WStr(settings->cc_port.Value());
        WStr(settings->cc_user.Value());
        WStr(settings->cc_password.Value());
        if (credit->CardType() == CARD_TYPE_CREDIT)
            WStr(cc_credit_termid.Value());
        else if (credit->CardType() == CARD_TYPE_DEBIT)
            WStr(cc_debit_termid.Value());
        else
            WStr("Unknown");
        WStr(credit->approval.Value());
        if (settings->authorize_method == CCAUTH_MAINSTREET)
            WStr(credit->swipe.Value());
        WStr(credit->number.Value());
        WStr(credit->name.Value());
        WStr(credit->expire.Value());
        WStr(credit->code.Value());
        WInt8(credit->intcode);
        WStr(credit->verb.Value());
        WStr(credit->auth.Value());
        WLLong(credit->batch);
        WLLong(credit->item);
        WLLong(credit->ttid);
        WStr(credit->AVS.Value());
        WStr(credit->CV.Value());
        if (amount > 0)
            WInt32(amount);
        else
            WInt32(credit->amount);
        WInt32(credit->FullAmount());
        WInt8(credit->card_type);

        // specific to CreditCheq
        WStr(credit->reference.Value());
        WStr(credit->sequence.Value());
        WStr(credit->server_date.Value());
        WStr(credit->server_time.Value());
        WStr(credit->receipt_line.Value());
        WStr(credit->display_line.Value());

        SendNow();
        retval = 0;
    }

    return retval;
}

int Terminal::ReadCreditCard()
{
    FnTrace("Terminal::ReadCreditCard()");
    int retval = 1;
    int auth = GetSettings()->authorize_method;

    if (credit != NULL)
    {
        credit->approval.Set(RStr());
        credit->number.Set(RStr());
        credit->expire.Set(RStr());
        credit->name.Set(RStr());
        credit->country.Set(RStr());
        credit->debit_acct = RInt8();
        credit->code.Set(RStr());
        credit->intcode = (signed char)RInt8();
        if (auth == CCAUTH_CREDITCHEQ)
        {
            credit->isocode.Set(RStr());
            credit->b24code.Set(RStr());
            credit->read_manual = RInt8();
        }
        credit->verb.Set(RStr());
        credit->auth.Set(RStr());
        credit->batch = RLLong();
        credit->item = RLLong();
        credit->ttid = RLLong();
        credit->AVS.Set(RStr());
        credit->CV.Set(RStr());
        credit->trans_success = RInt8();

        // kludge for CardNet, which doesn't apparently return Verb
        if (credit->verb.length == 0)
        {
            if (credit->code.length == 0 && credit->auth.length == 0)
            {
                credit->verb.Set(Translate("No Verbiage Set"));
            }
            else
            {
                char temp[STRLENGTH];
                snprintf(temp, STRLENGTH, "%s %s", credit->code.Value(), credit->auth.Value());
                credit->verb.Set(temp);
            }
        }

        // specific to CreditCheq
        credit->term_id.Set(RStr());
        credit->reference.Set(RStr());
        credit->sequence.Set(RStr());
        credit->server_date.Set(RStr());
        credit->server_time.Set(RStr());
        credit->receipt_line.Set(RStr());
        credit->display_line.Set(RStr());

        credit->SetState();

        if (auth == CCAUTH_MAINSTREET)
            system_data->AddBatch(credit->Batch());

        retval = 0;
    }
    else
    {
        // We don't have a credit card, but we still need to get the data
        // out of the pipeline, so we'll just junk it.
        if (debug_mode)
            printf("Junking the credit card data...\n");
        RStr();
        RStr();
        RStr();
        RStr();
        RStr();
        RInt8();
        RStr();
        RInt8();
        if (GetSettings()->authorize_method == CCAUTH_CREDITCHEQ)
        {
            RStr();
            RStr();
            RInt8();
        }
        RStr();
        RStr();
        RLLong();
        RLLong();
        RLLong();
        RStr();
        RStr();
        RInt8();

        // specific to CreditCheq
        RStr();
        RStr();
        RStr();
        RStr();
        RStr();
        RStr();
        RStr();
    }

    return retval;
}

int Terminal::CC_GetApproval()
{
    FnTrace("Terminal::CC_GetApproval()");
    int retval = 1;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;

    if (auth_method != CCAUTH_NONE && auth_method != CCAUTH_VISANET)
    {
        WInt8(TERM_CC_AUTH);
        WriteCreditCard();
        retval = SendNow();
        if (retval < 0 && credit != NULL)
            credit->intcode = CC_STATUS_WRITEFAIL;
    }

    return retval;
}

int Terminal::CC_GetPreApproval()
{
    FnTrace("Terminal::CC_GetPreApproval()");
    int retval = 0;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;
    int amount = 0;

    if (auth_method != CCAUTH_NONE && auth_method != CCAUTH_VISANET)
    {
        WInt8(TERM_CC_PREAUTH);
        if (settings->cc_preauth_add > 0)
        {
            amount = credit->amount;
            amount += ((amount * settings->cc_preauth_add) / 100);
        }
        WriteCreditCard(amount);
        retval = SendNow();
        if (retval < 0 && credit != NULL)
            credit->intcode = CC_STATUS_WRITEFAIL;
    }

    return retval;
}

int Terminal::CC_GetFinalApproval()
{
    FnTrace("Terminal::CC_GetFinalApproval()");
    int retval = 0;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;

    if (auth_method != CCAUTH_NONE && auth_method != CCAUTH_VISANET)
    {
        WInt8(TERM_CC_FINALAUTH);
        WriteCreditCard();
        retval = SendNow();
        if (retval < 0 && credit != NULL)
            credit->intcode = CC_STATUS_WRITEFAIL;
    }

    return retval;
}

int Terminal::CC_GetVoid()
{
    FnTrace("Terminal::CC_GetVoid()");
    int retval = 0;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;

    if (auth_method != CCAUTH_NONE && auth_method != CCAUTH_VISANET)
    {
        WInt8(TERM_CC_VOID);
        WriteCreditCard();
        retval = SendNow();
        if (retval < 0 && credit != NULL)
            credit->intcode = CC_STATUS_WRITEFAIL;
    }

    return retval;
}

int Terminal::CC_GetVoidCancel()
{
    FnTrace("Terminal::CC_GetVoidCancel()");
    int retval = 0;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;

    if (auth_method != CCAUTH_NONE && auth_method != CCAUTH_VISANET)
    {
        WInt8(TERM_CC_VOID_CANCEL);
        WriteCreditCard();
        retval = SendNow();
        if (retval < 0 && credit != NULL)
            credit->intcode = CC_STATUS_WRITEFAIL;
    }

    return retval;
}

int Terminal::CC_GetRefund()
{
    FnTrace("Terminal::CC_GetRefund()");
    int retval = 0;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;

    if (auth_method != CCAUTH_NONE && auth_method != CCAUTH_VISANET)
    {
        WInt8(TERM_CC_REFUND);
        WriteCreditCard();
        retval = SendNow();
        if (retval < 0 && credit != NULL)
            credit->intcode = CC_STATUS_WRITEFAIL;
    }

    return retval;
}

int Terminal::CC_GetRefundCancel()
{
    FnTrace("Terminal::CC_GetRefundCancel()");
    int retval = 0;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;

    if (auth_method != CCAUTH_NONE && auth_method != CCAUTH_VISANET)
    {
        WInt8(TERM_CC_REFUND_CANCEL);
        WriteCreditCard();
        retval = SendNow();
        if (retval < 0 && credit != NULL)
            credit->intcode = CC_STATUS_WRITEFAIL;
    }

    return retval;
}

#define CC_SYS_STATE_START   0
#define CC_SYS_STATE_CREDIT  1
#define CC_SYS_STATE_DEBIT   2
#define CC_SYS_STATE_DONE    3
#define CC_SYS_STATE_NEXT    4  // mostly for MainStreet

int Terminal::CC_TermIDIsDupe(const char* termid)
{
    FnTrace("Terminal::CC_TermIDIsDupe()");
    int retval = 0;
    Str *curr_termid = term_id_list.Head();

    while (curr_termid != NULL && retval == 0)
    {
        if (strcmp(curr_termid->Value(), termid) == 0)
            retval = 1;
        else
            curr_termid = curr_termid->next;
    }

    return retval;
}

int Terminal::CC_GetTermIDList(Terminal *start_term)
{
    FnTrace("Terminal::CC_GetTermIDList()");
    int retval = 0;
    Terminal *curr_term = start_term;
    Str *term_id;

    term_id_list.Purge();

    while (curr_term != NULL)
    {
        if (curr_term->cc_debit_termid.length > 0 &&
            CC_TermIDIsDupe(curr_term->cc_debit_termid.Value()) == 0)
        {
            term_id = new Str(curr_term->cc_debit_termid);
            term_id_list.AddToTail(term_id);
            retval += 1;
        }
        if (curr_term->cc_credit_termid.length > 0 &&
            CC_TermIDIsDupe(curr_term->cc_credit_termid.Value()) == 0)
        {
            term_id = new Str(curr_term->cc_credit_termid);
            term_id_list.AddToTail(term_id);
            retval += 1;
        }
        curr_term = curr_term->next;
    }

    return retval;
}

/****
 * CC_NextTermID: Walks through the terminals looking for Credit Card
 * Terminal IDs for those processors that need it (e.g. CreditCheq
 * Multi).  Returns 1 if another ID is found, 0 if we're all done.
 ****/
int Terminal::CC_NextTermID(int *cc_state, char* termid)
{
    FnTrace("Terminal::CC_NextTermID()");
    int retval = 0;
    static Str *next_id = NULL;
    Settings *settings = GetSettings();

    if (settings->authorize_method == CCAUTH_CREDITCHEQ)
    {
        if (*cc_state == CC_SYS_STATE_START)
        {
            CC_GetTermIDList(MasterControl->TermList());
            *cc_state = CC_SYS_STATE_NEXT;
            next_id = term_id_list.Head();
        }

        if (next_id != NULL)
        {
            strcpy(termid, next_id->Value());
            next_id = next_id->next;
            retval = 1;
        }
    }

    return retval;
}

/****
 * CC_NextBatch:  Walks through BatchList looking for the next
 *  batch number.  Returns 1 if there is a batch available,
 *  0 if we're all done.
 ****/
int Terminal::CC_NextBatch(int *state, BatchItem **currbatch, long long *batch)
{
    FnTrace("Terminal::CC_NextBatch()");
    int retval = 0;

    if (*state == CC_SYS_STATE_START)
    {
        *currbatch = system_data->BatchList.Head();
        if (*currbatch != NULL)
            *state = CC_SYS_STATE_NEXT;
    }
    else
        *currbatch = (*currbatch)->next;

    if (*currbatch != NULL)
    {
        *batch = (*currbatch)->batch;
        retval = 1;
    }

    return retval;
}

int Terminal::CC_Settle(const char* batch, int reset)
{
    FnTrace("Terminal::CC_Settle()");
    int retval                  = 1;
    Settings *settings          = GetSettings();
    int auth_method             = settings->authorize_method;
    char termid[STRLENGTH]      = "";
    static int state            = CC_SYS_STATE_START;
    char batchstr[STRLENGTH]    = "";

    if (reset)
    {
        state = CC_SYS_STATE_START;
    }
    else if (user->training == 0 && OtherTermsInUse() == 0)
    {
        if (auth_method == CCAUTH_MAINSTREET)
        {
            if (state == CC_SYS_STATE_START)
            {
                if (batch != NULL)
                    strcpy(batchstr, batch);
                else
                    strcpy(batchstr, "find");
                state = CC_SYS_STATE_DONE;
                retval = -1;
            }
            else
            {
                system_data->BatchList.Purge();
                batchstr[0] = '\0';
                state = CC_SYS_STATE_START;
            }
        }
        else if (auth_method == CCAUTH_CREDITCHEQ)
        {
            if (CC_NextTermID(&state, termid))
            {
                retval = -1;
            }
            else
            {
                state = CC_SYS_STATE_START;
            }
        }
    }

    if (termid[0] != '\0' || batchstr[0] != '\0')
    {
        cc_processing = 1;
        WInt8(TERM_CC_SETTLE);
        WStr(settings->cc_server.Value());
        WStr(settings->cc_port.Value());
        if (auth_method == CCAUTH_MAINSTREET)
        {
            WStr(batchstr);
            WStr(settings->cc_user.Value());
            WStr(settings->cc_password.Value());
        }
        else
        {
            WStr(termid);
        }
        SendNow();
    }

    return retval;
}

int Terminal::CC_Init()
{
    FnTrace("Terminal::CC_Init()");
    int retval = 1;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;
    
    if (auth_method == CCAUTH_CREDITCHEQ)
    { 
        cc_processing = 1;
        WInt8(TERM_CC_INIT);
        WStr(settings->cc_server.Value());
        WStr(settings->cc_port.Value());
        WStr(cc_debit_termid.Value());
        retval = SendNow();
    }
    else if (auth_method == CCAUTH_MAINSTREET)
    {
    }

    return retval;
}

int Terminal::CC_Totals(const char* batch)
{
    FnTrace("Terminal::CC_Totals()");
    int retval = 1;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;
    char termid[STRLENGTH] = "";
    static int state = CC_SYS_STATE_START;
    char batchnum[STRLENGTH] = "";

    if (user->training == 0)
    {
        if (auth_method == CCAUTH_MAINSTREET)
        {
            if (batch != NULL)
                strcpy(batchnum, batch);
            else
                strcpy(batchnum, "all");
            state = CC_SYS_STATE_CREDIT;
        }
        else if (auth_method == CCAUTH_CREDITCHEQ)
        {
            if (CC_NextTermID(&state, termid))
            {
                retval = -1;
            }
            else
            {
                state = CC_SYS_STATE_START;
            }
        }
    }

    if (termid[0] != '\0' || batchnum[0] != '\0')
    {
        cc_processing = 1;
        WInt8(TERM_CC_TOTALS);
        WStr(settings->cc_server.Value());
        WStr(settings->cc_port.Value());
        if (auth_method == CCAUTH_MAINSTREET)
        {
            WStr(batchnum);
            WStr(settings->cc_user.Value());
            WStr(settings->cc_password.Value());
        }
        else
        {
            WStr(termid);
        }
        SendNow();
    }

    return retval;
}

int Terminal::CC_Details()
{
    FnTrace("Terminal::CC_Details()");
    int retval = 1;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;
    char termid[STRLENGTH] = "";
    static int state = CC_SYS_STATE_START;
    char batchnum[STRLENGTH] = "";

    if (user->training == 0)
    {
        if (auth_method == CCAUTH_MAINSTREET)
        {
            strcpy(batchnum, "all");
        }
        else if (auth_method == CCAUTH_CREDITCHEQ)
        {
            if (CC_NextTermID(&state, termid))
            {
                retval = -1;
            }
            else
            {
                state = CC_SYS_STATE_START;
            }
        }
    }

    if (termid[0] != '\0' || batchnum[0] != '\0')
    {
        cc_processing = 1;
        WInt8(TERM_CC_DETAILS);
        WStr(settings->cc_server.Value());
        WStr(settings->cc_port.Value());
        if (auth_method == CCAUTH_MAINSTREET)
        {
            WStr(batchnum);
            WStr(settings->cc_user.Value());
            WStr(settings->cc_password.Value());
        }
        else
        {
            WStr(termid);
        }
        SendNow();
    }

    return retval;
}

int Terminal::CC_ClearSAF(int reset)
{
    FnTrace("Terminal::CC_ClearSAF()");
    int retval = 1;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;
    char termid[STRLENGTH] = "";
    static int state = CC_SYS_STATE_START;

    if (reset)
    {
        state = CC_SYS_STATE_START;
    }
    else if (user->training == 0 && OtherTermsInUse() == 0)
    {
        if (auth_method == CCAUTH_MAINSTREET)
        {
        }
        else if (auth_method == CCAUTH_CREDITCHEQ)
        {
            if (CC_NextTermID(&state, termid))
            {
                retval = -1;
            }
            else
            {
                state = CC_SYS_STATE_START;
            }
        }
    }

    if (termid[0] != '\0')
    {
        cc_processing = 1;
        WInt8(TERM_CC_CLEARSAF);
        WStr(settings->cc_server.Value());
        WStr(settings->cc_port.Value());
        WStr(termid);
        SendNow();
    }

    return retval;
}

int Terminal::CC_SAFDetails()
{
    FnTrace("Terminal::CC_SAFDetails()");
    int retval = 1;
    Settings *settings = GetSettings();
    int auth_method = settings->authorize_method;
    char termid[STRLENGTH] = "";
    static int state = CC_SYS_STATE_START;
    
    if (user->training == 0 && OtherTermsInUse() == 0)
    {
        if (auth_method == CCAUTH_MAINSTREET)
        {
        }
        else if (auth_method == CCAUTH_CREDITCHEQ)
        {
            if (CC_NextTermID(&state, termid))
            {
                retval = -1;
            }
            else
            {
                state = CC_SYS_STATE_START;
            }
        }
    }

    if (termid[0] != '\0')
    {
        cc_processing = 1;
        WInt8(TERM_CC_SAFDETAILS);
        WStr(settings->cc_server.Value());
        WStr(settings->cc_port.Value());
        WStr(cc_debit_termid.Value());
        SendNow();
    }

    return retval;
}

int Terminal::CC_GetSettlementResults()
{
    FnTrace("Terminal::CC_GetSettlementResults()");
    int    retval = 1;
    const char* batch;
    const char* termid;
    Check *currcheck;
    int    auth_method = GetSettings()->authorize_method;

    system_data->cc_settle_results->Add(this);

    // Now we need to set the batch number for CreditCheq transactions.
    if (auth_method == CCAUTH_CREDITCHEQ)
    {
        batch = system_data->cc_settle_results->Batch();
        termid = system_data->cc_settle_results->TermID();
        if (strlen(batch) > 0)
        {
            currcheck = system_data->CheckList();
            while (currcheck != NULL && currcheck->IsBatchSet() == 0)
            {
                currcheck->SetBatch(termid, batch);
                system_data->cc_settle_results->Add(currcheck);
                currcheck = currcheck->next;
            }
        }
    }

    cc_processing = 0;
    // For End of Day settlement, the looping is handled by Terminal::EndDay()
    if (system_data->non_eod_settle && CC_Settle() >= 0)
    {
        Signal("ccsettledone", 0);
        Draw(1);
    }

    return retval;
}

int Terminal::CC_GetInitResults()
{
    FnTrace("Terminal::CC_GetInitResults()");
    int retval = 1;
    Str termid;
    Str message;
    int intcode;

    termid.Set(RStr());
    message.Set(RStr());
    intcode = RInt8();

    system_data->cc_init_results->Add(termid.Value(), message.Value());
    
    cc_processing = 0;
    Signal("ccinitdone", 0);
    Draw(1);

    return retval;
}

int Terminal::CC_GetTotalsResults()
{
    FnTrace("Terminal::CC_GetTotalsResults()");
    int retval = 1;
    int auth_method = GetSettings()->authorize_method;
    int rows;
    char line[STRLONG];

    if (auth_method == CCAUTH_MAINSTREET)
    {
        MasterSystem->cc_totals_results->Clear();
        rows = RInt16();
        int total_rows = rows;
        while (rows > 0)
        {
            if (RStr(line) == NULL)
            {
                snprintf(line, STRLONG, "Failed at %d reading totals results", total_rows - rows);
                ReportError(line);
                rows = 0;
            }
            else
            {
                MasterSystem->cc_totals_results->Add(line);
                rows -= 1;
            }
        }
    }
    else
    {
        cc_totals.Add(this);
    }

    cc_processing = 0;
    if (auth_method == CCAUTH_MAINSTREET || CC_Totals() >= 0)
    {
        Signal("cctotalsdone", 0);
        Draw(1);
    }

    return retval;
}

int Terminal::CC_GetDetailsResults()
{
    FnTrace("Terminal::CC_GetDetailsResults()");
    int retval = 1;
    Str termid;
    Str message;
    int intcode;
    int auth_method = GetSettings()->authorize_method;
    int rows;
    char line[STRLONG];

    if (auth_method == CCAUTH_MAINSTREET)
    {
        MasterSystem->cc_details_results->Clear();
        rows = RInt16();
        while (rows > 0)
        {
            RStr(line);
            MasterSystem->cc_details_results->Add(line);
            rows -= 1;
        }
    }
    else
    {
        termid.Set(RStr());
        message.Set(RStr());
        intcode = RInt8();
    }

    cc_processing = 0;
    if (auth_method == CCAUTH_MAINSTREET || CC_Details() >= 0)
    {
        Signal("ccdetailsdone", 0);
        Draw(1);
    }

    return retval;
}

int Terminal::CC_GetSAFClearedResults()
{
    FnTrace("Terminal::CC_GetSAFClearedResults()");
    int retval = 0;

    MasterSystem->cc_saf_details_results->Add(this);

    cc_processing = 0;
    // For End of Day clearing, the looping is handled by Terminal::EndDay()
    if (system_data->non_eod_settle && CC_ClearSAF() >= 0)
    {
        Signal("ccsafdone", 0);
        Draw(1);
    }

    return retval;
}

int Terminal::CC_GetSAFDetails()
{
    FnTrace("Terminal::CC_GetSAFDetails()");
    int retval = 0;

    MasterSystem->cc_saf_details_results->Add(this);

    cc_processing = 0;
    if (CC_SAFDetails() >= 0)
    {
        Signal("ccsafdone", 0);
        Draw(1);
    }

    return retval;
}

int Terminal::SetCCTimeout(int cc_timeout)
{
    FnTrace("Terminal::SetCCTimeout()");
    int retval = 0;

    WInt8(TERM_CONNTIMEOUT);
    WInt16(cc_timeout);
    SendNow();
    

    return retval;
}


/**** Functions ****/

/****
 * OpenTerminalSocket:  Unfortunately, the name was chosen poorly.  This function
 *  also starts vt_term.
 ****/
int OpenTerminalSocket(const char* hostname, int hardware_type, int isserver, int width, int height)
{
    FnTrace("OpenTerminalSocket()");
    int socket_no = -1;
    int dev = -1;
    struct sockaddr_un server_adr;
    struct sockaddr_un client_adr;
    server_adr.sun_family = AF_UNIX;
    strcpy(server_adr.sun_path, SOCKET_FILE);
    unlink(SOCKET_FILE);

    genericChar str[256];
    dev = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dev <= 0)
    {
        sprintf(str, "Failed to open socket '%s'", SOCKET_FILE);
        ReportError(str);
    }
    else if (bind(dev, (struct sockaddr *) &server_adr, SUN_LEN(&server_adr)) < 0)
    {
        sprintf(str, "Failed to bind socket '%s'", SOCKET_FILE);
        ReportError(str);
    }
    else
    {
        if (width > -1 && height > -1)
        {
            sprintf(str, VIEWTOUCH_PATH "/bin/vt_term %s %d %s %d %d %d &",
                    SOCKET_FILE, hardware_type, hostname, isserver, width, height);
        }
        else
        {
            sprintf(str, VIEWTOUCH_PATH "/bin/vt_term %s %d %s %d&",
                    SOCKET_FILE, hardware_type, hostname, isserver);
        }
        system(str);
        listen(dev, 1);

#ifdef AIX
        Ulong len;
#else
        Uint len;
#endif

        len = sizeof(client_adr);
        socket_no = accept(dev, (struct sockaddr *) &client_adr, &len);
        if (socket_no <= 0)
        {
            sprintf(str, "Failed to open term on host '%s'", hostname);
            ReportError(str);
        }
    }

    if (dev)
        close(dev);
    unlink(SOCKET_FILE);

    return socket_no;
}

Terminal* NewTerminal(const genericChar* hostname, int hardware_type, int isserver)
{
    FnTrace("NewTerminal()");
    int socket_no = -1;
    Terminal *term = NULL;

    socket_no = OpenTerminalSocket(hostname, hardware_type, isserver);
    if (socket_no > 0)
    {
        term = new Terminal;
        term->socket_no = socket_no;
        term->buffer_in  = new CharQueue(QUEUE_SIZE);
        term->buffer_out = new CharQueue(QUEUE_SIZE);
        term->host.Set(hostname);
        term->input_id = AddInputFn((InputFn) TermCB, term->socket_no, term);
    }

    return term;
}

int CloneTerminal(Terminal *term, const char* dest, const char* name)
{
    FnTrace("CloneTerminal()");
    int retval = 0;
    int socket_no = -1;
    Terminal *new_term = NULL;

    socket_no = OpenTerminalSocket(dest, 0, 0, term->width, term->height);
    if (socket_no > 0)
    {
        new_term = new Terminal;
        new_term->socket_no = socket_no;
        new_term->buffer_in = NULL;
        new_term->buffer_out = NULL;
        term->host.Set(name);
        new_term->input_id = AddInputFn((InputFn) TermCB, new_term->socket_no, term);
        term->AddClone(new_term);
    }

    return retval;
}
