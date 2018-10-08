#include "sys.h"

int main() {

    std::string line1;
    std::string line2;
    std::string word;

    line1 = ReadOneLine();
     /* std::cout << line1 << std::endl; */
    line2 = ReadOneLine();
    if (line1 == line2) {
        std::cout << line1 << std::endl;
    } else if (line1 > line2) {
        std::cout << line1 << std::endl;
    } else {
        std::cout << line2 << std::endl;
    }
   /*  int a, b;
    a = b =0; */

    return 0;
}