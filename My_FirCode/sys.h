#ifndef _SYS_H
#define _SYS_H

#include "stdio.h"
#include <iostream>
#include <map>

namespace merbok {
    class Rvctool {
    public:
       /*  Rvctool();
        ~Rvctool(); */
    private:
        int a;
        int b;

    public:
    void print();



    };
}

struct Sales_data {
	double price;
	std::string booknum;
	unsigned units_sold;	
};

#endif