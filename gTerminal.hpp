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

#ifndef GTERMINAL_GTERMINAL_HPP
#define GTERMINAL_GTERMINAL_HPP

#include <string>
#include <string_view>
#include <cstdint>
#include <memory>
#include <list>
#include <vector>
#include <mutex>
#include <functional>

#ifndef _WIN32
    #define GTERMINAL_API
#else
    #ifdef GTERMINAL_EXPORTS
        #define GTERMINAL_API __declspec(dllexport)
    #else
        #define GTERMINAL_API __declspec(dllimport)
    #endif //GTERMINAL_EXPORTS
#endif //_WIN32

#define CSI_ERASE_DISPLAY(_n) ("\x1b[" #_n "J")
#define CSI_CURSOR_POSITION(_row, _col) ("\x1b[" #_row ";" #_col "H")
#define CSI_CURSOR_POSITION_STREAM(_row, _col) "\x1b[" << _row << ';' << _col << 'H'

#define CSI_SAVE_CURSOR_POSITION "\x1b[s"
#define CSI_RESTORE_CURSOR_POSITION "\x1b[u"

#define CSI_COLOR_NORMAL "\x1b[0m"
#define CSI_COLOR_FG_BLACK "\x1b[30m"
#define CSI_COLOR_BG_BLACK "\x1b[40m"
#define CSI_COLOR_FG_RED "\x1b[31m"
#define CSI_COLOR_BG_RED "\x1b[41m"
#define CSI_COLOR_FG_GREEN "\x1b[32m"
#define CSI_COLOR_BG_GREEN "\x1b[42m"
#define CSI_COLOR_FG_YELLOW "\x1b[33m"
#define CSI_COLOR_BG_YELLOW "\x1b[43m"
#define CSI_COLOR_FG_BLUE "\x1b[34m"
#define CSI_COLOR_BG_BLUE "\x1b[44m"
#define CSI_COLOR_FG_MAGENTA "\x1b[35m"
#define CSI_COLOR_BG_MAGENTA "\x1b[45m"
#define CSI_COLOR_FG_CYAN "\x1b[36m"
#define CSI_COLOR_BG_CYAN "\x1b[46m"
#define CSI_COLOR_FG_WHITE "\x1b[37m"
#define CSI_COLOR_BG_WHITE "\x1b[47m"

namespace gt
{

struct KeyEvent
{
    bool _keyDown;
    uint16_t _repeatCount;
    uint16_t _virtualKeyCode;
    uint16_t _virtualScanCode;
    char _asciiChar;
    uint32_t _controlKeyState;
};

struct BufferSize
{
    using ValueType = uint16_t;
    ValueType _width;
    ValueType _height;

    [[nodiscard]] constexpr bool operator==(BufferSize const& other) const
    {
        return this->_width == other._width && this->_height == other._height;
    }
    [[nodiscard]] constexpr bool operator!=(BufferSize const& other) const
    {
        return !(*this == other);
    }
};

class Terminal;

template<class ...TArgs>
class CallbackHandler
{
public:
    CallbackHandler() = default;
    ~CallbackHandler() = default;

    inline void add(std::function<void(TArgs...)> callback, void* owner=nullptr)
    {
        this->g_callbacks.emplace_back(std::move(callback), owner);
    }
    inline void remove(void* owner)
    {
        for (auto const& callback : this->g_callbacks)
        {
            if (callback._owner == owner)
            {
                this->g_callbacks.erase(callback);
                return;
            }
        }
    }
    inline void clear()
    {
        this->g_callbacks.clear();
    }

    inline void call(TArgs&&... args) const
    {
        for (auto const& callback : this->g_callbacks)
        {
            callback._callback(std::forward<TArgs>(args)...);
        }
    }

private:
    struct Callback
    {
        inline Callback(std::function<void(TArgs...)>&& callback, void* owner):
                _callback(std::move(callback)),
                _owner(owner)
        {}

        std::function<void(TArgs...)> _callback;
        void* _owner;
    };
    std::vector<Callback> g_callbacks;
};

class Element
{
public:
    Element() = default;
    virtual ~Element() = default;

    inline virtual void render() const {}

    [[nodiscard]] inline virtual bool haveOutputStream() const { return false; }
    [[nodiscard]] inline virtual bool haveInputStream() const { return false; }

    //Event
    inline virtual void onInput([[maybe_unused]] std::string_view str) {}
    inline virtual void onKeyInput([[maybe_unused]] KeyEvent const& keyEvent) {}
    inline virtual void onSizeChanged([[maybe_unused]] BufferSize size) {}

    [[nodiscard]] inline Terminal* getTerminal() const { return this->g_terminal; }

private:
    inline void setTerminal(Terminal* terminal) { this->g_terminal = terminal; }

    friend class Terminal;
    Terminal* g_terminal{nullptr};
};

class GTERMINAL_API TextOutputStream : public Element
{
public:
    TextOutputStream() = default;
    ~TextOutputStream() override = default;

    void render() const override;

    [[nodiscard]] inline bool haveOutputStream() const override { return true; }

    void setBufferLimit(std::size_t limit);
    [[nodiscard]] std::size_t getBufferLimit() const;

    void clear();

    //Event
    void onInput(std::string_view str) override;

private:
    std::vector<std::string> g_textBuffer;
    std::size_t g_bufferLimit{0};
};

class GTERMINAL_API TextInputStream : public Element
{
public:
    TextInputStream() = default;
    ~TextInputStream() override = default;

    void render() const override;

    [[nodiscard]] inline bool haveInputStream() const override { return true; }

    //Event
    void onKeyInput(KeyEvent const& keyEvent) override;

    //Callback
    CallbackHandler<std::string_view> _onInput;

private:
    std::string g_inputBuffer;
};

class GTERMINAL_API Banner : public Element
{
public:
    Banner() = default;
    explicit Banner(std::string_view banner);
    ~Banner() override = default;

    void render() const override;

    void setBanner(std::string_view banner);
    [[nodiscard]] std::string const& getBanner() const;

    void setCenterFlag(bool centered);
    [[nodiscard]] bool isCentered() const;

private:
    std::string g_banner;
    bool g_centered{true};
};

class GTERMINAL_API Terminal
{
public:
    Terminal();
    ~Terminal();

    [[nodiscard]] bool init();

    //Control
    [[nodiscard]] BufferSize getTerminalBufferSize() const;

    void clearTerminalBuffer();
    void saveCursorPosition();
    void restoreCursorPosition();

    //Output stream
    template<class ...TArgs>
    void output(std::string_view format, TArgs&&... args);

    //Element
    Element* addElement(std::unique_ptr<Element>&& element);
    template<class TElement, class ...TArgs>
    TElement* addElement(TArgs&&... args);

    void update();
    void render() const;
    void invalidate() const;

    void setRowOffset(uint16_t offset);
    [[nodiscard]] uint16_t getRowOffset() const;

private:
    using ElementList = std::list<std::unique_ptr<Element> >;
    union Handle
    {
        void* _ptr;
        int _desc;
    };

    mutable bool g_invalidRender{true};

    Handle g_internalInputHandle{nullptr};
    Handle g_internalOutputHandle{nullptr};

    ElementList g_elements;
    ElementList::const_iterator g_defaultOutputStream;

    BufferSize g_bufferSize{0,0};

    uint16_t g_rowOffset{0};

    mutable std::recursive_mutex g_mutex;
};

} //namespace gt

#include "gTerminal.inl"

#endif //GTERMINAL_GTERMINAL_HPP
