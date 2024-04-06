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

    auto size = std::snprintf(nullptr, 0, format.data(), std::forward<TArgs>(args)...);

    if (size == 0)
    {
        return;
    }

    std::string str(size+1, '\0');

    std::snprintf(str.data(), str.size(), format.data(), std::forward<TArgs>(args)...);

    this->g_defaultOutputStream->get()->onInput(str);
}

template<class TElement, class ...TArgs>
TElement* Terminal::addElement(TArgs&&... args)
{
    return static_cast<TElement*>(this->addElement(std::make_unique<TElement>(std::forward<TArgs>(args)...)));
}

} //namespace gt