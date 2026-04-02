#!/usr/bin/env bash

LC_NUMERIC=C
BUILD_DIR="bin"

mkdir -p $BUILD_DIR

CFLAGS="-Wall -Wextra"

show_usage() {
    cat << EOF
N-Body Simulation Runner Build Script

Usage: $0 [COMMAND]

Commands:
    serial        Build serial n-body simulation
    json-runner   Build json runner utility only
    test          Build and run json tests
    clean         Remove build artifacts
    help          Show this message
EOF
}

build_serial() {
    echo "Building serial n-body simulation..."
    if gcc $CFLAGS smain.c args/config-parsing.c arena/arena.c json/json.c simulation/simulation.c world/world.c world/initial_state.c -o "$BUILD_DIR/nbody-serial" -lm; then
        echo "Successfully built $BUILD_DIR/nbody-serial"
        return 0
    else
        echo "Failed to build serial simulation"
        return 1
    fi
}

run_tests() {
    if gcc $CFLAGS json_tests.c json/json.c arena/arena.c -o "$BUILD_DIR/json-tests" -lm 2>&1; then
        echo "tests compiled successfully, running..."
        
        start=$(date +%s.%N)
        ./"$BUILD_DIR/json-tests"
        exit_code=$?
        end=$(date +%s.%N)

        duration=$(echo "$end - $start" | bc)
        printf "Execution time: %.6f seconds\n" "$duration"

        return $exit_code
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
    serial)
        build_serial
        exit $?
        ;;
    test)
        run_tests
        exit $?
        ;;
    clean)
        clean
        exit 0
        ;;
    "")
        build_serial
        exit $?
        ;;
    *)
        echo -e "Error: unknown build target"
        show_usage
        exit 1
        ;;
esac
