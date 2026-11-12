#!/bin/sh
# Renders the UI on your PC into out/*.ppm (needs gcc). No PSP needed.
mkdir -p out && gcc -O1 -I src tools/preview.c src/ui.c src/gfx.c -lm -o out/preview && ./out/preview && echo "see out/*.ppm"
