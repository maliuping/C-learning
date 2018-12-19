#ifndef _SYS_H
#define _SYS_H

#include "stdio.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

/*****************************Sales****************************************/
class Sales_data {

public:
    std::string bookNo;
    unsigned units_sold;
    double revenue;

    //constructor fun
    Sales_data();

    std::string isbn() const;
    Sales_data& combine(const Sales_data &rhs);

private:
protected:
};

std::istream& read (std::istream &is,Sales_data &item);
std::ostream& print (std::ostream &os,const Sales_data &item);
Sales_data add(const Sales_data &lhs, const Sales_data &rhs);


/*****************************Person****************************************/
class Person_data {

public:
    std::string m_name;
    std::string m_address;

    // constructor fun
    Person_data(std::string name,std::string address):m_name(name),m_address(address) {
    };

    // constructor fun overload
    Person_data();

    void showname();
    std::string getname() const;
    std::string getaddress() const;

private:
protected:

};

#endif
