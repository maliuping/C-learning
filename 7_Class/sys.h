#ifndef _SYS_H
#define _SYS_H

#include "stdio.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

const std::vector<std::string> scores = {"F","D","C","B","A","A++"};

void ScoresToLevel(int grade,std::string *lettergrade);

class Sales_data {

public:
    std::string bookNo;
    unsigned units_sold;
    double revenue;

    //constructor fun
    Sales_data(); //{
        //units_sold = 0;
        //revenue = 0.0;// it do not initialise when variable is defined in class
    //};

};

#endif