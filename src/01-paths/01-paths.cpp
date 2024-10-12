
#include <print>

#include "Path.h"

int main()
{
    const auto& path = "M 100 100 L 300 100 L 200 300 z";

    const auto lexedTuple = PathLexer::lexFromSource(path);

    const auto& lexedTokens = std::get<std::vector<std::unique_ptr<PathToken>>>(lexedTuple);

    const auto& lexError = std::get<std::optional<Error>>(lexedTuple);

    if (lexError.has_value()) {
        std::println("error");

        return 1;
    }

    for (const auto& token : lexedTokens) {

        switch (token->type()) {

        case PathTokenType::Command: {

            const auto& commandToken = static_cast<PathCommandToken&>(*token);

            std::println("command: {}", commandToken.value());

            break;
        }

        case PathTokenType::Number: {

            const auto& numberToken = static_cast<PathNumberToken&>(*token);

            std::println("number: {}", numberToken.value());

            break;
        }

        case PathTokenType::Punc: {

            const auto& puncToken = static_cast<PathPuncToken&>(*token);

            std::println("punc: {}", puncToken.value());

            break;
        }

        case PathTokenType::Unknown: {

            const auto& unknownToken = static_cast<PathUnknownToken&>(*token);

            unknownToken.value().has_value()
                ? std::println("unknown: {}", unknownToken.value().value())
                : std::println("unknown (no value)");

            break;
        }

        case PathTokenType::Eof: {

            std::println("eof");

            break;
        }
        }
    }

    return 0;
}