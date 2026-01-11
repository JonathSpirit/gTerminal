#include "gTerminal.hpp"
#include <iostream>
#include <thread>

volatile bool gRunning = true;

void threadTest(gt::Terminal* terminal)
{
    unsigned int count = 0;
    auto id = std::hash<std::thread::id>{}(std::this_thread::get_id());

    while (gRunning)
    {
        std::cout << "std::cout > text from standard output\n";
        terminal->output("Thread (%u) test %u\n", id, count++);
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
}

int main(int argc, char** argv)
{
    gt::Terminal terminal;

    if (!terminal.init())
    {
        std::cout << "Failed to initialize terminal" << std::endl;
        return 1;
    }

    if (!terminal.redirectStandardOutputStream())
    {
        std::cout << "Failed to redirect standard output stream" << std::endl;
        return 1;
    }

    terminal.addElement<gt::TextOutputStream>()->setBufferLimit(20);
    terminal.addElement<gt::TextInputStream>()->_onInput.add([](std::string_view str)
    {
        if (str == "exit" || str == "quit")
        {
            gRunning = false;
        }
    });
    terminal.addElement<gt::Banner>("This is a test program ! With an interactive, thread safe terminal");

    terminal.setRowOffset(1);

    std::cout << "hello guys !" << std::endl;
    std::cout << "This text is coming from std::cout" << std::endl;

    std::thread thread1(threadTest, &terminal);
    std::thread thread2(threadTest, &terminal);

    while(gRunning)
    {
        terminal.update();

        terminal.render();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    thread1.join();
    thread2.join();

    return 0;
}
