
#include "Path.h"

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
