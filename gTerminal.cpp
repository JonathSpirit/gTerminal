#include "gTerminal.hpp"

#include <iostream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
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

    DWORD dwMode;
    if (GetConsoleMode(stdOutHandle, &dwMode) == 0)
    {
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    return SetConsoleMode(stdOutHandle, dwMode) != 0;
#endif
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
    }
}
void Terminal::render() const
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);

    std::cout << CSI_CURSOR_POSITION(1, 1) << CSI_ERASE_DISPLAY(0);
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