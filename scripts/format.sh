HEADERS=$(find ./src -name "*.h")
SOURCES=$(find ./src -name "*.cpp")

clang-format -i $HEADERS $SOURCES

clang-tidy -p=../compile_commands.json --fix-errors --fix-notes $HEADERS $SOURCES