# Contributing to Wadjet-Link

Thank you for your interest in contributing to Wadjet-Link! 🎉

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/wadjet-link.git`
3. Create a feature branch: `git checkout -b feature/your-feature-name`

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
```

## Code Style

We use **clang-format** and **clang-tidy** to maintain consistent code quality.

### Before committing:

```bash
# Format all files
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

# Run static analysis
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build src/*.cpp
```

### Style Guidelines

- **Namespaces**: `snake_case` (e.g., `wadjet::protocols::someip`)
- **Classes/Structs**: `PascalCase` (e.g., `CaptureSession`)
- **Functions/Methods**: `snake_case` (e.g., `start_capture()`)
- **Variables**: `snake_case` (e.g., `packet_count`)
- **Private members**: `snake_case_` with trailing underscore
- **Constants**: `UPPER_CASE` (e.g., `MAX_PACKET_SIZE`)
- **Macros**: `UPPER_CASE` with `WADJET_` prefix

## Commit Messages

Use clear, descriptive commit messages:

```
feat(protocols): add SOME/IP header parser
fix(capture): handle zero-length packets
docs: update README with build instructions
test: add unit tests for DoIP decoder
```

Prefixes:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `test`: Tests
- `refactor`: Code refactoring
- `perf`: Performance improvement
- `ci`: CI/CD changes

## Pull Requests

1. Ensure all tests pass
2. Add tests for new functionality
3. Update documentation if needed
4. Keep PRs focused on a single feature/fix
5. Reference any related issues

## Testing

- All new code must have unit tests
- Use GoogleTest framework
- Aim for meaningful coverage, not just metrics
- Add regression tests for bug fixes

```cpp
TEST(MyFeature, DescriptiveTestName) {
    // Arrange
    // Act
    // Assert
}
```

## License

By contributing, you agree that your contributions will be licensed under the Apache 2.0 License.

## Questions?

Open an issue or start a discussion. We're happy to help!
