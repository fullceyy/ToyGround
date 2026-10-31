#include "Defer.h"

#include <print>

void func(int x)
{
    std::println("{}", x);
}

int main(void)
{
    int x = 5;
    
    {
        Defer(x += 1);
        func(x);
    }
    
    func(x);
    return 0;
}