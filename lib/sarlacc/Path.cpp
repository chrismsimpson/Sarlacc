
#include "Path.h"

// path lexing

const std::tuple<std::vector<std::unique_ptr<PathToken>>, std::optional<Error>> PathLexer::lexFromSource(
    const std::string& source)
{
    std::optional<Error> error = std::nullopt;

    ///

    std::vector<std::unique_ptr<PathToken>> tokens;

    ///

    Lexer<PathToken> lexer(source);

    ///

    while (!lexer.isEof()) {

        auto tokenTuple = PathLexer::lexToken(lexer);

        auto& token = std::get<std::unique_ptr<PathToken>>(tokenTuple);

        const auto& tokenError = std::get<std::optional<Error>>(tokenTuple);

        if (tokenError.has_value()) {

            error.emplace(tokenError.value());

            break;
        }

        tokens.push_back(std::move(token));
    }

    ///

    const auto& last = tokens.back();

    if (last->type() != PathTokenType::Eof) {
        tokens.push_back(std::make_unique<PathEofToken>(
            SourceLocation(lexer.position())));
    }

    ///

    return { std::move(tokens), error };
}

const bool PathLexer::isDigit(
    const char& c)
{
    return c == '0'
        || c == '1'
        || c == '2'
        || c == '3'
        || c == '4'
        || c == '5'
        || c == '6'
        || c == '7'
        || c == '8'
        || c == '9';
}

const bool PathLexer::isNumberHead(
    const char& c)
{
    return c == '-'
        || c == '0'
        || c == '1'
        || c == '2'
        || c == '3'
        || c == '4'
        || c == '5'
        || c == '6'
        || c == '7'
        || c == '8'
        || c == '9';
}

const bool PathLexer::isNumberTail(
    const char& c)
{
    return PathLexer::isNumberHead(c) || c == 'e' || c == 'E' || c == '-';
}

const std::tuple<std::unique_ptr<PathToken>, std::optional<Error>> PathLexer::lexToken(
    Lexer<PathToken>& lexer)
{
    std::optional<Error> error = std::nullopt;

    ///

    while (!lexer.isEof()) {

        const auto& peekTuple = lexer.peek();

        const auto& peekOrNull = std::get<std::optional<char>>(peekTuple);

        const auto& peekError = std::get<std::optional<Error>>(peekTuple);

        if (peekError.has_value()) {

            error.emplace(peekError.value());

            break;
        }

        ///

        if (peekOrNull.has_value()) {

            const auto& peek = peekOrNull.value();

            if (peek == 'A'
                || peek == 'a'
                || peek == 'C'
                || peek == 'c'
                || peek == 'H'
                || peek == 'h'
                || peek == 'L'
                || peek == 'l'
                || peek == 'M'
                || peek == 'm'
                || peek == 'Q'
                || peek == 'q'
                || peek == 'S'
                || peek == 's'
                || peek == 'T'
                || peek == 't'
                || peek == 'V'
                || peek == 'v'
                || peek == 'Z'
                || peek == 'z') {

                // commands

                lexer.increment();

                return {
                    std::make_unique<PathCommandToken>(
                        SourceLocation(lexer.position()),
                        peek),
                    error
                };
            } else if (PathLexer::isNumberHead(peek)) {

                // numbers

                const auto isNumber = (peek == '-' && lexer.match(PathLexer::isDigit, 1))
                    || PathLexer::isDigit(peek);

                if (!isNumber) {
                    goto lexUnknownToken;
                }

                const auto start = lexer.position();

                while (!lexer.isEof()) {

                    lexer.increment();

                    if (lexer.isEof()) {
                        break;
                    }

                    if (lexer.match('.')
                        && !lexer.match(PathLexer::isNumberTail, 1)) {
                        break;
                    }

                    if (!lexer.match('.')
                        && !lexer.match(PathLexer::isNumberTail)) {
                        break;
                    }
                }

                const auto len = lexer.position() - start + (lexer.isEof() ? 1 : 0);

                const auto& source = lexer.source().substr(start, len);

                return {
                    std::make_unique<PathNumberToken>(
                        SourceLocation(start, lexer.position()),
                        source),
                    error
                };
            } else if (peek == ' '
                || peek == '\t'
                || peek == '\n'
                || peek == '\r') {

                // whitespace

                lexer.increment();
            } else if (peek == ',') {

                // punctuation

                lexer.increment();

                return {
                    std::make_unique<PathPuncToken>(
                        SourceLocation(lexer.position()),
                        PathPuncType::Comma,
                        ","),
                    error
                };
            } else {
                goto lexUnknownToken;
            }

        } else {

        lexUnknownToken:

            lexer.increment();

            return {
                std::make_unique<PathUnknownToken>(
                    SourceLocation(lexer.position()),
                    peekOrNull),
                error
            };
        }
    }

    ///

    return {
        std::make_unique<PathEofToken>(
            SourceLocation(lexer.position())),
        error
    };
}

// path parsing

const std::tuple<std::optional<std::vector<std::vector<PathCommand>>>, std::optional<Error>> PathParser::parsePathFromSource(
    const std::string& source)
{
    const auto lexedTuple = PathLexer::lexFromSource(source);

    const auto& lexedTokens = std::get<std::vector<std::unique_ptr<PathToken>>>(lexedTuple);

    const auto& lexError = std::get<std::optional<Error>>(lexedTuple);

    if (lexError.has_value()) {

        return { std::nullopt, lexError };
    }

    ///

    Parser<PathToken> parser(source, lexedTokens);

    ///

    return PathParser::parseSubPaths(parser);
}

const std::tuple<std::optional<PathPoint>, std::optional<Error>> PathParser::parsePoint(
    Parser<PathToken>& parser)
{
    if (parser.isEof()) {

        return { std::nullopt, Error(ErrorType::Parser, "unexpected eof") };
    }

    ///

    const auto peekXOrNull = parser.peek();

    if (!peekXOrNull.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected token when parsing point") };
    }

    ///

    const auto& peekX = peekXOrNull.value().get();

    if (peekX.type() != PathTokenType::Number) {

        return { std::nullopt, Error(ErrorType::Parser, "expected number when parsing point") };
    }

    const auto& x = static_cast<const PathNumberToken&>(peekX);

    parser.increment();

    ///

    const auto peekNextOrNull = parser.peek();

    if (!peekNextOrNull.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected token when parsing point") };
    }

    ///

    const auto& peekNext = peekNextOrNull.value().get();

    if (peekNext.type() == PathTokenType::Number) {

        const auto& y = static_cast<const PathNumberToken&>(peekNext);

        parser.increment();

        return {
            PathPoint {
                PathNumber { std::stof(x.value()), x.value() },
                PathNumber { std::stof(y.value()), y.value() } },
            std::nullopt
        };
    }

    ///

    if (peekNext.type() != PathTokenType::Punc) {

        return { std::nullopt, Error(ErrorType::Parser, "expected number or comma delimiter when parsing point") };
    }

    parser.increment();

    ///

    const auto& peekYOrNull = parser.peek();

    if (!peekYOrNull.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected token when parsing point") };
    }

    ///

    const auto& peekY = peekYOrNull.value().get();

    if (peekY.type() != PathTokenType::Number) {

        return { std::nullopt, Error(ErrorType::Parser, "expected number when parsing point") };
    }

    ///

    const auto& y = static_cast<const PathNumberToken&>(peekY);

    parser.increment();

    return {
        PathPoint {
            PathNumber { std::stof(x.value()), x.value() },
            PathNumber { std::stof(y.value()), y.value() } },
        std::nullopt
    };
}

const std::tuple<std::optional<std::vector<PathPoint>>, std::optional<Error>> PathParser::parsePoints(
    Parser<PathToken>& parser)
{
    if (parser.isEof()) {

        return { std::nullopt, Error(ErrorType::Parser, "unexpected eof") };
    }

    ///

    std::vector<PathPoint> points;

    ///

    while (!parser.isEof()) {

        const auto peekOrNull = parser.peek();

        if (!peekOrNull.has_value()) {

            break;
        }

        ///

        const auto& peek = peekOrNull.value().get();

        if (peek.type() == PathTokenType::Command
            || peek.type() == PathTokenType::Punc
            || peek.type() == PathTokenType::Eof) {

            break;
        }

        ///

        const auto pointTuple = PathParser::parsePoint(parser);

        const auto& point = std::get<std::optional<PathPoint>>(pointTuple);

        const auto& pointError = std::get<std::optional<Error>>(pointTuple);

        if (pointError.has_value()) {

            return { std::nullopt, pointError };
        }

        if (!point.has_value()) {

            break;
        }

        points.push_back(point.value());
    }

    ///

    return { std::move(points), std::nullopt };
}

const std::tuple<std::optional<PathNumber>, std::optional<Error>> PathParser::parseNumber(
    Parser<PathToken>& parser)
{
    if (parser.isEof()) {

        return { std::nullopt, Error(ErrorType::Parser, "unexpected eof") };
    }

    ///

    const auto peekOrNull = parser.peek();

    if (!peekOrNull.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected token when parsing number") };
    }

    ///

    const auto& peek = peekOrNull.value().get();

    if (peek.type() != PathTokenType::Number) {

        return { std::nullopt, Error(ErrorType::Parser, "expected number when parsing number") };
    }

    ///

    const auto& number = static_cast<const PathNumberToken&>(peek);

    parser.increment();

    ///

    return {
        PathNumber { std::stof(number.value()), number.value() },
        std::nullopt
    };
}

const std::tuple<std::optional<std::vector<PathNumber>>, std::optional<Error>> PathParser::parseNumbers(
    Parser<PathToken>& parser)
{
    if (parser.isEof()) {

        return { std::nullopt, Error(ErrorType::Parser, "unexpected eof") };
    }

    ///

    std::vector<PathNumber> numbers;

    ///

    while (!parser.isEof()) {

        const auto peekOrNull = parser.peek();

        if (!peekOrNull.has_value()) {

            break;
        }

        ///

        const auto& peek = peekOrNull.value().get();

        if (peek.type() == PathTokenType::Command
            || peek.type() == PathTokenType::Punc
            || peek.type() == PathTokenType::Eof) {

            break;
        }

        ///

        if (peek.type() != PathTokenType::Number) {

            return { std::nullopt, Error(ErrorType::Parser, "expected number when parsing numbers") };
        }

        ///

        const auto& number = static_cast<const PathNumberToken&>(peek);

        parser.increment();

        numbers.push_back(PathNumber { std::stof(number.value()), number.value() });
    }

    ///

    return { std::move(numbers), std::nullopt };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandMoveTo(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'M'
        && command.value() != 'm') {

        return { std::nullopt, Error(ErrorType::Parser, "expected move to command when parsing move to command") };
    }

    const auto position = command.value() == 'M'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    const auto pointsTuple = PathParser::parsePoints(parser);

    const auto& points = std::get<std::optional<std::vector<PathPoint>>>(pointsTuple);

    const auto& pointsError = std::get<std::optional<Error>>(pointsTuple);

    if (pointsError.has_value()) {

        return { std::nullopt, pointsError };
    }

    if (!points.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points when parsing move to command") };
    }

    ///

    return {
        PathCommand {
            PathCommandType::MoveTo,
            position,
            points.value(),
            std::nullopt,
            std::nullopt },
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandLineTo(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'L'
        && command.value() != 'l') {

        return { std::nullopt, Error(ErrorType::Parser, "expected line to command when parsing line to command") };
    }

    const auto position = command.value() == 'L'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    const auto pointsTuple = PathParser::parsePoints(parser);

    const auto& points = std::get<std::optional<std::vector<PathPoint>>>(pointsTuple);

    const auto& pointsError = std::get<std::optional<Error>>(pointsTuple);

    if (pointsError.has_value()) {

        return { std::nullopt, pointsError };
    }

    if (!points.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points when parsing line to command") };
    }

    ///

    return {
        PathCommand {
            PathCommandType::LineTo,
            position,
            points.value(),
            std::nullopt,
            std::nullopt },
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandHLineTo(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'H'
        && command.value() != 'h') {

        return { std::nullopt, Error(ErrorType::Parser, "expected horizontal line to command when parsing horizontal line to command") };
    }

    const auto position = command.value() == 'H'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    const auto numbersTuple = PathParser::parseNumbers(parser);

    const auto& numbers = std::get<std::optional<std::vector<PathNumber>>>(numbersTuple);

    const auto& numbersError = std::get<std::optional<Error>>(numbersTuple);

    if (numbersError.has_value()) {

        return { std::nullopt, numbersError };
    }

    if (!numbers.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected numbers when parsing horizontal line to command") };
    }

    ///

    return {
        PathCommand {
            PathCommandType::HorizontalLineTo,
            position,
            std::nullopt,
            numbers.value(),
            std::nullopt },
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandVLineTo(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'V'
        && command.value() != 'v') {

        return { std::nullopt, Error(ErrorType::Parser, "expected vertical line to command when parsing vertical line to command") };
    }

    const auto position = command.value() == 'V'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    const auto numbersTuple = PathParser::parseNumbers(parser);

    const auto& numbers = std::get<std::optional<std::vector<PathNumber>>>(numbersTuple);

    const auto& numbersError = std::get<std::optional<Error>>(numbersTuple);

    if (numbersError.has_value()) {

        return { std::nullopt, numbersError };
    }

    if (!numbers.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected numbers when parsing vertical line to command") };
    }

    ///

    return {
        PathCommand {
            PathCommandType::VerticalLineTo,
            position,
            std::nullopt,
            numbers.value(),
            std::nullopt },
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandCurveTo(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'C'
        && command.value() != 'c') {

        return { std::nullopt, Error(ErrorType::Parser, "expected curve to command when parsing curve to command") };
    }

    const auto position = command.value() == 'C'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    const auto pointsTuple = PathParser::parsePoints(parser);

    const auto& points = std::get<std::optional<std::vector<PathPoint>>>(pointsTuple);

    const auto& pointsError = std::get<std::optional<Error>>(pointsTuple);

    if (pointsError.has_value()) {

        return { std::nullopt, pointsError };
    }

    if (!points.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points when parsing curve to command") };
    }

    ///

    if (points.value().size() % 3 != 0) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points in multiples of 3 when parsing curve to command") };
    }

    ///

    return {
        PathCommand {
            PathCommandType::CurveTo,
            position,
            points.value(),
            std::nullopt,
            std::nullopt },
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandSmoothCurveTo(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'S'
        && command.value() != 's') {

        return { std::nullopt, Error(ErrorType::Parser, "expected smooth curve to command when parsing smooth curve to command") };
    }

    const auto position = command.value() == 'S'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    const auto pointsTuple = PathParser::parsePoints(parser);

    const auto& points = std::get<std::optional<std::vector<PathPoint>>>(pointsTuple);

    const auto& pointsError = std::get<std::optional<Error>>(pointsTuple);

    if (pointsError.has_value()) {

        return { std::nullopt, pointsError };
    }

    if (!points.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points when parsing smooth curve to command") };
    }

    ///

    if (points.value().size() % 2 != 0) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points in multiples of 2 when parsing smooth curve to command") };
    }

    ///

    return {
        PathCommand {
            PathCommandType::SmoothCurveTo,
            position,
            points.value(),
            std::nullopt,
            std::nullopt },
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandQuadraticBezierCurveTo(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'Q'
        && command.value() != 'q') {

        return { std::nullopt, Error(ErrorType::Parser, "expected quadratic bezier curve to command when parsing quadratic bezier curve to command") };
    }

    const auto position = command.value() == 'Q'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    const auto pointsTuple = PathParser::parsePoints(parser);

    const auto& points = std::get<std::optional<std::vector<PathPoint>>>(pointsTuple);

    const auto& pointsError = std::get<std::optional<Error>>(pointsTuple);

    if (pointsError.has_value()) {

        return { std::nullopt, pointsError };
    }

    if (!points.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points when parsing quadratic bezier curve to command") };
    }

    ///

    if (points.value().size() % 2 != 0) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points in multiples of 2 when parsing quadratic bezier curve to command") };
    }

    ///

    return {
        PathCommand {
            PathCommandType::QuadraticBezierCurveTo,
            position,
            points.value(),
            std::nullopt,
            std::nullopt },
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandSmoothQuadraticBezierCurveTo(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'T'
        && command.value() != 't') {

        return { std::nullopt, Error(ErrorType::Parser, "expected smooth quadratic bezier curve to command when parsing smooth quadratic bezier curve to command") };
    }

    const auto position = command.value() == 'T'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    const auto pointsTuple = PathParser::parsePoints(parser);

    const auto& points = std::get<std::optional<std::vector<PathPoint>>>(pointsTuple);

    const auto& pointsError = std::get<std::optional<Error>>(pointsTuple);

    if (pointsError.has_value()) {

        return { std::nullopt, pointsError };
    }

    if (!points.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points when parsing smooth quadratic bezier curve to command") };
    }

    ///

    if (points.value().size() % 2 != 0) {

        return { std::nullopt, Error(ErrorType::Parser, "expected points in multiples of 2 when parsing smooth quadratic bezier curve to command") };
    }

    ///

    return {
        PathCommand {
            PathCommandType::SmoothQuadraticBezierCurveTo,
            position,
            points.value(),
            std::nullopt,
            std::nullopt },
        std::nullopt
    };
}

const std::tuple<std::optional<std::tuple<PathPoint, PathNumber, PathPoint, PathPoint>>, std::optional<Error>> PathParser::parseEllipticalArc(
    Parser<PathToken>& parser)
{
    if (parser.isEof()) {

        return { std::nullopt, Error(ErrorType::Parser, "unexpected eof") };
    }

    ///

    const auto radTuple = PathParser::parsePoint(parser);

    const auto& rad = std::get<std::optional<PathPoint>>(radTuple);

    const auto& radError = std::get<std::optional<Error>>(radTuple);

    if (radError.has_value()) {

        return { std::nullopt, radError };
    }

    if (!rad.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected radius/point when parsing elliptical arc") };
    }

    ///

    const auto xRotationTuple = PathParser::parseNumber(parser);

    const auto& xRotation = std::get<std::optional<PathNumber>>(xRotationTuple);

    const auto& xRotationError = std::get<std::optional<Error>>(xRotationTuple);

    if (xRotationError.has_value()) {

        return { std::nullopt, xRotationError };
    }

    if (!xRotation.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected x-axis-rotation when parsing elliptical arc") };
    }

    ///

    const auto flagsTuple = PathParser::parsePoint(parser);

    const auto& flags = std::get<std::optional<PathPoint>>(flagsTuple);

    const auto& flagsError = std::get<std::optional<Error>>(flagsTuple);

    if (flagsError.has_value()) {

        return { std::nullopt, flagsError };
    }

    if (!flags.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected flags when parsing elliptical arc") };
    }

    ///

    const auto endTuple = PathParser::parsePoint(parser);

    const auto& end = std::get<std::optional<PathPoint>>(endTuple);

    const auto& endError = std::get<std::optional<Error>>(endTuple);

    if (endError.has_value()) {

        return { std::nullopt, endError };
    }

    if (!end.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected end point when parsing elliptical arc") };
    }

    ///

    return {
        std::make_tuple(rad.value(), xRotation.value(), flags.value(), end.value()),
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandEllipticalArc(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'A'
        && command.value() != 'a') {

        return { std::nullopt, Error(ErrorType::Parser, "expected elliptical arc command when parsing elliptical arc command") };
    }

    const auto position = command.value() == 'A'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    std::vector<std::tuple<PathPoint, PathNumber, PathPoint, PathPoint>> arcs;

    ///

    while (!parser.isEof()) {

        const auto peekOrNull = parser.peek();

        if (!peekOrNull.has_value()) {

            break;
        }

        ///

        const auto& peek = peekOrNull.value().get();

        if (peek.type() == PathTokenType::Command
            || peek.type() == PathTokenType::Punc

            || peek.type() == PathTokenType::Eof) {

            break;
        }

        ///

        const auto arcTuple = PathParser::parseEllipticalArc(parser);

        const auto& arc = std::get<std::optional<std::tuple<PathPoint, PathNumber, PathPoint, PathPoint>>>(arcTuple);

        const auto& arcError = std::get<std::optional<Error>>(arcTuple);

        if (arcError.has_value()) {

            return { std::nullopt, arcError };
        }

        if (!arc.has_value()) {

            break;
        }

        arcs.push_back(arc.value());
    }

    ///

    if (arcs.empty()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected arcs when parsing elliptical arc command") };
    }

    ///

    return {
        PathCommand {
            PathCommandType::EllipticalArc,
            position,
            std::nullopt,
            std::nullopt,
            std::move(arcs) },
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommandClosePath(
    const PathCommandToken& command,
    Parser<PathToken>& parser)
{
    if (command.value() != 'Z'
        && command.value() != 'z') {

        return { std::nullopt, Error(ErrorType::Parser, "expected close path command when parsing close path command") };
    }

    const auto position = command.value() == 'A'
        ? PathCommandPosition::Absolute
        : PathCommandPosition::Relative;

    parser.increment();

    ///

    return {
        PathCommand {
            PathCommandType::ClosePath,
            position,
            std::nullopt,
            std::nullopt,
            std::nullopt },
        std::nullopt
    };
}

const std::tuple<std::optional<PathCommand>, std::optional<Error>> PathParser::parseCommand(
    Parser<PathToken>& parser)
{
    if (parser.isEof()) {

        return { std::nullopt, Error(ErrorType::Parser, "unexpected eof") };
    }

    ///

    const auto peekOrNull = parser.peek();

    if (!peekOrNull.has_value()) {

        return { std::nullopt, Error(ErrorType::Parser, "expected token when parsing command") };
    }

    ///

    const auto& peek = peekOrNull.value().get();

    if (peek.type() == PathTokenType::Eof) {

        return { std::nullopt, std::nullopt };
    }

    if (peek.type() != PathTokenType::Command) {

        return { std::nullopt, Error(ErrorType::Parser, "expected command when parsing command") };
    }

    ///

    const auto& command = static_cast<const PathCommandToken&>(peek);

    switch (command.value()) {
    case 'M':
    case 'm': {
        return PathParser::parseCommandMoveTo(command, parser);
    }

    case 'L':
    case 'l': {
        return PathParser::parseCommandLineTo(command, parser);
    }

    case 'H':
    case 'h': {
        return PathParser::parseCommandHLineTo(command, parser);
    }

    case 'V':
    case 'v': {
        return PathParser::parseCommandVLineTo(command, parser);
    }

    case 'C':
    case 'c': {
        return PathParser::parseCommandCurveTo(command, parser);
    }

    case 'S':
    case 's': {
        return PathParser::parseCommandSmoothCurveTo(command, parser);
    }

    case 'Q':
    case 'q': {
        return PathParser::parseCommandQuadraticBezierCurveTo(command, parser);
    }

    case 'T':
    case 't': {
        return PathParser::parseCommandSmoothQuadraticBezierCurveTo(command, parser);
    }

    case 'A':
    case 'a': {
        return PathParser::parseCommandEllipticalArc(command, parser);
    }

    case 'Z':
    case 'z': {
        return PathParser::parseCommandClosePath(command, parser);
    }

    default: {
        return { std::nullopt, Error(ErrorType::Parser, "unknown command token when parsing command") };
    }
    }
}

const std::tuple<std::optional<std::vector<PathCommand>>, std::optional<Error>> PathParser::parseSubPath(
    Parser<PathToken>& parser)
{
    if (parser.isEof()) {

        return { std::nullopt, Error(ErrorType::Parser, "unexpected eof") };
    }

    ///

    std::vector<PathCommand> commands;

    ///

    while (!parser.isEof()) {

        const auto commandTuple = PathParser::parseCommand(parser);

        const auto& commandOrNull = std::get<std::optional<PathCommand>>(commandTuple);

        const auto& commandError = std::get<std::optional<Error>>(commandTuple);

        if (commandError.has_value()) {

            return { std::nullopt, commandError };
        }

        if (!commandOrNull.has_value()) {

            break;
        }

        const auto& command = commandOrNull.value();

        commands.push_back(command);

        ///

        if (command.type == PathCommandType::ClosePath) {

            break;
        }
    }

    ///

    if (commands.empty()) {

        return { std::nullopt, std::nullopt };
    }

    ///

    return { std::move(commands), std::nullopt };
}

const std::tuple<std::optional<std::vector<std::vector<PathCommand>>>, std::optional<Error>> PathParser::parseSubPaths(
    Parser<PathToken>& parser)
{
    if (parser.isEof()) {

        return { std::nullopt, Error(ErrorType::Parser, "unexpected eof") };
    }

    ///

    std::vector<std::vector<PathCommand>> subPaths;

    ///

    while (!parser.isEof()) {

        const auto subPathTuple = PathParser::parseSubPath(parser);

        const auto& subPath = std::get<std::optional<std::vector<PathCommand>>>(subPathTuple);

        const auto& subPathError = std::get<std::optional<Error>>(subPathTuple);

        if (subPathError.has_value()) {

            return { std::nullopt, subPathError };
        }

        if (!subPath.has_value()) {

            break;
        }

        subPaths.push_back(subPath.value());
    }

    ///

    return { std::move(subPaths), std::nullopt };
}
