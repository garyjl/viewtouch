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
 * customer.hh - revision 6 (10/8/98)
 * Implementation of customer infomation module
 */

#ifndef _CUSTOMER_HH
#define _CUSTOMER_HH

#include "list_utility.hh"
#include "utility.hh"

#define CUSTOMER_VERSION         14
        

/**** Types ****/
class Settings;
class OutputDataFile;
class InputDataFile;
class CustomerInfoDB;


/*********************************************************************
 * CustomerInfo Class
 ********************************************************************/
class CustomerInfo
{
protected:
    int type;
    int guests;
    Str filepath;
    Str lastname;
    Str firstname;
    Str company;
    Str phone;
    Str extension;
    Str address;
    Str address2;
    Str cross_street;
    Str city;
    Str state;
    Str postal;
    Str license;
    Str cc_number;
    Str cc_expire;
    Str vehicle;
    Str comment;
    int training;
    int id;

    // deprecated. These are for backwards compatibility and should not
    // be used going forward.
    Str table;
    Str room;
    TimeInfo reserve_start;
    TimeInfo reserve_end;
    TimeInfo stay_start;
    TimeInfo stay_end;

public:
    // CustomerInfoDB needs to be able to set the Customer ID.  No one else
    // should be able to do this, either directly or with an interface method.
    friend class CustomerInfoDB;

    CustomerInfo(int new_type = -1);
    virtual ~CustomerInfo();

    CustomerInfo *next;
    CustomerInfo *fore;

    // Member Functions
    int         IsBlank();
    int         IsTraining(int set = -1);

    virtual int SetFileName(const genericChar* filename);
    virtual int Load(const genericChar* filename);
    virtual int Save();
    virtual int Read(InputDataFile &df, int version);
    virtual int Write(OutputDataFile &df, int version);
    virtual int DeleteFile();

    // Access Functions
    virtual int          Type() { return type; }
    virtual int          CustomerID() { return id; }
    virtual int          Guests(int set = -1);
    virtual const genericChar* LastName(const char* set = NULL);
    virtual const genericChar* FirstName(const char* set = NULL);
    virtual const genericChar* Company(const char* set = NULL);
    virtual const genericChar* PhoneNumber(const char* set = NULL);
    virtual const genericChar* Extension(const char* set = NULL);
    virtual const genericChar* Address(const char* set = NULL);
    virtual const genericChar* Address2(const char* set = NULL);
    virtual const genericChar* CrossStreet(const char* set = NULL);
    virtual const genericChar* City(const char* set = NULL);
    virtual const genericChar* State(const char* set = NULL);
    virtual const genericChar* Postal(const char* set = NULL);
    virtual const genericChar* License(const char* set = NULL);
    virtual const genericChar* CCNumber(const char* set = NULL);
    virtual const genericChar* CCExpire(const char* set = NULL);
    virtual const genericChar* Comment(const char* set = NULL);
    virtual const genericChar* Vehicle(const char* set = NULL);
    virtual int          Search(const char* word);
};


/*********************************************************************
 * CustomerInfoDB Class
 ********************************************************************/
class CustomerInfoDB
{
    DList<CustomerInfo> customers;
    Str pathname;
    int last_id;

    int RemoveBlank();
    int NextID() { last_id += 1; return last_id; }

public:

    CustomerInfoDB();
    ~CustomerInfoDB();

    CustomerInfo *CustomerList()                  { return customers.Head(); }
    CustomerInfo *CustomerListEnd()               { return customers.Tail(); }

    int           Count();
    int           Save(const genericChar* filepath = NULL);
    int           Save(CustomerInfo *customer);
    int           Load(const genericChar* filepath);
    CustomerInfo *NewCustomer(int customer_type);
    int           Add(CustomerInfo *customer);
    int           Remove(CustomerInfo *customer);
    CustomerInfo *FindByID(int customer_id);
    CustomerInfo *FindByString(const genericChar* search_string, int start = -1);
    CustomerInfo *FindBlank();
};


/**** Functions ****/
CustomerInfo *NewCustomerInfo(int type = -1);

#endif
