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
    clean         Remove build artifacts
    help          Show this message
EOF
}

build_json_runner() {
    if gcc $CFLAGS json-runner.c json/json.c arena/arena.c -o "$BUILD_DIR/json-runner" -lm 2>&1; then
        echo -e "${GREEN} json runner compiled successfully${NC}"
        echo -e "  ${YELLOW}Output: $output${NC}"
        return 0
    else
        echo -e "${RED} Failed to compile $util${NC}"
        return 1
    fi

}

clean() {
    echo -e "Cleaning build artifacts"
    rm -f "$BUILD_DIR"/json-tester
}

case "$1" in
    help|-h|--help)
        show_usage
        exit 0
        ;;
    json-runner)
        build_json_runner
        exit 0
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
