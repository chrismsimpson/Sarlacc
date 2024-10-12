
#pragma once

#include <utility>

struct SourceLocation {
    int start;
    int end;

    SourceLocation(int position)
        : start(position)
        , end(position)
    {
    }

    SourceLocation(int start, int end)
        : start(start)
        , end(end)
    {
    }
};

///

class Locatable {
public:
    Locatable(const SourceLocation& location)
        : m_location(location)
    {
    }

    Locatable(
        SourceLocation&& location)
        : m_location(std::move(location))
    {
    }

    Locatable& operator=(const Locatable&) = delete;

    Locatable&& operator=(Locatable&&) = delete;

    const SourceLocation& location() const
    {
        return m_location;
    }

private:
    SourceLocation m_location;
};