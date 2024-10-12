
#pragma once

#include <optional>
#include <string>

#include "SourceLocation.h"

enum class ErrorType {
    Unknown,
    Lexer,
    Parser,
};

class Error {
public:
    Error(
        const ErrorType& type,
        const std::optional<std::string>& message)
        : m_type(type)
        , m_message(message)
    {
    }

    Error(
        ErrorType&& type,
        std::optional<std::string>&& message)
        : m_type(std::move(type))
        , m_message(std::move(message))
    {
    }

    const ErrorType type() const { return m_type; }

    const std::optional<std::string> message() const { return m_message; }

private:
    const ErrorType m_type;

    const std::optional<std::string> m_message;
};

class SourceError : public Error {
public:
    SourceError(
        const ErrorType& type,
        const std::optional<std::string>& message,
        const SourceLocation& location)
        : Error(type, message)
        , m_location(location)
    {
    }

    SourceError(
        ErrorType&& type,
        std::optional<std::string>&& message,
        SourceLocation&& location)
        : Error(std::move(type), std::move(message))
        , m_location(std::move(location))
    {
    }

    const SourceLocation& location() const { return m_location; }

private:
    const SourceLocation m_location;
};