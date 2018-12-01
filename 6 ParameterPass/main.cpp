#include <cctype>
#include "sys.h"

int main(int argc,char *argv[]) {

    std::string line;
    for (int i = 1; i != argc; ++i) {
        line += argv[i];
        line += " ";
    }

    std::cout << line <<std::endl;

}