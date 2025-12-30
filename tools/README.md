# Wadjet-Link CLI Tools

This directory contains command-line tools built on the Wadjet-Link library.

## Available Tools

### wadjet-run

A scenario-based test runner for automated protocol validation.

```bash
# Basic usage
wadjet-run <scenario.yaml>

# Run all scenarios in a directory
wadjet-run --dir scenarios/

# With options
wadjet-run [options] <scenario-file>...
```

#### Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message |
| `-V, --version` | Show version information |
| `-v, --verbose` | Enable verbose output |
| `-q, --quiet` | Suppress all output except errors |
| `--dry-run` | Parse and validate scenarios without running |
| `--stop-on-failure` | Stop at first failed expectation |
| `--timeout <ms>` | Global timeout in milliseconds (default: 60000) |
| `-o, --output <file>` | Write report to file |
| `-f, --format <fmt>` | Report format: junit, json, text, tap |
| `--pcap-dir <dir>` | Directory for failure pcap files |
| `--no-pcap` | Don't save pcap files on failure |
| `-t, --tag <tag>` | Only run scenarios with this tag (repeatable) |
| `-d, --dir <directory>` | Run all scenarios in directory |
| `-l, --list` | List scenarios without running them |

#### Examples

```bash
# Run a single scenario
wadjet-run test.yaml

# Run all scenarios in a directory
wadjet-run --dir scenarios/

# Run scenarios with specific tag and generate JUnit report
wadjet-run --tag smoke -f junit -o results.xml scenarios/

# Dry run to validate scenario files
wadjet-run --dry-run --dir scenarios/

# List available scenarios
wadjet-run --list --dir scenarios/
```

#### Report Formats

| Format | Description | Use Case |
|--------|-------------|----------|
| `text` | Human-readable plain text | Console output |
| `junit` | JUnit XML format | Jenkins, CI/CD integration |
| `json` | JSON format | Programmatic analysis |
| `tap` | Test Anything Protocol | TAP consumers |

## Building

The tools are built automatically when building Wadjet-Link:

```bash
cmake -B build
cmake --build build
./build/tools/wadjet-run --help
```

## Adding New Tools

1. Create a new `.cpp` file in this directory
2. Add it to `CMakeLists.txt`
3. Link against the `wadjet` library
4. Document usage in this README
