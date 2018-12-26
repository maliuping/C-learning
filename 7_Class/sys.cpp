#include "sys.h"

/*****************************Sales****************************************/
Sales_data::Sales_data() {
    units_sold = 0;
    revenue = 0.0;// it do not initialise when variable is defined in class
}

std::string Sales_data::isbn() const {
    return bookNo;
}

Sales_data& Sales_data::combine(const Sales_data &rhs) {

    units_sold += rhs.units_sold; // the number of sales
    revenue += rhs.revenue; // the total price
    return *this;
}

std::istream& read (std::istream &is,Sales_data &item) {
    double price = 0.0;
    is >> item.bookNo >> item.units_sold >> price;
    item.revenue = price * item.units_sold;

    return is;
}

std::ostream& print (std::ostream &os,const Sales_data &item) {
    os << item.isbn() << " " << item.units_sold << " " <<item.revenue;

    return os;
}

/* Sales_data add(const Sales_data &lhs, const Sales_data &rhs) {
    Sales_data sum = lhs; // copy data members of lhs to sum
    sum.combine(rhs); //add data members of rhs to sum

    return sum;
} */

void add(Sales_data &lhs, Sales_data &rhs) {
    lhs.combine(rhs); //add data members of rhs to sum
}

/*****************************Person****************************************/
std::string Person_data::getname() const {
    return m_name;
}

std::string Person_data::getaddress() const {
    return m_address;
}

 void Person_data::showname() {
        std::cout <<"name: "<<m_name <<" "<<"address: "<<m_address <<std::endl;
}