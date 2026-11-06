#include <iostream>

class obj
{
public:
    obj() { std::cout << 1 << std::endl; }
    obj(const obj&) { std::cout << 2 << std::endl; }
    void operator=(const obj&) { std::cout << 3 << std::endl; }
};

int main(void)
{
    obj a; // 1
    obj b{}; // 1
    obj c();
    obj d = a; // 2 copy constructor, assignment only when existing obj = existing obj;
    d = b; // 3
    auto e = obj{}; // 1
    obj f{a}; // 2
    obj g(a); // 2
    auto h = b; // 2
    obj(i); // what 
}