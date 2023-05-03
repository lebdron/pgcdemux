#include "PgcDemux.h"

extern CPgcDemuxApp theApp;

int main(int argc, char *argv[])
    {
    theApp.InitInstance(argc, argv);
    return theApp.ExitInstance();
    }
