#!/bin/sh

DOXYFILE="docs/Doxyfile"
OUTPUT_DIR="/docs_output"

mkdir -p $OUTPUT_DIR

echo "Starting Doxygen Watcher..."
echo "Initial generation..."
doxygen $DOXYFILE

echo "Watching for changes in /code/SDEngine..."
while inotifywait -r -e modify,create,delete /code/SDEngine; do
    echo "Change detected. Regenerating..."
    doxygen $DOXYFILE
done
