#include "named-pipe.hpp"
#include <iostream>

os::apple::named_pipe::named_pipe(os::create_only_t, const std::string name)
{
    int ret = 0;

    unlink(name.c_str());
    ret = mkfifo(name.c_str(), S_IRUSR | S_IWUSR| S_IFIFO);

    if (ret < 0)
        throw "Could not create named pipe.";
}

os::apple::named_pipe::named_pipe(os::open_only_t, const std::string name)
{
    int ret = 0;
    ret = open(name.c_str(), O_RDWR);

    if (ret < 0)
        throw "Could not open named pipe.";
}
