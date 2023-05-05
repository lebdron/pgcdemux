#include "spdlog/spdlog.h"

#include "PgcDemux.h"

extern CPgcDemuxApp theApp;

int main(int argc, char *argv[])
    {
    spdlog::set_pattern("[source %s] [function %!] [line %#] %v");
    theApp.InitInstance(argc, argv);
    return theApp.ExitInstance();
    }
