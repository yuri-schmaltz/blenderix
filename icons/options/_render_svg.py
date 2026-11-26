"""Rasterizador SVG minimo via ctypes (librsvg + cairo), sem dependencias pip."""

import ctypes

rsvg = ctypes.CDLL("librsvg-2.so.2")
cairo = ctypes.CDLL("libcairo.so.2")
gobj = ctypes.CDLL("libgobject-2.0.so.0")


class RsvgRectangle(ctypes.Structure):
    _fields_ = [
        ("x", ctypes.c_double),
        ("y", ctypes.c_double),
        ("width", ctypes.c_double),
        ("height", ctypes.c_double),
    ]


rsvg.rsvg_handle_new_from_data.restype = ctypes.c_void_p
rsvg.rsvg_handle_new_from_data.argtypes = [
    ctypes.c_char_p,
    ctypes.c_size_t,
    ctypes.c_void_p,
]
rsvg.rsvg_handle_render_document.restype = ctypes.c_bool
rsvg.rsvg_handle_render_document.argtypes = [
    ctypes.c_void_p,
    ctypes.c_void_p,
    ctypes.POINTER(RsvgRectangle),
    ctypes.c_void_p,
]
cairo.cairo_image_surface_create.restype = ctypes.c_void_p
cairo.cairo_image_surface_create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
cairo.cairo_create.restype = ctypes.c_void_p
cairo.cairo_create.argtypes = [ctypes.c_void_p]
cairo.cairo_set_source_rgb.argtypes = [ctypes.c_void_p] + [ctypes.c_double] * 3
cairo.cairo_paint.argtypes = [ctypes.c_void_p]
cairo.cairo_surface_write_to_png.restype = ctypes.c_int
cairo.cairo_surface_write_to_png.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
cairo.cairo_destroy.argtypes = [ctypes.c_void_p]
cairo.cairo_surface_destroy.argtypes = [ctypes.c_void_p]
gobj.g_object_unref.argtypes = [ctypes.c_void_p]


def render(svg_path, png_path, size, background=(1, 1, 1)):
    """Renderiza svg_path para png_path com lado `size`."""
    data = open(svg_path, "rb").read()
    err = ctypes.c_void_p()
    handle = rsvg.rsvg_handle_new_from_data(data, len(data), ctypes.byref(err))
    if not handle:
        raise RuntimeError(f"falha ao parsear SVG: {svg_path}")
    surface = cairo.cairo_image_surface_create(0, size, size)
    ctx = cairo.cairo_create(surface)
    cairo.cairo_set_source_rgb(ctx, *background)
    cairo.cairo_paint(ctx)
    viewport = RsvgRectangle(0, 0, size, size)
    ok = rsvg.rsvg_handle_render_document(
        handle, ctx, ctypes.byref(viewport), ctypes.byref(err)
    )
    if not ok:
        raise RuntimeError(f"falha ao renderizar SVG: {svg_path}")
    status = cairo.cairo_surface_write_to_png(surface, png_path.encode())
    cairo.cairo_destroy(ctx)
    cairo.cairo_surface_destroy(surface)
    gobj.g_object_unref(handle)
    return status
