namespace gt
{

template<class ...TArgs>
void Terminal::output(std::string_view format, TArgs&&... args)
{
    if (format.empty())
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> const lock(this->g_mutex);

    if (this->g_defaultOutputStream == this->g_elements.end())
    {
        return;
    }

    std::string str;

    auto size = std::snprintf(nullptr, 0, format.data(), std::forward<TArgs>(args)...);

    if (size == 0)
    {
        return;
    }

    str.resize(size+1);

    std::snprintf(str.data(), str.size(), format.data(), std::forward<TArgs>(args)...);

    this->g_defaultOutputStream->get()->onInput(str);
}

template<class TElement, class ...TArgs>
TElement* Terminal::addElement(TArgs&&... args)
{
    return static_cast<TElement*>(this->addElement(std::make_unique<TElement>(std::forward<TArgs>(args)...)));
}

} //namespace gt