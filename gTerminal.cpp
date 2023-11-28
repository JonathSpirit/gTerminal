#include "gTerminal.hpp"

#include <iostream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace gt
{

bool Terminal::init()
{
#ifdef _WIN32
    HANDLE stdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE stdInHandle = GetStdHandle(STD_INPUT_HANDLE);

    if (stdOutHandle == INVALID_HANDLE_VALUE ||
        stdInHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    this->g_internalOutputHandle = stdOutHandle;
    this->g_internalInputHandle = stdInHandle;

    DWORD dwMode;
    if (GetConsoleMode(stdOutHandle, &dwMode) == 0)
    {
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    return SetConsoleMode(stdOutHandle, dwMode) != 0;
#endif
}

std::string Terminal::read()
{
    INPUT_RECORD records[10];
    DWORD read = 0;

    if (WaitForSingleObject(this->g_internalInputHandle, 0) != WAIT_OBJECT_0)
    {
        return {};
    }

    auto errOut = ReadConsoleInput(this->g_internalInputHandle,
                                   records, sizeof(records)/sizeof(INPUT_RECORD), &read);

    if (errOut == 0)
    {
        return {};
    }

    std::string result;

    for (DWORD i=0; i<read; ++i)
    {
        if (records[i].EventType == KEY_EVENT)
        {
            if (records[i].Event.KeyEvent.bKeyDown)
            {
                result.push_back(records[i].Event.KeyEvent.uChar.AsciiChar);
            }
        }
    }

    return result;
}

} //namespace gt