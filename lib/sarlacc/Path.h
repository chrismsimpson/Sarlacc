
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "Error.h"
#include "Parsing.h"
#include "SourceLocation.h"

// path tokens

enum class PathTokenType {
    Command,
    Number,
    Punc,
    Unknown,
    Eof,
};

class PathToken : public Locatable {
public:
    PathToken(
        const PathToken& other)
        : Locatable(other.location())
        , m_type(other.type())
    {
    }

    PathToken(
        const PathTokenType& type,
        const SourceLocation& location)
        : Locatable(location)
        , m_type(type)
    {
    }

    PathToken(
        PathTokenType&& type,
        SourceLocation&& location)
        : Locatable(std::move(location))
        , m_type(std::move(type))
    {
    }

    PathToken& operator=(const PathToken&) = delete;

    PathToken& operator=(PathToken&&) = delete;

    ///

    const PathTokenType& type() const { return m_type; }

private:
    const PathTokenType m_type;
};

///

class PathUnknownToken final : public PathToken {
public:
    PathUnknownToken(
        const SourceLocation& location,
        const std::optional<char>& value)
        : PathToken(PathTokenType::Unknown, location)
        , m_value(value)
    {
    }

    PathUnknownToken(
        SourceLocation&& location,
        std::optional<char>&& value)
        : PathToken(PathTokenType::Unknown, std::move(location))
        , m_value(std::move(value))
    {
    }

    PathUnknownToken& operator=(
        const PathUnknownToken& other)
        = delete;

    PathUnknownToken& operator=(
        const PathUnknownToken&& other)
        = delete;

    ///

    const std::optional<char>& value() const { return m_value; }

private:
    std::optional<char> m_value;
};

///

class PathEofToken final : public PathToken {
public:
    PathEofToken(
        const SourceLocation& location)
        : PathToken(PathTokenType::Eof, location)
    {
    }

    PathEofToken(
        SourceLocation&& location)
        : PathToken(PathTokenType::Eof, std::move(location))
    {
    }

    PathEofToken& operator=(
        const PathEofToken& other)
        = delete;

    PathEofToken& operator=(
        PathEofToken&& other)
        = delete;
};

///

class PathCommandToken final : public PathToken {
public:
    PathCommandToken(
        const SourceLocation& location,
        const char value)
        : PathToken(PathTokenType::Command, location)
        , m_value(value)
    {
    }

    PathCommandToken(
        SourceLocation&& location,
        char value)
        : PathToken(PathTokenType::Command, std::move(location))
        , m_value(value)
    {
    }

    PathCommandToken& operator=(
        const PathCommandToken& other)
        = delete;

    PathCommandToken& operator=(
        PathCommandToken&& other)
        = delete;

    ///

    const char& value() const { return m_value; }

private:
    const char m_value;
};

///

class PathNumberToken final : public PathToken {
public:
    PathNumberToken(
        const SourceLocation& location,
        const std::string& value)
        : PathToken(PathTokenType::Number, location)
        , m_value(value)
    {
    }

    PathNumberToken(
        SourceLocation&& location,
        std::string&& value)
        : PathToken(PathTokenType::Number, std::move(location))
        , m_value(std::move(value))
    {
    }

    PathNumberToken& operator=(
        const PathNumberToken& other)
        = delete;

    PathNumberToken& operator=(
        PathNumberToken&& other)
        = delete;

    ///

    const std::string& value() const { return m_value; }

private:
    const std::string m_value;
};

///

enum class PathPuncType {
    Comma,
};

class PathPuncToken final : public PathToken {
public:
    PathPuncToken(
        const SourceLocation& location,
        const PathPuncType& puncType,
        const std::string& value)
        : PathToken(PathTokenType::Punc, location)
        , m_puncType(puncType)
        , m_value(value)
    {
    }

    PathPuncToken(
        SourceLocation&& location,
        PathPuncType&& puncType,
        std::string&& value)
        : PathToken(PathTokenType::Punc, std::move(location))
        , m_puncType(std::move(puncType))
        , m_value(std::move(value))
    {
    }

    PathPuncToken& operator=(
        const PathPuncToken& other)
        = delete;

    PathPuncToken& operator=(
        PathPuncToken&& other)
        = delete;

    ///

    const PathPuncType& puncType() const { return m_puncType; }

    const std::string& value() const { return m_value; }

private:
    const PathPuncType m_puncType;

    const std::string m_value;
};

///

// path lexing

class PathLexer final {
public:
    static const std::tuple<std::vector<std::unique_ptr<PathToken>>, std::optional<Error>> lexFromSource(
        const std::string& source);

private:
    static const bool isDigit(
        const char& c);

    static const bool isNumberHead(
        const char& c);

    static const bool isNumberTail(
        const char& c);

    static const std::tuple<std::unique_ptr<PathToken>, std::optional<Error>> lexToken(
        Lexer<PathToken>& lexer);
};

///

// path types

struct PathNumber {
    float value;
    std::string source;
};

struct PathPoint {
    PathNumber x;
    PathNumber y;
};

enum class PathCommandPosition {
    Absolute,
    Relative,
};

enum class PathCommandType {
    MoveTo,
    LineTo,
    HorizontalLineTo,
    VerticalLineTo,
    ClosePath,
    CurveTo,
    SmoothCurveTo,
    QuadraticBezierCurveTo,
    SmoothQuadraticBezierCurveTo,
    EllipticalArc,
};

struct PathCommand {
    PathCommandType type;
    PathCommandPosition position;
    std::optional<std::vector<PathPoint>> points;
    std::optional<std::vector<PathNumber>> numbers;
    std::optional<std::vector<std::tuple<PathPoint, PathNumber, PathPoint, PathPoint>>> arcs;
};

///

// path parsing

class PathParser final {

public:
    static const std::tuple<std::optional<std::vector<std::vector<PathCommand>>>, std::optional<Error>> parsePathFromSource(
        const std::string& source);

private:
    static const std::tuple<std::optional<PathPoint>, std::optional<Error>> parsePoint(
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<std::vector<PathPoint>>, std::optional<Error>> parsePoints(
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathNumber>, std::optional<Error>> parseNumber(
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<std::vector<PathNumber>>, std::optional<Error>> parseNumbers(
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandMoveTo(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandLineTo(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandHLineTo(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandVLineTo(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandCurveTo(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandSmoothCurveTo(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandQuadraticBezierCurveTo(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandSmoothQuadraticBezierCurveTo(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<std::tuple<PathPoint, PathNumber, PathPoint, PathPoint>>, std::optional<Error>> parseEllipticalArc(
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandEllipticalArc(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommandClosePath(
        const PathCommandToken& command,
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<PathCommand>, std::optional<Error>> parseCommand(
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<std::vector<PathCommand>>, std::optional<Error>> parseSubPath(
        Parser<PathToken>& parser);

    static const std::tuple<std::optional<std::vector<std::vector<PathCommand>>>, std::optional<Error>> parseSubPaths(
        Parser<PathToken>& parser);
};