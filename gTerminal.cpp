#include "gTerminal.hpp"

#include <iostream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/ioctl.h>
#endif

namespace gt
{

Terminal::Terminal()
{
    this->g_defaultOutputStream = this->g_elements.end();
}

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

    CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
    if (GetConsoleScreenBufferInfo(stdOutHandle, &bufferInfo) != TRUE)
    {
        return false;
    }

    this->g_bufferSize._width = bufferInfo.dwSize.X;
    this->g_bufferSize._height = bufferInfo.dwSize.Y;

    DWORD dwMode;
    if (GetConsoleMode(stdOutHandle, &dwMode) != TRUE)
    {
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    return SetConsoleMode(stdOutHandle, dwMode) != 0;
#else
    this->g_internalInputHandle._desc = fileno(stdin);
    this->g_internalOutputHandle._desc = fileno(stdout);

    if (this->g_internalInputHandle._desc == -1 ||
        this->g_internalOutputHandle._desc == -1)
    {
        return false;
    }

    winsize w{};
    if ( ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != 0 )
    {
        return false;
    }

    this->g_bufferSize._width = w.ws_col;
    this->g_bufferSize._height = w.ws_row;

    return true;
#endif
}

BufferSize Terminal::getBufferSize() const
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);
    return this->g_bufferSize;
}

void Terminal::addElement(std::unique_ptr<Element>&& element)
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);

    auto& ref = this->g_elements.emplace_back(std::move(element));

    ref->setTerminal(this);

    if (ref->haveOutputStream() && this->g_defaultOutputStream == this->g_elements.end())
    {
        this->g_defaultOutputStream = this->g_elements.end();
        --this->g_defaultOutputStream;
    }
}

void Terminal::update()
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);

#ifdef _WIN32
    INPUT_RECORD records[10];
    DWORD read = 0;

    if (WaitForSingleObject(this->g_internalInputHandle, 0) != WAIT_OBJECT_0)
    {
        return;
    }

    auto errOut = ReadConsoleInput(this->g_internalInputHandle,
                                   records, sizeof(records)/sizeof(INPUT_RECORD), &read);

    if (errOut == 0)
    {
        return;
    }

    for (DWORD i=0; i<read; ++i)
    {
        if (records[i].EventType == KEY_EVENT)
        {
            KeyEvent keyEvent{records[i].Event.KeyEvent.bKeyDown==TRUE,
                              records[i].Event.KeyEvent.wRepeatCount,
                              records[i].Event.KeyEvent.wVirtualKeyCode,
                              records[i].Event.KeyEvent.wVirtualScanCode,
                              records[i].Event.KeyEvent.uChar.AsciiChar,
                              records[i].Event.KeyEvent.dwControlKeyState};

            for (auto& element : this->g_elements)
            {
                if (element->haveInputStream())
                {
                    element->onKeyInput(keyEvent);
                }
            }
        }
        else if (records[i].EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            this->g_bufferSize._width = records[i].Event.WindowBufferSizeEvent.dwSize.X;
            this->g_bufferSize._height = records[i].Event.WindowBufferSizeEvent.dwSize.Y;
        }
    }
#else

#endif //_WIN32
}
void Terminal::render() const
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);

    std::cout << CSI_CURSOR_POSITION(1, 1) << CSI_ERASE_DISPLAY(0) << CSI_ERASE_DISPLAY(3);
    for (const auto& element : this->g_elements)
    {
        element->render();
    }
    std::cout << std::flush;
}

void TextOutputStream::render() const
{
    for (const auto& str : this->g_textBuffer)
    {
        std::cout << str;
    }
}

void TextOutputStream::onInput(std::string_view str)
{
    this->g_textBuffer.emplace_back(str);
}

void TextInputStream::render() const
{
    std::cout << "\n" CSI_COLOR_FG_GREEN "INPUT> " CSI_COLOR_NORMAL << this->g_inputBuffer;
}

void TextInputStream::onKeyInput(KeyEvent const& keyEvent)
{
    if (keyEvent._keyDown)
    {
        if (keyEvent._asciiChar == '\r')
        {
            this->getTerminal()->output("%s\n", this->g_inputBuffer.data());
            this->g_inputBuffer.clear();
            return;
        }

        this->g_inputBuffer.push_back(keyEvent._asciiChar);
    }
}

} //namespace gt