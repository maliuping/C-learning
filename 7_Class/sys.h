#ifndef _SYS_H
#define _SYS_H

#include "stdio.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

/*****************************Sales****************************************/
class Sales_data {

    friend std::istream& read (std::istream &is,Sales_data &item);
    friend std::ostream& print (std::ostream &os,const Sales_data &item);
    friend void add(Sales_data &lhs, Sales_data &rhs);

public:
    //constructor fun
    Sales_data();
    Sales_data(std::string s,unsigned SalesNum,double price) \
        :bookNo(s),units_sold(SalesNum),revenue(SalesNum*price){};
    Sales_data(std::istream &is);

    std::string isbn() const;
    double avg_price() const;
    Sales_data& combine(const Sales_data &rhs);

private:
    std::string bookNo;
    unsigned units_sold; // SalesNumber
    double revenue; // TotalPrice

protected:

};

std::istream& read (std::istream &is,Sales_data &item);
std::ostream& print (std::ostream &os,const Sales_data &item);
//Sales_data add(const Sales_data &lhs, const Sales_data &rhs);
void add(Sales_data &lhs, Sales_data &rhs);

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

/*****************************Screen****************************************/

class Screen {

public:
    using pos = std::string::size_type;

    Screen() =default;
    Screen(pos h,pos w):height(h),width(w),contents(h*w,'') {};
    Screen(pos h,pos w,char c):height(h),width(w),contents(h*w,c) {};
    char get() {
        return contents[cursor];
    }
    char get(pos h,pos w);
    Screen& move(pos r, pos c);
    void some_member() const;

private:
    pos height = 0;
    pos width = 0;
    std::string contents;
    unsigned cursor = 0; // cursor
    mutable unsigned access_ctr = 0; // variable data member

protected:

};

class Window_mgr {
public:

private:
    std::vector<Screen> screens { Screen(80,100,' ') };

protected:
};

#endif
