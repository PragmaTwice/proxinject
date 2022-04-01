#include <iostream>
#include <string>
#include <filesystem>
#include "injector.hpp"
#include "rpc.hpp"

using namespace std;

int main(int argc, char *argv[])
{
    if (argc < 2) {
        cout << "expected process pid" << endl;
    }

    
    DWORD pid = stoul(argv[1]);

    if (injector::inject(pid)) {
        cout << "success" << endl;
    }
    else {
        cout << "failed" << endl;
    }
}
