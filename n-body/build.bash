#!/usr/bin/env bash

BUILD_DIR="bin"

mkdir -p $BUILD_DIR

CFLAGS="-Wall -Wextra"

show_usage() {
    cat << EOF
N-Body Simulation Runner Build Script

Usage: $0 [COMMAND]

Commands:
    json-runner   Build json runner utility only
    test          Build and run json tests
    clean         Remove build artifacts
    help          Show this message
EOF
}

run_tests() {
    if gcc $CFLAGS json_tests.c json/json.c arena/arena.c -o "$BUILD_DIR/json-tests" -lm 2>&1; then
        echo "tests compiled successfully, running..."
        ./"$BUILD_DIR/json-tests"
        return $?
    else
        echo "Failed to compile tests"
        return 1
    fi
}

clean() {
    echo "Cleaning build artifacts"
    rm -rf "$BUILD_DIR"
}

case "$1" in
    help|-h|--help)
        show_usage
        exit 0
        ;;
    test)
        run_tests
        exit $?
        ;;
    clean)
        clean
        exit 0
        ;;
    *)
        echo -e "Error: unknown build target"
        show_usage
        exit 1
        ;;
esac
