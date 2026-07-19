#ifndef BABYBEHAVE_EXAMPLES_GHERKIN_LOAD_FEATURE_FILE_HPP
#define BABYBEHAVE_EXAMPLES_GHERKIN_LOAD_FEATURE_FILE_HPP

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

// Reads a real, standalone .feature file from disk (plain std::ifstream,
// nothing fancy) and returns its contents as a std::string, for the
// registry-reuse examples under examples/gherkin/ (GherkinBakery*.cpp,
// GherkinLibrary*.cpp) that load their scenario text from
// examples/gherkin/features/ instead of embedding it as a C++ raw string
// literal like every other example in this project does.
//
// Important: this is purely example-level file I/O. BabyBehave::BDD::
// Gherkin::RunFeature() itself takes std::string_view and never touches
// the filesystem on its own (see docs/design/gherkin-support.md's "runtime
// interpreter, not a code generator" design) - only these example mains
// read a file first and then hand RunFeature() the resulting string, same
// as they would a raw string literal.
//
// `filename` is resolved relative to BABYBEHAVE_GHERKIN_FEATURES_DIR, a
// compile-time definition set per-target in examples/CMakeLists.txt
// (CMAKE_CURRENT_SOURCE_DIR-relative, baked into the binary at build
// time), so this works regardless of the working directory the example
// is launched from - a bare `./build/examples/example_GherkinBakery...`
// invocation, `ctest --test-dir build`, or anything else.
inline std::string LoadFeatureFile(const std::string& filename) {
    const std::string path = std::string(BABYBEHAVE_GHERKIN_FEATURES_DIR) + "/" + filename;
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("BabyBehave example: could not open feature file: " + path);
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

#endif // BABYBEHAVE_EXAMPLES_GHERKIN_LOAD_FEATURE_FILE_HPP
