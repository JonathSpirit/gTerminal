#include "gTerminal.hpp"
#include <iostream>
#include <thread>

int main(int argc, char** argv)
{
    gt::Terminal terminal;

    if (!terminal.init())
    {
        std::cout << "Failed to initialize terminal" << std::endl;
        return 1;
    }

    while(1)
    {
        auto input = terminal.read();
        if (!input.empty())
        {
            std::cout << input << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }


    return 0;
}
