#include <cctype>
#include "sys.h"

int main(int argc,char *argv[]) {

    Sales_data total;
    Sales_data trans;

    //if(std::cin >> total.bookNo >> total.units_sold >> price) {
    if(read(std::cin,total)) {
        //total.revenue = price * total.units_sold; // calculate the total price

        //while (std::cin >> trans.bookNo >> trans.units_sold >> price) {
        while (read(std::cin,trans)) {
            //trans.revenue = price * trans.units_sold; // Calculate the total price
            if (trans.isbn() == total.isbn()) {
                //total.combine(trans);
                add(total,trans);
            } else {
                //std::cout << total.isbn() << " "<< total.units_sold <<" "<< total.revenue <<std::endl;// print the sales price of the previous book
                print(std::cout,total);
                std::cout << std::endl;
                total = trans;
            }
        }
        //std::cout << total.isbn() <<" "<< total.units_sold <<" "<<total.revenue <<std::endl;
        print(std::cout,total);
        std::cout << std::endl;
    } else {
        std::cout << "No data!"<<std::endl;
        return -1;
    }

    return 0;
}
