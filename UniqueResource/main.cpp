#include "UniqueResource.h"

#include <cstdio>
#include <utility>
#include <vector>

using namespace ToyGround;

static void CloseFile(std::FILE* f)
{
    std::fclose(f);
    std::puts("  [file closed]");
}

using File = UniqueResource<std::FILE*>;   // sentinel nullptr, deleter void(*)(FILE*)

static File OpenFile(const char* path, const char* mode)
{
    File f{nullptr, &CloseFile};
    f.Reset(std::fopen(path, mode));
    return f;
}

int main()
{
    std::puts("1. scope-based release");
    {
        File log = OpenFile("example.txt", "w");
        if (!log)
            return 1;
        std::fputs("hello from UniqueResource\n", log);
    }

    std::puts("2. ownership moves, never copies");
    File a = OpenFile("example.txt", "r");
    File b = std::move(a);                                 // a is now empty
    std::printf("  a owns a file: %s, b owns a file: %s\n", a ? "yes" : "no", b ? "yes" : "no");

    char line[64]{};
    if (std::fgets(line, sizeof line, b))
        std::printf("  read back: %s", line);

    std::puts("3. Reset releases the old handle first");
    b.Reset(std::fopen("example.txt", "r"));               // old file closed, new one adopted

    std::puts("4. handles in a container");
    std::vector<File> files;
    for (int i = 0; i < 3; ++i)
        files.push_back(OpenFile("example.txt", "r"));
    std::printf("  %zu files open\n", files.size());
    files.clear();                                         // each closed exactly once

    std::puts("5. end of main");
    std::remove("example.txt");
    return 0;
}   // b closed here