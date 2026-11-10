#include "Exceptions.h"

using namespace ToyGround;

void Validate(int age)
{
    if (age < 0)
        throw CError{std::format("age cannot be negative, got {}", age)};
}

void LoadUser() { Validate(-5); }

int main()
{
    try
    {
       LoadUser();
    } catch (const CError& e) {
       e.Output();
    }
   return 0;
}

