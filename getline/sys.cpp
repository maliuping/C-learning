#include "sys.h"

std::string ReadOneLine () {
    std::string line;
    while (getline(std::cin,line)) {
        if (!line.empty()) {
            //std::cout << line << std::endl;
            return line;
        } else {
            break;
        }
    }
    return 0;
}

void ReadOneWord (std::string word) {
     //不能保留输入时留下的空白。逐个输出单词，每个单词后面紧跟一个换行。
    while (std::cin >> word) {
        //std::cout << word << std::endl;
    }
}