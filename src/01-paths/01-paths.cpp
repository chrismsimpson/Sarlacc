
#include <print>
#include <utility>

#include "Path.h"

int main()
{
    const auto& path = "M 100 100 L 300 100 L 200 300 z";

    ///

    const auto& start = std::chrono::steady_clock::now();

    ///

    const auto p = PathParser::parsePathFromSource(path);

    const auto& parsedCommands = std::get<std::optional<std::vector<std::vector<PathCommand>>>>(p);

    const auto& parsedError = std::get<std::optional<Error>>(p);

    if (parsedError.has_value()) {

        if (parsedError.value().message().has_value()) {

            std::println("error: {}", parsedError.value().message().value());
        }

        return 1;
    }

    for (const auto& commands : parsedCommands.value()) {

        for (const auto& command : commands) {

            switch (command.type) {

            case PathCommandType::MoveTo: {

                std::println("move to");

                break;
            }

            case PathCommandType::LineTo: {

                std::println("line to");

                break;
            }

            case PathCommandType::HorizontalLineTo: {

                std::println("h line to");

                break;
            }

            case PathCommandType::VerticalLineTo: {

                std::println("v line to");

                break;
            }

            case PathCommandType::CurveTo: {

                std::println("curve to");

                break;
            }

            case PathCommandType::SmoothCurveTo: {

                std::println("smooth curve to");

                break;
            }

            case PathCommandType::QuadraticBezierCurveTo: {

                std::println("quadratic bezier curve to");

                break;
            }

            case PathCommandType::SmoothQuadraticBezierCurveTo: {

                std::println("smooth quadratic bezier curve to");

                break;
            }

            case PathCommandType::EllipticalArc: {

                std::println("elliptical arc");

                break;
            }

            case PathCommandType::ClosePath: {

                std::println("close path");

                break;
            }
            }
        }
    }

    ///

    const auto& stop = std::chrono::steady_clock::now();

    const auto& duration = stop - start;

    const auto& durationNano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    const auto& durationMicro = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();

    const auto& durationMilli = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    const auto& durationSec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    ///

    std::println("Duration: {} ns", durationNano);

    std::println("Duration: {} us", durationMicro);

    std::println("Duration: {} ms", durationMilli);

    std::println("Duration: {} s", durationSec);

    return 0;
}