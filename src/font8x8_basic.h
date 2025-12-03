#pragma once
#include <vector>
#include <string>
#include <cstdint>

static const uint8_t font8x8_basic[128][8] = {
#include "font8x8_data.inc"   // keeps this file short — see note below
};

// Draw one character
inline void drawChar(int x, int y, char c, std::vector<unsigned char>& pixels,
                     int width, int height) {
    if (c < 0 || c > 127) return;
    const uint8_t* bitmap = font8x8_basic[(int)c];
    for (int row=0; row<8; ++row)
        for (int col=0; col<8; ++col)
            if (bitmap[row] & (1 << col)) {
                // int px=x+col, py=y+row;
                int px = x + col;
                int py = height - 1 - (y + row); // flipped Y
                if (px>=0 && px<width && py>=0 && py<height) {
                    int i=(py*width+px)*3;
                    pixels[i]=pixels[i+1]=pixels[i+2]=255;
                }
            }
}

// Draw a string
inline void drawText(int x, int y, const std::string& text,
                     std::vector<unsigned char>& pixels,
                     int width, int height) {
    for (size_t i=0;i<text.size();++i)
        drawChar(x+i*8, y, text[i], pixels, width, height);
}
