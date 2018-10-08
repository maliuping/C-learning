#include <stdio.h>
#include <iostream>
#include <string>
#include "sys.h"



int main() {


    //double average_price = 0;

    Sales_data sample1,sample2;
    std::cout << "请依次输入书号、价格、售出数量" << std::endl;
    std::cin >> sample1.booknum >> sample1.price >> sample1.units_sold;
    std::cout << "书号: " << sample1.booknum << " 价格: " << sample1.price \
                << " 售出数量: " << sample1.units_sold << std::endl;
    //std::cout << "价格: " << sample1.price << std::endl;
    //std::cout << "售出数量: " << sample1.units_sold << std::endl;

    std::cout << "请依次输入书号、价格、售出数量" << std::endl;
    std::cin >> sample2.booknum >> sample2.price >> sample2.units_sold;
    std::cout << "书号: " << sample2.booknum << " 价格: " << sample2.price \
                << " 售出数量: " << sample2.units_sold << std::endl;

    if (sample1.booknum == sample2.booknum) {
        unsigned int total_sold = sample1.units_sold + sample2.units_sold;
        double total_price = sample1.units_sold * sample1.price + sample2.units_sold * sample2.price;
        if (total_sold != 0) {
            std::cout << "平均售价: " << total_price/total_sold << std::endl;
        } else
            std::cout << "没有售出！" << std::endl;
    } else {
        std::cout << "书号不一致" << std::endl;
    }

}