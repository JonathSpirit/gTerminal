/*
 * MIT License
 * Copyright (c) 2024 Guillaume Guillet
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "gTerminal.hpp"

#include <iostream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <termios.h>
#endif

namespace gt
{

namespace
{
#ifndef _WIN32

//https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

termios gOriginalTermios;

[[nodiscard]] bool EnableRawMode(int fd)
{
    termios raw{};
    if ( tcgetattr(fd, &raw) != 0 )
    {
        return false;
    }

    gOriginalTermios = raw;

    raw.c_lflag &= ~(ECHO); //Disable echo
    raw.c_lflag &= ~(ICANON); //Disable canonical mode
    //raw.c_oflag &= ~(OPOST); //Disable post-processing of output
    raw.c_cc[VMIN] = 0; //Minimum number of bytes of input needed before read() can return
    raw.c_cc[VTIME] = 0; //Maximum amount of time to wait before read() returns

    if ( tcsetattr(fd, TCSAFLUSH, &raw) != 0 )
    {
        return false;
    }
    return true;
}

[[nodiscard]] bool DisableRawMode(int fd)
{
    return tcsetattr(fd, TCSAFLUSH, &gOriginalTermios) == 0;
}

#endif //_WIN32
}//namespace

Terminal::Terminal()
{
    this->g_defaultOutputStream = this->g_elements.end();
}

#ifdef _WIN32
Terminal::~Terminal() = default;
#else
Terminal::~Terminal()
{
    (void) DisableRawMode(this->g_internalInputHandle._desc);
}
#endif //_WIN32

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

    this->g_internalOutputHandle._ptr = stdOutHandle;
    this->g_internalInputHandle._ptr = stdInHandle;

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
    if ( ioctl(this->g_internalOutputHandle._desc, TIOCGWINSZ, &w) != 0 )
    {
        return false;
    }

    this->g_bufferSize._width = w.ws_col;
    this->g_bufferSize._height = w.ws_row;

    return EnableRawMode(this->g_internalInputHandle._desc);
#endif
}

BufferSize Terminal::getTerminalBufferSize() const
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);
    return this->g_bufferSize;
}

void Terminal::clearTerminalBuffer()
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);
    std::cout << CSI_CURSOR_POSITION(1, 1) << CSI_ERASE_DISPLAY(0) << CSI_ERASE_DISPLAY(3) << std::flush;
}
void Terminal::saveCursorPosition()
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);
    std::cout << CSI_SAVE_CURSOR_POSITION << std::flush;
}
void Terminal::restoreCursorPosition()
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);
    std::cout << CSI_RESTORE_CURSOR_POSITION << std::flush;
}

Element* Terminal::addElement(std::unique_ptr<Element>&& element)
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);

    auto& ref = this->g_elements.emplace_back(std::move(element));

    ref->setTerminal(this);

    if (ref->haveOutputStream() && this->g_defaultOutputStream == this->g_elements.end())
    {
        this->g_defaultOutputStream = this->g_elements.end();
        --this->g_defaultOutputStream;
    }

    return ref.get();
}

void Terminal::update()
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);

#ifdef _WIN32
    INPUT_RECORD records[10];
    DWORD read = 0;

    if (WaitForSingleObject(this->g_internalInputHandle._ptr, 0) != WAIT_OBJECT_0)
    {
        return;
    }

    auto errOut = ReadConsoleInput(this->g_internalInputHandle._ptr,
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
    winsize w{};
    if ( ioctl(this->g_internalOutputHandle._desc, TIOCGWINSZ, &w) == 0 )
    {///TODO not great, I prefer events
        this->g_bufferSize._width = w.ws_col;
        this->g_bufferSize._height = w.ws_row;
    }

    uint8_t buffer[10];
    auto result = read(this->g_internalInputHandle._desc, &buffer, 10);
    if (result == -1 || result == 0)
    {
        return;
    }

    for (decltype(result) i=0; i<result; ++i)
    {
        auto c = static_cast<char>(buffer[i]);

        KeyEvent keyEvent{true, 1,
                          0, 0,
                          c, 0};

        for (auto& element : this->g_elements)
        {
            if (element->haveInputStream())
            {
                element->onKeyInput(keyEvent);
            }
        }
    }
#endif //_WIN32
}
void Terminal::render() const
{
    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);

    std::cout << CSI_CURSOR_POSITION(1, 1) << CSI_ERASE_DISPLAY(0) << CSI_ERASE_DISPLAY(3);
    if (this->g_rowOffset > 0)
    {
        std::cout << CSI_CURSOR_POSITION_STREAM(this->g_rowOffset+1, 1);
    }

    for (const auto& element : this->g_elements)
    {
        element->render();
    }
    std::cout << std::flush;
}

void Terminal::setRowOffset(uint16_t offset)
{
    this->g_rowOffset = offset;
}
uint16_t Terminal::getRowOffset() const
{
    return this->g_rowOffset;
}

void TextOutputStream::render() const
{
    for (const auto& str : this->g_textBuffer)
    {
        std::cout << str;
    }
}

void TextOutputStream::setBufferLimit(std::size_t limit)
{
    this->g_bufferLimit = limit;
}
std::size_t TextOutputStream::getBufferLimit() const
{
    return this->g_bufferLimit;
}

void TextOutputStream::clear()
{
    this->g_textBuffer.clear();
}

void TextOutputStream::onInput(std::string_view str)
{
    this->g_textBuffer.emplace_back(str);
    if (this->g_bufferLimit != 0 &&
        this->g_textBuffer.size() > this->g_bufferLimit)
    {
        this->g_textBuffer.erase(this->g_textBuffer.begin());
    }
}

void TextInputStream::render() const
{
    std::cout << CSI_COLOR_FG_GREEN "INPUT> " CSI_COLOR_NORMAL << this->g_inputBuffer;
}

void TextInputStream::onKeyInput(KeyEvent const& keyEvent)
{
    if (keyEvent._keyDown)
    {
        //Enter
#ifdef _WIN32
        if (keyEvent._asciiChar == '\r')
#else
        if (keyEvent._asciiChar == '\n')
#endif //_WIN32
        {
            if (this->g_inputBuffer.empty())
            {
                return;
            }

            this->getTerminal()->output("%s\n", this->g_inputBuffer.c_str());

            this->_onInput.call(this->g_inputBuffer);
            this->g_inputBuffer.clear();
            return;
        }
        //Backspace
#ifdef _WIN32
        else if (keyEvent._asciiChar == '\b')
#else
        else if (keyEvent._asciiChar == 127)
#endif //_WIN32
        {
            if (!this->g_inputBuffer.empty())
            {
                this->g_inputBuffer.pop_back();
            }
            return;
        }
        //Unhandled control
        else if (iscntrl(keyEvent._asciiChar) != 0)
        {
            return;
        }

        this->g_inputBuffer.push_back(keyEvent._asciiChar);
    }
}

Banner::Banner(std::string_view banner) :
        g_banner{banner}
{}

void Banner::render() const
{
    this->getTerminal()->saveCursorPosition();
    if (this->g_centered)
    {
        auto size = this->getTerminal()->getTerminalBufferSize();
        unsigned int col = this->g_banner.size() >= size._width ? 1 : ((size._width - this->g_banner.size())/2 + 1);
        std::cout << CSI_CURSOR_POSITION_STREAM(1, col);
    }
    else
    {
        std::cout << CSI_CURSOR_POSITION(1, 1);
    }
    std::cout << CSI_COLOR_BG_WHITE << CSI_COLOR_FG_BLACK;
    std::cout << ' ' << this->g_banner << ' ' << CSI_COLOR_NORMAL;
    this->getTerminal()->restoreCursorPosition();
}

void Banner::setBanner(std::string_view banner)
{
    this->g_banner = banner;
}
std::string const& Banner::getBanner() const
{
    return this->g_banner;
}

void Banner::setCenterFlag(bool centered)
{
    this->g_centered = centered;
}
bool Banner::isCentered() const
{
    return this->g_centered;
}

} //namespace gt