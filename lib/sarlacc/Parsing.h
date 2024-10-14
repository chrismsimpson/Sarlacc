
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <tuple>

#include "Error.h"

template <typename T>
class Lexer {
public:
    Lexer(const std::string& source)
        : m_source(source)
    {
    }

    Lexer(const Lexer&) = delete;

    Lexer& operator=(const Lexer&) = delete;

    ///

    bool isEof() const
    {
        return m_position >= m_source.size();
    }

    void increment(
        int amount = 1)
    {
        m_position += amount;
    }

    ///

    const std ::tuple<std::optional<char>, std::optional<Error>> peek() const
    {
        if (isEof()) {
            return { std::nullopt, Error(ErrorType::Lexer, "Unexpected end of file") };
        }

        return { m_source[m_position], std::nullopt };
    }

    const std::tuple<std::optional<std::string>, std::optional<Error>> peek(
        int length) const
    {
        const auto end = m_position + length;

        if (end > m_source.size()) {
            return { std::nullopt, Error(ErrorType::Lexer, "eof reached") };
        }

        return { m_source.substr(m_position, length), std::nullopt };
    }

    ///

    const bool match(
        char equals,
        int distance = 0) const
    {
        if (isEof()) {
            return false;
        }

        ///

        const auto length = 1 + distance;

        ///

        const auto end = m_position + length;

        if (end > m_source.size()) {
            return false;
        }

        ///

        const auto& peekTuple = peek(length);

        const auto& peekOrNull = std::get<std::optional<std::string>>(peekTuple);

        const auto& peekError = std::get<std::optional<Error>>(peekTuple);

        if (peekError.has_value()) {
            return false;
        }

        if (!peekOrNull.has_value()) {
            return false;
        }

        const auto& peek = peekOrNull.value();

        if (peek.size() != length) {
            return false;
        }

        ///

        return peek[distance] == equals;
    }

    const bool match(
        std::function<bool(char)> equals,
        int distance = 0) const
    {
        if (isEof()) {
            return false;
        }

        ///

        const auto length = 1 + distance;

        ///

        const auto end = m_position + length;

        if (end > m_source.size()) {
            return false;
        }

        ///

        const auto& peekTuple = peek(length);

        const auto& peekOrNull = std::get<std::optional<std::string>>(peekTuple);

        const auto& peekError = std::get<std::optional<Error>>(peekTuple);

        if (peekError.has_value()) {
            return false;
        }

        if (!peekOrNull.has_value()) {
            return false;
        }

        const auto& peek = peekOrNull.value();

        if (peek.size() != length) {
            return false;
        }

        ///

        return equals(peek[distance]);
    }

    const bool match(
        const std::string& equals,
        int distance = 0) const
    {
        if (isEof()) {
            return false;
        }

        ///

        const auto length = equals.size() + distance;

        ///

        const auto end = m_position + length;

        if (end > m_source.size()) {
            return false;
        }

        ///

        const auto& peekTuple = peek(length);

        const auto& peekOrNull = std::get<std::optional<std::string>>(peekTuple);

        const auto& peekError = std::get<std::optional<Error>>(peekTuple);

        if (peekError.hasValue()) {
            return false;
        }

        if (!peekOrNull.hasValue() || peekOrNull.value().size() != length) {
            return false;
        }

        ///

        const auto sub = peekOrNull.value().substr(distance, equals.size());

        ///

        return sub == equals;
    }

    ///

    const std::optional<std::string> match(
        const std::vector<std::string> reservedNames) const
    {
        for (const auto& reservedName : reservedNames) {
            if (match(reservedName)) {
                return reservedName;
            }
        }

        return std::nullopt;
    }

    ///

    const std::string& source() const { return m_source; }

    const int& position() const { return m_position; }

private:
    const std::string& m_source;

    int m_position = 0;
};

///

template <typename T>
class Parser {
public:
    Parser(
        const std::string& source,
        const std::vector<std::unique_ptr<T>>& tokens)
        : m_source(source)
        , m_tokens(tokens)
    {
    }

    Parser(const Parser&) = delete;

    Parser& operator=(const Parser&) = delete;

    ///

    const std::vector<std::unique_ptr<T>>& tokens() const
    {
        return m_tokens;
    }

    const int& position() const
    {
        return m_position;
    }

    const bool isEof() const
    {
        return m_position >= m_tokens.size();
    }

    void increment(
        int amount = 1)
    {
        m_position += amount;
    }

    const SourceLocation location() const
    {
        if (isEof()) {
            return SourceLocation(m_position);
        }

        return m_tokens[m_position]->location();
    }

    const SourceLocation location(
        int start) const
    {
        if (tokens().empty()) {
            return { 0, 0 };
        }

        if (m_position >= tokens().size()) {
            return { start, tokens().end()->get()->location().end };
        }

        return { start, tokens().at(m_position).get()->location().end };
    }

    const int start() const
    {
        if (tokens()->empty()) {
            return 0;
        }

        if (m_position >= tokens()->size()) {
            return tokens()->end()->get()->location().start;
        }

        return tokens().at(m_position).get()->location().start;
    }

    ///

    const std::optional<std::reference_wrapper<const T>> peek() const
    {
        if (isEof()) {
            return std::nullopt;
        }

        return std::cref(*tokens().at(m_position));
    }

private:
    const std::string& m_source;

    const std::vector<std::unique_ptr<T>>& m_tokens;

    int m_position = 0;
};