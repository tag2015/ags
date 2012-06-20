
#include <stdio.h>
#include "wgt2allg.h"
#include "acmain/ac_maindefines.h"
#include "acmain/ac_drawingsurface.h"
#include "acmain/ac_commonheaders.h"

// ** SCRIPT DRAWINGSURFACE OBJECT

void DrawingSurface_Release(ScriptDrawingSurface* sds)
{
    if (sds->roomBackgroundNumber >= 0)
    {
        if (sds->modified)
        {
            if (sds->roomBackgroundNumber == play.bg_frame)
            {
                invalidate_screen();
                mark_current_background_dirty();
            }
            play.raw_modified[sds->roomBackgroundNumber] = 1;
        }

        sds->roomBackgroundNumber = -1;
    }
    if (sds->dynamicSpriteNumber >= 0)
    {
        if (sds->modified)
        {
            int tt;
            // force a refresh of any cached object or character images
            if (croom != NULL) 
            {
                for (tt = 0; tt < croom->numobj; tt++) 
                {
                    if (objs[tt].num == sds->dynamicSpriteNumber)
                        objcache[tt].sppic = -31999;
                }
            }
            for (tt = 0; tt < game.numcharacters; tt++) 
            {
                if (charcache[tt].sppic == sds->dynamicSpriteNumber)
                    charcache[tt].sppic = -31999;
            }
            for (tt = 0; tt < game.numgui; tt++) 
            {
                if ((guis[tt].bgpic == sds->dynamicSpriteNumber) &&
                    (guis[tt].on == 1))
                {
                    guis_need_update = 1;
                    break;
                }
            }
        }

        sds->dynamicSpriteNumber = -1;
    }
    if (sds->dynamicSurfaceNumber >= 0)
    {
        destroy_bitmap(dynamicallyCreatedSurfaces[sds->dynamicSurfaceNumber]);
        dynamicallyCreatedSurfaces[sds->dynamicSurfaceNumber] = NULL;
        sds->dynamicSurfaceNumber = -1;
    }
    sds->modified = 0;
}

void ScriptDrawingSurface::MultiplyCoordinates(int *xcoord, int *ycoord)
{
    if (this->highResCoordinates)
    {
        if (current_screen_resolution_multiplier == 1) 
        {
            // using high-res co-ordinates but game running at low-res
            xcoord[0] /= 2;
            ycoord[0] /= 2;
        }
    }
    else
    {
        if (current_screen_resolution_multiplier > 1) 
        {
            // using low-res co-ordinates but game running at high-res
            xcoord[0] *= 2;
            ycoord[0] *= 2;
        }
    }
}

void ScriptDrawingSurface::MultiplyThickness(int *valueToAdjust)
{
    if (this->highResCoordinates)
    {
        if (current_screen_resolution_multiplier == 1) 
        {
            valueToAdjust[0] /= 2;
            if (valueToAdjust[0] < 1)
                valueToAdjust[0] = 1;
        }
    }
    else
    {
        if (current_screen_resolution_multiplier > 1) 
        {
            valueToAdjust[0] *= 2;
        }
    }
}

// convert actual co-ordinate back to what the script is expecting
void ScriptDrawingSurface::UnMultiplyThickness(int *valueToAdjust)
{
    if (this->highResCoordinates)
    {
        if (current_screen_resolution_multiplier == 1) 
        {
            valueToAdjust[0] *= 2;
        }
    }
    else
    {
        if (current_screen_resolution_multiplier > 1) 
        {
            valueToAdjust[0] /= 2;
            if (valueToAdjust[0] < 1)
                valueToAdjust[0] = 1;
        }
    }
}

ScriptDrawingSurface* DrawingSurface_CreateCopy(ScriptDrawingSurface *sds)
{
    BITMAP *sourceBitmap = sds->GetBitmapSurface();

    for (int i = 0; i < MAX_DYNAMIC_SURFACES; i++)
    {
        if (dynamicallyCreatedSurfaces[i] == NULL)
        {
            dynamicallyCreatedSurfaces[i] = create_bitmap_ex(bitmap_color_depth(sourceBitmap), sourceBitmap->w, sourceBitmap->h);
            blit(sourceBitmap, dynamicallyCreatedSurfaces[i], 0, 0, 0, 0, sourceBitmap->w, sourceBitmap->h);
            ScriptDrawingSurface *newSurface = new ScriptDrawingSurface();
            newSurface->dynamicSurfaceNumber = i;
            newSurface->hasAlphaChannel = sds->hasAlphaChannel;
            ccRegisterManagedObject(newSurface, newSurface);
            return newSurface;
        }
    }

    quit("!DrawingSurface.CreateCopy: too many copied surfaces created");
    return NULL;
}

void DrawingSurface_DrawSurface(ScriptDrawingSurface* target, ScriptDrawingSurface* source, int translev) {
    if ((translev < 0) || (translev > 99))
        quit("!DrawingSurface.DrawSurface: invalid parameter (transparency must be 0-99)");

    target->StartDrawing();
    BITMAP *surfaceToDraw = source->GetBitmapSurface();

    if (surfaceToDraw == abuf)
        quit("!DrawingSurface.DrawSurface: cannot draw surface onto itself");

    if (translev == 0) {
        // just draw it over the top, no transparency
        blit(surfaceToDraw, abuf, 0, 0, 0, 0, surfaceToDraw->w, surfaceToDraw->h);
        target->FinishedDrawing();
        return;
    }

    if (bitmap_color_depth(surfaceToDraw) <= 8)
        quit("!DrawingSurface.DrawSurface: 256-colour surfaces cannot be drawn transparently");

    // Draw it transparently
    trans_mode = ((100-translev) * 25) / 10;
    put_sprite_256(0, 0, surfaceToDraw);
    target->FinishedDrawing();
}

void DrawingSurface_DrawImage(ScriptDrawingSurface* sds, int xx, int yy, int slot, int trans, int width, int height)
{
    if ((slot < 0) || (slot >= MAX_SPRITES) || (spriteset[slot] == NULL))
        quit("!DrawingSurface.DrawImage: invalid sprite slot number specified");

    if ((trans < 0) || (trans > 100))
        quit("!DrawingSurface.DrawImage: invalid transparency setting");

    // 100% transparency, don't draw anything
    if (trans == 100)
        return;

    BITMAP *sourcePic = spriteset[slot];
    bool needToFreeBitmap = false;

    if (width != SCR_NO_VALUE)
    {
        // Resize specified

        if ((width < 1) || (height < 1))
            return;

        sds->MultiplyCoordinates(&width, &height);

        // resize the sprite to the requested size
        block newPic = create_bitmap_ex(bitmap_color_depth(sourcePic), width, height);

        stretch_blit(sourcePic, newPic,
            0, 0, spritewidth[slot], spriteheight[slot],
            0, 0, width, height);

        sourcePic = newPic;
        needToFreeBitmap = true;
        update_polled_stuff_if_runtime();
    }

    sds->StartDrawing();
    sds->MultiplyCoordinates(&xx, &yy);

    if (bitmap_color_depth(sourcePic) != bitmap_color_depth(abuf)) {
        debug_log("RawDrawImage: Sprite %d colour depth %d-bit not same as background depth %d-bit", slot, bitmap_color_depth(spriteset[slot]), bitmap_color_depth(abuf));
    }

    if (trans > 0)
    {
        trans_mode = ((100 - trans) * 255) / 100;
    }

    draw_sprite_support_alpha(xx, yy, sourcePic, slot);

    sds->FinishedDrawing();

    if (needToFreeBitmap)
        destroy_bitmap(sourcePic);
}


void DrawingSurface_SetDrawingColor(ScriptDrawingSurface *sds, int newColour) 
{
    sds->currentColourScript = newColour;
    // StartDrawing to set up abuf to set the colour at the appropriate
    // depth for the background
    sds->StartDrawing();
    if (newColour == SCR_COLOR_TRANSPARENT)
    {
        sds->currentColour = bitmap_mask_color(abuf);
    }
    else
    {
        sds->currentColour = get_col8_lookup(newColour);
    }
    sds->FinishedDrawingReadOnly();
}

int DrawingSurface_GetDrawingColor(ScriptDrawingSurface *sds) 
{
    return sds->currentColourScript;
}

void DrawingSurface_SetUseHighResCoordinates(ScriptDrawingSurface *sds, int highRes) 
{
    sds->highResCoordinates = (highRes) ? 1 : 0;
}

int DrawingSurface_GetUseHighResCoordinates(ScriptDrawingSurface *sds) 
{
    return sds->highResCoordinates;
}

int DrawingSurface_GetHeight(ScriptDrawingSurface *sds) 
{
    sds->StartDrawing();
    int height = abuf->h;
    sds->FinishedDrawingReadOnly();
    sds->UnMultiplyThickness(&height);
    return height;
}

int DrawingSurface_GetWidth(ScriptDrawingSurface *sds) 
{
    sds->StartDrawing();
    int width = abuf->w;
    sds->FinishedDrawingReadOnly();
    sds->UnMultiplyThickness(&width);
    return width;
}

void DrawingSurface_Clear(ScriptDrawingSurface *sds, int colour) 
{
    sds->StartDrawing();
    int allegroColor;
    if ((colour == -SCR_NO_VALUE) || (colour == SCR_COLOR_TRANSPARENT))
    {
        allegroColor = bitmap_mask_color(abuf);
    }
    else
    {
        allegroColor = get_col8_lookup(colour);
    }
    clear_to_color(abuf, allegroColor);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawCircle(ScriptDrawingSurface *sds, int x, int y, int radius) 
{
    sds->MultiplyCoordinates(&x, &y);
    sds->MultiplyThickness(&radius);

    sds->StartDrawing();
    circlefill(abuf, x, y, radius, sds->currentColour);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawRectangle(ScriptDrawingSurface *sds, int x1, int y1, int x2, int y2) 
{
    sds->MultiplyCoordinates(&x1, &y1);
    sds->MultiplyCoordinates(&x2, &y2);

    sds->StartDrawing();
    rectfill(abuf, x1,y1,x2,y2, sds->currentColour);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawTriangle(ScriptDrawingSurface *sds, int x1, int y1, int x2, int y2, int x3, int y3) 
{
    sds->MultiplyCoordinates(&x1, &y1);
    sds->MultiplyCoordinates(&x2, &y2);
    sds->MultiplyCoordinates(&x3, &y3);

    sds->StartDrawing();
    triangle(abuf, x1,y1,x2,y2,x3,y3, sds->currentColour);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawString(ScriptDrawingSurface *sds, int xx, int yy, int font, const char* texx, ...) 
{
    char displbuf[STD_BUFFER_SIZE];
    va_list ap;
    va_start(ap,texx);
    my_sprintf(displbuf,get_translation(texx),ap);
    va_end(ap);
    // don't use wtextcolor because it will do a 16->32 conversion
    textcol = sds->currentColour;

    sds->MultiplyCoordinates(&xx, &yy);
    sds->StartDrawing();
    wtexttransparent(TEXTFG);
    if ((bitmap_color_depth(abuf) <= 8) && (play.raw_color > 255)) {
        wtextcolor(1);
        debug_log ("RawPrint: Attempted to use hi-color on 256-col background");
    }
    wouttext_outline(xx, yy, font, displbuf);
    sds->FinishedDrawing();
}

void DrawingSurface_DrawStringWrapped(ScriptDrawingSurface *sds, int xx, int yy, int wid, int font, int alignment, const char *msg) {
    int texthit = wgetfontheight(font);
    sds->MultiplyCoordinates(&xx, &yy);
    sds->MultiplyThickness(&wid);

    break_up_text_into_lines(wid, font, (char*)msg);

    textcol = sds->currentColour;
    sds->StartDrawing();

    wtexttransparent(TEXTFG);
    for (int i = 0; i < numlines; i++)
    {
        int drawAtX = xx;

        if (alignment == SCALIGN_CENTRE)
        {
            drawAtX = xx + ((wid / 2) - wgettextwidth(lines[i], font) / 2);
        }
        else if (alignment == SCALIGN_RIGHT)
        {
            drawAtX = (xx + wid) - wgettextwidth(lines[i], font);
        }

        wouttext_outline(drawAtX, yy + texthit*i, font, lines[i]);
    }

    sds->FinishedDrawing();
}

void DrawingSurface_DrawMessageWrapped(ScriptDrawingSurface *sds, int xx, int yy, int wid, int font, int msgm)
{
    char displbuf[3000];
    get_message_text(msgm, displbuf);
    // it's probably too late but check anyway
    if (strlen(displbuf) > 2899)
        quit("!RawPrintMessageWrapped: message too long");

    DrawingSurface_DrawStringWrapped(sds, xx, yy, wid, font, SCALIGN_LEFT, displbuf);
}

void DrawingSurface_DrawLine(ScriptDrawingSurface *sds, int fromx, int fromy, int tox, int toy, int thickness) {
    sds->MultiplyCoordinates(&fromx, &fromy);
    sds->MultiplyCoordinates(&tox, &toy);
    sds->MultiplyThickness(&thickness);
    int ii,jj,xx,yy;
    sds->StartDrawing();
    // draw several lines to simulate the thickness
    for (ii = 0; ii < thickness; ii++) 
    {
        xx = (ii - (thickness / 2));
        for (jj = 0; jj < thickness; jj++)
        {
            yy = (jj - (thickness / 2));
            line (abuf, fromx + xx, fromy + yy, tox + xx, toy + yy, sds->currentColour);
        }
    }
    sds->FinishedDrawing();
}

void DrawingSurface_DrawPixel(ScriptDrawingSurface *sds, int x, int y) {
    sds->MultiplyCoordinates(&x, &y);
    int thickness = 1;
    sds->MultiplyThickness(&thickness);
    int ii,jj;
    sds->StartDrawing();
    // draw several pixels to simulate the thickness
    for (ii = 0; ii < thickness; ii++) 
    {
        for (jj = 0; jj < thickness; jj++)
        {
            putpixel(abuf, x + ii, y + jj, sds->currentColour);
        }
    }
    sds->FinishedDrawing();
}

int DrawingSurface_GetPixel(ScriptDrawingSurface *sds, int x, int y) {
    sds->MultiplyCoordinates(&x, &y);
    sds->StartDrawing();
    unsigned int rawPixel = getpixel(abuf, x, y);
    unsigned int maskColor = bitmap_mask_color(abuf);
    int colDepth = bitmap_color_depth(abuf);

    if (rawPixel == maskColor)
    {
        rawPixel = SCR_COLOR_TRANSPARENT;
    }
    else if (colDepth > 8)
    {
        int r = getr_depth(colDepth, rawPixel);
        int g = getg_depth(colDepth, rawPixel);
        int b = getb_depth(colDepth, rawPixel);

        rawPixel = Game_GetColorFromRGB(r, g, b);
    }

    sds->FinishedDrawingReadOnly();

    return rawPixel;
}




