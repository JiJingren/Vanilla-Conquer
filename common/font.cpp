//
// Copyright 2020 Electronic Arts Inc.
//
// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection

/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D   A S S O C I A T E S   **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : LIBRARY                                  *
 *                                                                         *
 *                    File Name : FONT.C                                   *
 *                                                                         *
 *                   Programmer : David Dettmer                            *
 *                                                                         *
 *                  Last Update : July 20, 1994   [SKB]                    *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 *   Char_Pixel_Width -- Return pixel width of a character.                *
 *   String_Pixel_Width -- Return pixel width of a string of characters.   *
 *   Get_Next_Text_Print_XY -- Calculates X and Y given ret value from Text_P*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "font.h"

#include "gbuffer.h"
#include "file.h"
#include "memflag.h"
#include "endianness.h"
#include "rawfile.h"

#include <errno.h>
#include <string.h>

int FontXSpacing = 0;
int FontYSpacing = 0;
void const* FontPtr = nullptr;
char FontWidth = 8;
char FontHeight = 8;

// only font.c and set_font.c use the following
char* FontWidthBlockPtr = nullptr;

unsigned char ColorXlat[16][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

/*
**	Optional CJK/UTF-8 bitmap font support. When a font data file is loaded via
**	Load_CJK_Font(), any UTF-8 encoded multibyte character (typically CJK) is
**	rendered using this font instead of the single byte game font. The file
**	format is a flat dump:
**	  'C' 'J' 'K' 'F', u16 width, u16 height, u32 count,
**	  count * { u32 codepoint, height * u16 rows (MSB first) }
*/
static const unsigned char* CJKFontData = nullptr;
static const unsigned char* CJKEntries = nullptr;
static unsigned short CJKFontWidth = 0;
static unsigned short CJKFontHeight = 0;
static unsigned int CJKFontCount = 0;
static int CJKFontStride = 0;
static bool CJKFontLoaded = false;

void Load_CJK_Font(char const* name)
{
    RawFileClass file(name);

    if (!file.Is_Available()) {
        return;
    }

    int size = file.Size();

    if (size < 12) {
        return;
    }

    unsigned char* buffer = new unsigned char[size];

    if (file.Read(buffer, size) != size) {
        delete[] buffer;
        return;
    }

    if (buffer[0] != 'C' || buffer[1] != 'J' || buffer[2] != 'K' || buffer[3] != 'F') {
        delete[] buffer;
        return;
    }

    unsigned short width = (unsigned short)(buffer[4] | (buffer[5] << 8));
    unsigned short height = (unsigned short)(buffer[6] | (buffer[7] << 8));
    unsigned int count = (unsigned int)buffer[8] | ((unsigned int)buffer[9] << 8) | ((unsigned int)buffer[10] << 16)
                         | ((unsigned int)buffer[11] << 24);
    int stride = 4 + height * 2;

    if (width == 0 || height == 0 || count == 0 || 12 + (long)count * stride > size) {
        delete[] buffer;
        return;
    }

    if (CJKFontData != nullptr) {
        delete[] CJKFontData;
    }

    CJKFontData = buffer;
    CJKEntries = buffer + 12;
    CJKFontWidth = width;
    CJKFontHeight = height;
    CJKFontCount = count;
    CJKFontStride = stride;
    CJKFontLoaded = true;
}

/*
**	Binary searches the loaded CJK font for the supplied codepoint. Returns a
**	pointer to the raw glyph rows, or nullptr if the codepoint is not present.
*/
static const unsigned char* CJK_Find(unsigned int codepoint)
{
    if (!CJKFontLoaded) {
        return nullptr;
    }

    int lo = 0;
    int hi = (int)CJKFontCount - 1;

    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        const unsigned char* entry = CJKEntries + (long)mid * CJKFontStride;
        unsigned int value = (unsigned int)entry[0] | ((unsigned int)entry[1] << 8) | ((unsigned int)entry[2] << 16)
                             | ((unsigned int)entry[3] << 24);

        if (value == codepoint) {
            return entry + 4;
        }

        if (value < codepoint) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return nullptr;
}

/*
**	Decodes a single UTF-8 encoded codepoint and advances the string pointer past
**	it. Malformed or truncated sequences are treated as a single raw byte so that
**	the caller can never walk off the end of the buffer.
*/
static unsigned int UTF8_Decode(char const** string)
{
    const unsigned char* p = (const unsigned char*)*string;
    unsigned int c = p[0];

    if (c < 0x80) {
        *string += 1;
        return c;
    }

    if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        *string += 2;
        return ((c & 0x1F) << 6) | (p[1] & 0x3F);
    }

    if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        *string += 3;
        return ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }

    if ((c & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        *string += 4;
        return ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    }

    *string += 1;
    return c;
}

/***************************************************************************
 * SET_FONT -- Changes the default text printing font.                     *
 *                                                                         *
 *    This routine will change the default text printing font for all      *
 *    text output.  It handles updating the system where necessary.        *
 *                                                                         *
 * INPUT:   fontptr  -- Pointer to the font to change to.                  *
 *                                                                         *
 * OUTPUT:  Returns with a pointer to the previous font.                   *
 *                                                                         *
 * WARNINGS:   none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   09/06/1991 JLB : Created.                                             *
 *   09/17/1991 JLB : Fixed return value bug.                              *
 *   01/31/1992 DRD : Modified to use new font format.                     *
 *   06/29/1994 SKB : modified for 32 bit library                          *
 *=========================================================================*/
void* Set_Font(void const* fontptr)
{
    void* oldfont = (void*)FontPtr;

    if (fontptr) {
        FontPtr = (void*)fontptr;

        /*
        **	Inform the system about the new font.
        */
        unsigned short fontwidthblock;
        unsigned short fontinfoblock;

        memcpy(&fontwidthblock, (char*)fontptr + FONTWIDTHBLOCK, sizeof(short));
        memcpy(&fontinfoblock, (char*)fontptr + FONTINFOBLOCK, sizeof(short));

        fontwidthblock = le16toh(fontwidthblock);
        fontinfoblock = le16toh(fontinfoblock);

        FontWidthBlockPtr = (char*)fontptr + fontwidthblock;
        char const* blockptr = (char*)fontptr + fontinfoblock;
        FontHeight = *(blockptr + FONTINFOMAXHEIGHT);
        FontWidth = *(blockptr + FONTINFOMAXWIDTH);
    }

    return oldfont;
}

/***************************************************************************
 * CHAR_PIXEL_WIDTH -- Return pixel width of a character.                  *
 *                                                                         *
 *    Retreives the pixel width of a character from the font width block.  *
 *                                                                         *
 * INPUT:      Character.                                                  *
 *                                                                         *
 * OUTPUT:     Pixel width of a string of characters.                      *
 *                                                                         *
 * WARNINGS:   Set_Font must have been called first.                       *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/31/1992 DRD : Created.                                             *
 *   06/30/1994 SKB : Converted to 32 bit library.                         *
 *=========================================================================*/
int Char_Pixel_Width(char chr)
{
    unsigned char c = (unsigned char)chr;

    if (c < 0x80) {
        return (unsigned char)*(FontWidthBlockPtr + c) + FontXSpacing;
    }

    if (c < 0xC0) {
        /*
        **	UTF-8 continuation byte. Its width is accounted for by the leading
        **	byte of the multibyte sequence, so it contributes nothing here.
        */
        return 0;
    }

    /*
    **	Leading byte of a multibyte UTF-8 sequence (typically CJK). Report the
    **	full width of the glyph so byte oriented width calculations stay correct.
    */
    return (CJKFontLoaded ? CJKFontWidth : FontWidth) + FontXSpacing;
}

/***************************************************************************
 * NEXT_TEXT_CHAR -- Step forward one whole (possibly multibyte) character. *
 *                                                                         *
 *    Text wrapping code frequently walks raw bytes and then chops the     *
 *      string at an arbitrary byte.  These two helpers let it move in     *
 *      whole characters instead so a split never lands in the middle of   *
 *      a UTF-8 sequence.                                                 *
 *                                                                         *
 * INPUT:      Pointer to the first byte of a character.                  *
 *                                                                         *
 * OUTPUT:     Pointer to the first byte of the following character.      *
 ****************************************************************************/

char const* Next_Text_Char(char const* p)
{
    if (!p || !*p) {
        return p;
    }

    p++;

    /*
    **	Skip any UTF-8 continuation bytes. Stopping at the terminator keeps
    **	truncated sequences from walking off the end of the buffer.
    */
    while (*p && (((unsigned char)*p) & 0xC0) == 0x80) {
        p++;
    }

    return p;
}

/***************************************************************************
 * PREVIOUS_TEXT_CHAR -- Step backward one whole (possibly multibyte) char *
 ****************************************************************************/

char const* Previous_Text_Char(char const* p)
{
    if (!p) {
        return p;
    }

    p--;

    /*
    **	Back over the continuation bytes to reach the leading byte.
    */
    while (p && (((unsigned char)*p) & 0xC0) == 0x80) {
        p--;
    }

    return p;
}

/***************************************************************************
 * STRING_PIXEL_WIDTH -- Return pixel width of a string of characters.     *
 *                                                                         *
 *    Calculates the pixel width of a string of characters.  This uses     *
 *      the font width block for the widths.                               *
 *                                                                         *
 * INPUT:      Pointer to string of characters.                            *
 *                                                                         *
 * OUTPUT:     Pixel width of a string of characters.                      *
 *                                                                         *
 * WARNINGS:   Set_Font must have been called first.                       *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/30/1992 DRD : Created.                                             *
 *   01/31/1992 DRD : Use Char_Pixel_Width.                                *
 *   06/30/1994 SKB : Converted to 32 bit library.                         *
 *=========================================================================*/
unsigned int String_Pixel_Width(char const* string)
{
    if (!string) {
        return 0;
    }

    unsigned short largest = 0; // Largest recorded width of the string.
    unsigned short width = 0;   // Working accumulator of string width.
    while (*string) {
        if (*string == '\r') {
            string++;
            largest = MAX(largest, width);
            width = 0;
        } else {
            width += Char_Pixel_Width(*string++); // add each char's width
        }
    }
    largest = MAX(largest, width);
    return largest;
}

/***************************************************************************
 * GET_NEXT_TEXT_PRINT_XY -- Calculates X and Y given ret value from Text_P*
 *                                                                         *
 *                                                                         *
 * INPUT:   VVPC& vp - viewport that was printed to.                       *
 *          unsigned int offset - offset that Text_Print returned.        *
 *          INT *x - x return value.                                       *
 *          INT *y - y return value.                                       *
 *                                                                         *
 * OUTPUT:  x and y are set.                                               *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   07/20/1994 SKB : Created.                                             *
 *=========================================================================*/
void Get_Next_Text_Print_XY(GraphicViewPortClass& gp, unsigned int offset, int* x, int* y)
{
    if (offset) {
        int buffwidth = gp.Get_Width() + gp.Get_XAdd();
        offset -= gp.Get_Offset();
        *x = offset % buffwidth;
        *y = offset / buffwidth;
    } else {
        *x = *y = 0;
    }
}

void Set_Font_Palette_Range(void const* palette, int start_idx, int end_idx)
{
    start_idx &= 15;
    end_idx &= 15;
    const unsigned char* colors = (const unsigned char*)palette;

    for (int i = start_idx; i < end_idx + 1; ++i) {
        ColorXlat[0][i] = *colors;
        ColorXlat[i][0] = *colors++;
    }
}

/***************************************************************************
 * LOAD_FONT -- Loads a font from disk.                                    *
 *                                                                         *
 *    This loads a font from disk.  This function must be called as a      *
 *    precursor to calling Set_Font().  You need only call this function   *
 *    once per desired font at the beginning of your code, but AFTER       *
 *    Prog_Init() is called.                                               *
 *                                                                         *
 * INPUT:      name  - Pointer to font name to use (eg. "topaz.font")      *
 *                                                                         *
 *             fontsize - Size in points of the font loaded.               *
 *                                                                         *
 * OUTPUT:     Pointer to font data or NULL if unable to load.             *
 *                                                                         *
 * WARNINGS:   Some system memory is grabbed by this routine.              *
 *                                                                         *
 * HISTORY:                                                                *
 *   4/10/91    BS  : 2.0 compatibily                                      *
 *   6/09/91    JLB : IBM and Amiga compatability.                         *
 *   11/27/1991 JLB : Uses file I/O routines for disk access.              *
 *   01/29/1992 DRD : Modified to use new font format.                     *
 *   02/01/1992 DRD : Added font file verification.                        *
 *   06/29/1994 SKB : modified for 32 bit library                          *
 *=========================================================================*/
void* Load_Font(char const* name)
{
    char valid;
    int fh;              // DOS file handle for font file.
    unsigned short size; // Size of the data in the file (-2);
    char* ptr = NULL;    // Pointer to newly loaded font.

    fh = Open_File(name, READ);
    if (fh >= 0) {
        if (Read_File(fh, (char*)&size, 2) != 2)
            return (NULL);

        ptr = (char*)Alloc(size, MEM_NORMAL);
        *(short*)ptr = size;
        Read_File(fh, ptr + 2, size - 2);
        Close_File(fh);
    } else {
        return ((void*)(intptr_t)errno);
    }

    //
    // verify that the file loaded is a valid font file.
    //

    valid = false;
    if (*(ptr + 2) == 0) {     // no compression
        if (*(ptr + 3) == 5) { // currently only 5 data blocks are used.
            valid = true;
        }
    }

    if (!valid) {
        return (NULL);
    }

    return (ptr);
}

#pragma pack(push, 1)
struct FontHeader
{
    unsigned short FontLength;        // Size of the file
    unsigned char FontCompress;       // file version? 0==EOB? 1==RA? 2==TS? what about RA2?
    unsigned char FontDataBlocks;     // 5==TD/RA 0==TS? unused in RA
    unsigned short InfoBlockOffset;   // offset to start of font description Pad, always 0x0010
    unsigned short OffsetBlockOffset; // offset to bitmap data offset list, always 0x0014 as follows header
    unsigned short WidthBlockOffset;  // points to list of widths
    unsigned short DataBlockOffset;   // points to graphics data, unused by RA code
    unsigned short HeightOffset;      // points to list of heights (or lines)
    unsigned short UnknownConst;      // Unknown entry (always 0x1012 or 0x1011) unused by RA
    unsigned char Pad;                // padding?
    unsigned char CharCount;          // Number of chars in font - 1
    unsigned char MaxHeight;          // Max char height
    unsigned char MaxWidth;           // Max char width
};
#pragma pack(pop)
/***************************************************************************
 * Buffer_Print -- C++ text print to graphic buffer routine                *
 *                                                                         *
 *                                                                         *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * PROTO:                                                                  *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   01/17/1995 PWG : Created.                                             *
 *   18/08/2020 OmniBlade : Translation to C++ added.                      *
 *=========================================================================*/
int Buffer_Print(void* thisptr, const char* string, int x, int y, int fground, int bground)
{
    GraphicViewPortClass& vp = *static_cast<GraphicViewPortClass*>(thisptr);
    const FontHeader* fntheader = reinterpret_cast<const FontHeader*>(FontPtr);
    int pitch = vp.Get_XAdd() + vp.Get_Width() + vp.Get_Pitch();
    unsigned char* offset = y * pitch + reinterpret_cast<unsigned char*>(vp.Get_Offset());
    unsigned char* dst = x + offset;
    int char_width = 0;
    int base_x = x;

    if (FontPtr != nullptr) {
        const unsigned short* datalist = reinterpret_cast<const unsigned short*>(
            reinterpret_cast<const char*>(FontPtr) + le16toh(fntheader->OffsetBlockOffset));
        const unsigned char* widthlist =
            reinterpret_cast<const unsigned char*>(FontPtr) + le16toh(fntheader->WidthBlockOffset);
        const unsigned short* linelist = reinterpret_cast<const unsigned short*>(reinterpret_cast<const char*>(FontPtr)
                                                                                 + le16toh(fntheader->HeightOffset));

        int fntheight = fntheader->MaxHeight;
        int ydisplace = FontYSpacing + fntheight;

        // Check if we are drawing in bounds, we don't draw clipped text
        if (y + fntheight <= vp.Get_Height()) {
            int fntbottom = y + fntheight;
            // Set colors to draw with
            ColorXlat[0][1] = fground;
            ColorXlat[0][0] = bground;

            while (true) {
                // Handle a new line
                unsigned char char_num;
                unsigned char* char_dst;
                while (true) {
                    char_num = *string;

                    if (char_num == '\0') {
                        return 0;
                    }

                    char_dst = dst;
                    ++string;

                    if (char_num != '\n' && char_num != '\r') {
                        break;
                    }

                    // We don't handle clipping text, it either draws or it doesn't
                    if (ydisplace + fntbottom > vp.Get_Height()) {
                        return 0;
                    }

                    // If its a new line, we are just going to set the start to an increased y displacement, hence x_pos
                    // becomes 0
                    x = char_num == '\n' ? 0 : base_x;
                    dst = ydisplace * pitch + offset + x;
                    offset += ydisplace * pitch;
                    fntbottom += ydisplace;
                }

                // Handle UTF-8 encoded (CJK) characters using the optional CJK font
                if (char_num >= 0x80 && CJKFontLoaded) {
                    const char* cjk_string = string - 1;
                    unsigned int codepoint = UTF8_Decode(&cjk_string);
                    int cjk_width = CJKFontWidth;

                    // Handle text wrapping for long strings
                    if (FontXSpacing + cjk_width + x > vp.Get_Width()) {
                        // We don't handle clipping text, it either draws or it doesn't
                        if (ydisplace + fntbottom > vp.Get_Height()) {
                            return 0;
                        }

                        x = base_x;
                        dst = ydisplace * pitch + offset + x;
                        offset += ydisplace * pitch;
                        fntbottom += ydisplace;
                        --string;
                        continue;
                    }

                    const unsigned char* glyph = CJK_Find(codepoint);
                    unsigned char cjk_fg = ColorXlat[0][1];
                    unsigned char cjk_bg = ColorXlat[0][0];
                    unsigned char* cell_dst = char_dst;

                    for (int ry = 0; ry < CJKFontHeight; ++ry) {
                        unsigned short row = 0;

                        if (glyph) {
                            row = (unsigned short)((glyph[ry * 2] << 8) | glyph[ry * 2 + 1]);
                        }

                        for (int rx = 0; rx < cjk_width; ++rx) {
                            unsigned char color = (row & (0x8000 >> rx)) ? cjk_fg : cjk_bg;

                            if (color) {
                                cell_dst[rx] = color;
                            }
                        }

                        cell_dst += pitch;
                    }

                    string = cjk_string;
                    dst += FontXSpacing + cjk_width;
                    x += FontXSpacing + cjk_width;
                    continue;
                }

                // Move to the start of the next char
                char_width = widthlist[char_num];
                dst += FontXSpacing + char_width;

                // Handle text wrapping for long strings
                if (FontXSpacing + char_width + x > vp.Get_Width()) {
                    --string;
                    char_num = '\0';

                    // We don't handle clipping text, it either draws or it doesn't
                    if (ydisplace + fntbottom > vp.Get_Height()) {
                        return 0;
                    }

                    // If its a new line, we are just going to set the start to an increased y displacement, hence x_pos
                    // becomes 0
                    x = char_num == '\n' ? 0 : base_x;
                    dst = ydisplace * pitch + offset + x;
                    offset += ydisplace * pitch;
                    fntbottom += ydisplace;

                    continue;
                }

                // Prepare variables for drawing
                x += FontXSpacing + char_width;
                int next_line = pitch - char_width;
                unsigned short dlist;
                memcpy(&dlist, datalist + char_num, sizeof(unsigned short));
                dlist = le16toh(dlist);
                const unsigned char* char_data = reinterpret_cast<const unsigned char*>(FontPtr) + dlist;
                short char_lle;
                memcpy(&char_lle, linelist + char_num, sizeof(short));
                char_lle = le16toh(char_lle);
                int char_ypos = char_lle & 0xFF;
                int char_lines = char_lle >> 8;
                int char_height = fntheight - (char_ypos + char_lines);
                int blit_width = widthlist[char_num];

                // Fill unused lines if we have a color other than 0
                if (char_ypos) {
                    unsigned char color = ColorXlat[0][0];
                    if (color) {
                        for (int i = char_ypos; i; --i) {
                            memset(char_dst, color, blit_width);
                            char_dst += pitch;
                        }
                    } else {
                        char_dst += pitch * char_ypos;
                    }
                }

                // Draw the character
                if (char_lines) {
                    for (int i = 0; i < char_lines; ++i) {
                        int width_todraw = blit_width;

                        while (width_todraw) {
                            unsigned char color_packed;
                            color_packed = *char_data++;
                            unsigned char color = ColorXlat[0][color_packed & 0x0F];

                            if (color) {
                                *char_dst = color;
                            }

                            ++char_dst;
                            --width_todraw;

                            if (width_todraw == 0) {
                                break;
                            }

                            color = ColorXlat[0][color_packed >> 4];

                            if (color) {
                                *char_dst = color;
                            }

                            ++char_dst;
                            --width_todraw;
                        }

                        char_dst += next_line;
                    }

                    // Fill any remaining unused lines.
                    if (char_height) {
                        unsigned char color = ColorXlat[0][0];

                        if (color) {
                            for (int i = char_height; i; --i) {
                                memset(char_dst, color, blit_width);
                                char_dst += pitch;
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}

void* Get_Font_Palette_Ptr()
{
    return ColorXlat;
}
