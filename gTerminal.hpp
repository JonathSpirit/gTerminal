#ifndef GTERMINAL_GTERMINAL_HPP
#define GTERMINAL_GTERMINAL_HPP

#include <string>
#include <cstdint>

#ifndef _WIN32
    #define GTERMINAL_API
#else
    #ifdef GTERMINAL_EXPORTS
        #define GTERMINAL_API __declspec(dllexport)
    #else
        #define GTERMINAL_API __declspec(dllimport)
    #endif //GTERMINAL_EXPORTS
#endif //_WIN32

namespace gt
{

class GTERMINAL_API Terminal
{
public:
    Terminal() = default;
    ~Terminal() = default;

    [[nodiscard]] bool init();

    std::string read();

private:
    void* g_internalInputHandle{nullptr};
    void* g_internalOutputHandle{nullptr};
};

} //namespace gt

#endif //GTERMINAL_GTERMINAL_HPP
