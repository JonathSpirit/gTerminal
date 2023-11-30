#ifndef GTERMINAL_GTERMINAL_HPP
#define GTERMINAL_GTERMINAL_HPP

#include <string>
#include <string_view>
#include <cstdint>
#include <memory>
#include <list>
#include <vector>
#include <mutex>

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

#define CSI_COLOR_NORMAL "\x1b[0m"
#define CSI_COLOR_FG_BLACK "\x1b[30m"
#define CSI_COLOR_BG_BLACK "\x1b[40m"
#define CSI_COLOR_FG_RED "\x1b[31m"
#define CSI_COLOR_BG_RED "\x1b[41m"
#define CSI_COLOR_FG_GREEN "\x1b[32m"
#define CSI_COLOR_BG_GREEN "\x1b[42m"

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
    uint16_t _width;
    uint16_t _height;
};

class Terminal;

class Element
{
public:
    Element() = default;
    ~Element() = default;

    inline virtual void render() const {}

    [[nodiscard]] inline virtual bool haveOutputStream() const { return false; }
    [[nodiscard]] inline virtual bool haveInputStream() const { return false; }

    //Event
    inline virtual void onInput(std::string_view str) {}
    inline virtual void onKeyInput(KeyEvent const& keyEvent) {}

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
    ~TextOutputStream() = default;

    void render() const override;

    [[nodiscard]] inline bool haveOutputStream() const override { return true; }

    //Event
    void onInput(std::string_view str) override;

private:
    std::vector<std::string> g_textBuffer;
};

class GTERMINAL_API TextInputStream : public Element
{
public:
    TextInputStream() = default;
    ~TextInputStream() = default;

    void render() const override;

    [[nodiscard]] inline bool haveInputStream() const override { return true; }

    //Event
    void onKeyInput(KeyEvent const& keyEvent) override;

private:
    std::string g_inputBuffer;
};

class GTERMINAL_API Terminal
{
public:
    Terminal();
    ~Terminal();

    [[nodiscard]] bool init();

    [[nodiscard]] BufferSize getBufferSize() const;

    //Output stream
    template<class ...TArgs>
    void output(std::string_view format, TArgs&&... args);

    //Element
    void addElement(std::unique_ptr<Element>&& element);

    void update();
    void render() const;

private:
    using ElementList = std::list<std::unique_ptr<Element> >;
    union Handle
    {
        void* _ptr;
        int _desc;
    };

    Handle g_internalInputHandle{nullptr};
    Handle g_internalOutputHandle{nullptr};

    ElementList g_elements;
    ElementList::const_iterator g_defaultOutputStream;

    BufferSize g_bufferSize{0,0};

    mutable std::recursive_mutex g_mutex;
};

} //namespace gt

#include "gTerminal.inl"

#endif //GTERMINAL_GTERMINAL_HPP
