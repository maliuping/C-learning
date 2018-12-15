#include "sys.h"

/*
C++ 中保留了C语言的 struct 关键字，并且加以扩充。在C语言中，struct 只能包含成员变量，不能包含成员函数。
而在C++中，struct 类似于 class，既可以包含成员变量，又可以包含成员函数。


C++中的 struct 和 class 基本是通用的，唯有几个细节不同：
1:使用 class 时，类中的成员默认都是 private 属性的；而使用 struct 时，结构体中的成员默认都是 public 属性的。(最本质的区别)
2:class 继承默认是 private 继承，而 struct 继承默认是 public 继承（《C++继承与派生》一章会讲解继承）。
3:class 可以使用模板，而 struct 不能。
*/


class Student {

public:
    void showname() {
        std::cout <<"name: "<<m_name <<" age: "<<m_age <<" scores: "<<m_scores<<std::endl;
    }

     // constructor fun
    Student (std::string name,int age,int scores):m_name(name),m_age(age),m_scores(scores){
    };

private:
    std::string m_name;
    int m_age;
    int m_scores;// 定义三个成员变量
protected:
};

/*
总结：
1：struct作为数据结构的实现体，它默认的数据访问控制是public的，而class作为对象的实现体，它默认的成员变量访问控制是private的
2：当你觉得你要做的更像是一种数据结构的话，那么用struct，如果你要做的更像是一种对象的话，那么用class。
3：然而对于访问控制，应该在程序里明确的指出，而不是依靠默认，这是一个良好的习惯，也让你的代码更具可读性
*/

int student (int argc, char **argv) {
    Student stu1("晓明",18,1);
    stu1.showname();

    return 0;
}
