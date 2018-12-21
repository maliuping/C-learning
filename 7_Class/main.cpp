#include <cctype>
#include "sys.h"

int main(int argc,char *argv[]) {

    Sales_data total;
    Sales_data trans;
    Sales_data sum;
    double price = 0.0;

    //if(std::cin >> total.bookNo >> total.units_sold >> price) {
    if(read(std::cin,total)) {
        //total.revenue = price * total.units_sold; // calculate the total price

        //while (std::cin >> trans.bookNo >> trans.units_sold >> price) {
        while (read(std::cin,trans)) {
            //trans.revenue = price * trans.units_sold; // Calculate the total price
            if (trans.isbn() == total.isbn()) {
                //sum = total.combine(trans);
                sum = add(total,trans);
            } else {
                std::cout << total.isbn() << " "<< total.units_sold <<" "<< total.revenue <<std::endl;// print the sales price of the previous book
                total = trans;
            }
        }
        std::cout << sum.isbn() <<" "<< sum.units_sold <<" "<<sum.revenue <<std::endl;
    } else {
        std::cout << "No data!"<<std::endl;
        return -1;
    }

    return 0;
}
