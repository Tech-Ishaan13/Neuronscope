#include "tui/app.h"
#include <iostream>
#include <string>

void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options]\n\n"
              << "Options:\n"
              << "  -h, --help      Show this help message\n"
              << "  --ascii         Use ASCII character fallback (disable NerdFonts)\n"
              << "  --mock          Run with simulated input (starts self-mock stream)\n\n"
              << "NeuronScope — Local LLM Instrumentation, Tracing & Replay Platform\n";
}

int main(int argc, char* argv[]) {
    bool use_ascii = false;
    bool start_mock = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--ascii") {
            use_ascii = true;
        } else if (arg == "--mock") {
            start_mock = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    App app;
    if (!app.init()) {
        std::cerr << "Failed to initialize NeuronScope Application.\n";
        return 1;
    }

    std::cout << "Starting NeuronScope...\n";
    app.run();
    app.stop();

    return 0;
}
