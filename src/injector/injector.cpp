#include <iostream>
#include <string>
#include "injector.hpp"

using namespace std;

int main(int argc, char *argv[])
{
    if (argc < 3) {
        cout << "expected dll path and process pid" << endl;
    }

    const char *dll = argv[1];
    DWORD pid = stoul(argv[2]);

    cout << "inject '" << dll << "' to process " << pid << endl;

    if (injector::inject(pid, dll)) {
        cout << "success" << endl;
    }
    else {
        cout << "failed" << endl;
    }
}
