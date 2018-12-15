#include <cctype>
#include "sys.h"


Sales_data::Sales_data() {
    units_sold = 0;
    revenue = 0.0;// it do not initialise when variable is defined in class
}

int main(int argc,char *argv[]) {

    Sales_data total;
    if(std::cin >> total.bookNo >> total.units_sold >> total.revenue) {
        Sales_data trans;
        while (std::cin >> trans.bookNo >> trans.units_sold >> trans.revenue) {
            if (trans.bookNo == total.bookNo) {
                total.units_sold += trans.units_sold;
            } else {
                std::cout << total.bookNo << " "<< total.units_sold <<" "<< total.revenue <<std::endl;//打印前一本书的销售结果
                total = trans;
            }
        }
        std::cout << total.bookNo <<" "<< total.units_sold <<" "<<total.revenue <<std::endl;
    } else {
        std::cout << "No data!"<<std::endl;
        return -1;
    }

    return 0;
}