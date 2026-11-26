"""Monta uma folha de contato PNG com todas as variantes (ctypes + cairo)."""

import ctypes
import os

cairo = ctypes.CDLL("libcairo.so.2")
cairo.cairo_image_surface_create.restype = ctypes.c_void_p
cairo.cairo_image_surface_create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
cairo.cairo_create.restype = ctypes.c_void_p
cairo.cairo_create.argtypes = [ctypes.c_void_p]
cairo.cairo_set_source_rgb.argtypes = [ctypes.c_void_p] + [ctypes.c_double] * 3
cairo.cairo_paint.argtypes = [ctypes.c_void_p]
cairo.cairo_set_source_surface.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                           ctypes.c_double, ctypes.c_double]
cairo.cairo_surface_write_to_png.restype = ctypes.c_int
cairo.cairo_surface_write_to_png.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
cairo.cairo_image_surface_create_from_png.restype = ctypes.c_void_p
cairo.cairo_image_surface_create_from_png.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
cairo.cairo_destroy.argtypes = [ctypes.c_void_p]
cairo.cairo_surface_destroy.argtypes = [ctypes.c_void_p]

cairo.cairo_select_font_face.argtypes = [ctypes.c_char_p] * 4
cairo.cairo_set_font_size.argtypes = [ctypes.c_void_p, ctypes.c_double]
cairo.cairo_show_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
cairo.cairo_move_to.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double]
cairo.cairo_select_font_face.argtypes = [ctypes.c_void_p, ctypes.c_char_p,
                                         ctypes.c_int, ctypes.c_int]
cairo.cairo_set_font_size.argtypes = [ctypes.c_void_p, ctypes.c_double]
cairo.cairo_show_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
cairo.cairo_set_source_surface.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                           ctypes.c_double, ctypes.c_double]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    slugs = sorted(d for d in os.listdir(here)
                   if os.path.isdir(os.path.join(here, d)) and d[0].isdigit())
    cols, cell, pad, top = 5, 220, 18, 40
    rows = (len(slugs) + cols - 1) // cols
    w = cols * (cell + pad) + pad
    h = top + rows * (cell + pad + 26) + pad
    surf = cairo.cairo_image_surface_create(0, w, h)
    cr = cairo.cairo_create(surf)
    cairo.cairo_set_source_rgb(cr, 0.10, 0.11, 0.13)
    cairo.cairo_paint(cr)
    for i, slug in enumerate(slugs):
        png = os.path.join(here, slug, "icon-512.png")
        if not os.path.exists(png):
            continue
        img = cairo.cairo_image_surface_create_from_png(png, None)
        x = pad + (i % cols) * (cell + pad)
        y = top + (i // cols) * (cell + pad + 26)
        cairo.cairo_set_source_surface(cr, img, x, y)
        cairo.cairo_paint(cr)
        cairo.cairo_surface_destroy(img)
        cairo.cairo_select_font_face(cr, b"sans", 0, 0)
        cairo.cairo_set_font_size(cr, 13)
        cairo.cairo_set_source_rgb(cr, 0.85, 0.87, 0.9)
        cairo.cairo_move_to(cr, x, y + cell + 18)
        cairo.cairo_show_text(cr, slug.encode())
    out = os.path.join(here, "contact-sheet.png")
    cairo.cairo_surface_write_to_png(surf, out.encode())
    cairo.cairo_destroy(cr)
    cairo.cairo_surface_destroy(surf)
    print(out)


if __name__ == "__main__":
    main()
