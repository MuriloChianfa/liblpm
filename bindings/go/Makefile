.PHONY: all build test bench clean install-deps fmt vet lint run-example

# Default target
all: build test

# Build all Go packages
build: install-deps
	@echo "Building Go packages..."
	go build ./...

# Run all tests
test: build
	@echo "Running tests..."
	go test -v ./liblpm/

# Run benchmarks
bench: build
	@echo "Running benchmarks..."
	go test -bench=. -benchmem ./benchmarks/

# Run benchmarks with more iterations (for iprbench comparison)
bench-compare: build
	@echo "Running benchmarks for comparison (6 iterations)..."
	go test -bench=. -benchmem -count=6 ./benchmarks/ | tee benchmark_results.txt

# Clean build artifacts
clean:
	@echo "Cleaning Go build artifacts..."
	go clean ./...
	rm -f benchmark_results.txt

# Ensure liblpm C library is built and available
install-deps:
	@echo "Checking liblpm library..."
	@if [ ! -f ../build/liblpm.so ]; then \
		echo "Error: liblpm.so not found. Please build liblpm first:"; \
		echo "  cd .. && mkdir -p build && cd build && cmake .. && make"; \
		exit 1; \
	fi
	@echo "liblpm library found."

# Format Go code
fmt:
	@echo "Formatting Go code..."
	go fmt ./...

# Run go vet
vet:
	@echo "Running go vet..."
	go vet ./...

# Run staticcheck (if installed)
lint: vet
	@echo "Running staticcheck..."
	@if command -v staticcheck >/dev/null 2>&1; then \
		staticcheck ./...; \
	else \
		echo "staticcheck not found. Install with: go install honnef.co/go/tools/cmd/staticcheck@latest"; \
	fi

# Run example
run-example: build
	@echo "Running basic example..."
	cd examples && go run basic_example.go

# Run tests with race detector
test-race: build
	@echo "Running tests with race detector..."
	go test -race -v ./liblpm/

# Run tests with coverage
test-coverage: build
	@echo "Running tests with coverage..."
	go test -cover -coverprofile=coverage.out ./liblpm/
	go tool cover -html=coverage.out -o coverage.html
	@echo "Coverage report generated: coverage.html"

# Update go.mod dependencies
tidy:
	@echo "Tidying go.mod..."
	go mod tidy

# Install the Go module locally for development
install:
	@echo "Installing Go module..."
	go install ./...

# Help target
help:
	@echo "Available targets:"
	@echo "  all           - Build and test (default)"
	@echo "  build         - Build all Go packages"
	@echo "  test          - Run all tests"
	@echo "  bench         - Run benchmarks"
	@echo "  bench-compare - Run benchmarks for comparison (6 iterations)"
	@echo "  clean         - Clean build artifacts"
	@echo "  fmt           - Format Go code"
	@echo "  vet           - Run go vet"
	@echo "  lint          - Run staticcheck linter"
	@echo "  run-example   - Run basic example"
	@echo "  test-race     - Run tests with race detector"
	@echo "  test-coverage - Run tests with coverage report"
	@echo "  tidy          - Update go.mod dependencies"
	@echo "  install       - Install Go module locally"
	@echo "  help          - Show this help message"


