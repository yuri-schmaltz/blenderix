#!/usr/bin/env python3
"""Gera 10 variantes de icone derivadas do logo oficial do Blender.

Geometria base: release/darwin/blender_liquid_glass.icon/Assets/Blender Logo.svg
(viewBox 0 0 1024 1024; paths: cabeca/olho branco, corpo laranja, olho azul).

Saida: icons/options/<slug>/icon.svg, icon-512.png, preview-128.png
"""

import math
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
DARWIN_LOGO = os.path.join(
    REPO, "release/darwin/blender_liquid_glass.icon/Assets/Blender Logo.svg"
)

VIEW = 1024
HEAD_CX, HEAD_CY, HEAD_R = 551.3, 389.1, 221.0

BLENDER_ORANGE = "#e87d0d"
BLENDER_BLUE = "#265787"


def load_paths():
    src = open(DARWIN_LOGO, encoding="utf-8").read()
    paths = re.findall(
        r'<path\s+d="([^"]+)"[^>]*?fill="(#[0-9A-Fa-f]{6})"', src
    )
    head = next(d for d, f in paths if f.lower() == "#ffffff")
    body = next(d for d, f in paths if f.lower() == "#e87d0d")
    eye = next(d for d, f in paths if f.lower() == "#265787")
    return head, body, eye


HEAD, BODY, EYE = load_paths()


def circle(cx, cy, r, fill, opacity=None):
    op = f' opacity="{opacity}"' if opacity is not None else ""
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}"{op}/>'


def facet_disc(cx, cy, r, colors, n=18, inner=0.52):
    """Disco facetado low-poly: leque de triangulos em duas coruras."""
    out = [f'<clipPath id="facetClip"><circle cx="{cx}" cy="{cy}" r="{r}"/></clipPath>',
           '<g clip-path="url(#facetClip)">']
    ri = r * inner
    for i in range(n):
        a0 = 2 * math.pi * i / n - math.pi / 2
        a1 = 2 * math.pi * (i + 1) / n - math.pi / 2
        am = (a0 + a1) / 2
        p0 = (cx + r * math.cos(a0), cy + r * math.sin(a0))
        p1 = (cx + r * math.cos(a1), cy + r * math.sin(a1))
        p2 = (cx + ri * math.cos(am), cy + ri * math.sin(am))
        out.append(
            f'<path d="M{p0[0]:.1f} {p0[1]:.1f} L{p1[0]:.1f} {p1[1]:.1f} '
            f'L{p2[0]:.1f} {p2[1]:.1f} Z" fill="{colors[i % len(colors)]}"/>'
        )
    for i in range(n):
        a0 = 2 * math.pi * i / n - math.pi / 2
        a1 = 2 * math.pi * (i + 1) / n - math.pi / 2
        am = (a0 + a1) / 2
        p0 = (cx + ri * math.cos(a0), cy + ri * math.sin(a0))
        p1 = (cx + ri * math.cos(a1), cy + ri * math.sin(a1))
        p2 = (cx + r * 0.16 * math.cos(am), cy + r * 0.16 * math.sin(am))
        out.append(
            f'<path d="M{p0[0]:.1f} {p0[1]:.1f} L{p1[0]:.1f} {p1[1]:.1f} '
            f'L{p2[0]:.1f} {p2[1]:.1f} Z" fill="{colors[(i + 1) % len(colors)]}"/>'
        )
    out.append(circle(cx, cy, r * 0.16, colors[0]))
    out.append("</g>")
    return "".join(out)


def svg(inner, defs="", size=VIEW):
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" '
        f'viewBox="0 0 {VIEW} {VIEW}">{defs}{inner}</svg>'
    )


def rounded_rect(fill, radius=225, gradient=None):
    if gradient:
        return (
            f'<rect x="0" y="0" width="{VIEW}" height="{VIEW}" rx="{radius}" '
            f'fill="url(#{gradient})"/>'
        )
    return f'<rect x="0" y="0" width="{VIEW}" height="{VIEW}" rx="{radius}" fill="{fill}"/>'


def logo_paths(head_fill, body_fill, eye_fill, head_op=None, body_op=None,
               eye_op=None, stroke=None, stroke_w=0):
    """Compoe os tres paths oficiais numa paleta dada."""
    def op(v):
        return "" if v is None else f' opacity="{v}"'

    extra = ""
    if stroke:
        extra = f' stroke="{stroke}" stroke-width="{stroke_w}" stroke-linejoin="round"'

    return (
        f'<path d="{HEAD}" fill="{head_fill}"{op(head_op)}{extra}/>'
        f'<path d="{BODY}" fill="{body_fill}"{op(body_op)}{extra}/>'
        f'<path d="{EYE}" fill="{eye_fill}"{op(eye_op)}{extra}/>'
    )


# ------------------------------------------------------------------ variantes

def v1_oficial():
    """1. Oficial, cor plana (referencia)."""
    return svg(logo_paths("#ffffff", BLENDER_ORANGE, BLENDER_BLUE))


def v2_monocromo_escuro():
    """2. Silhueta monocromatica clara sobre fundo escuro."""
    inner = (
        f'<rect width="{VIEW}" height="{VIEW}" fill="#101418"/>'
        + logo_paths("#ffffff", "#ffffff", "#101418")
    )
    return svg(inner)


def v3_duotone():
    """3. Duotone azul/laranja sobre badge escuro arredondado."""
    inner = rounded_rect("#161b22", 225) + logo_paths(
        BLENDER_BLUE, BLENDER_ORANGE, "#f2f5f7"
    )
    return svg(inner)


def v4_gradiente():
    """4. Gradiente moderno laranja -> azul."""
    defs = (
        '<defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1">'
        '<stop offset="0" stop-color="#f8a03c"/>'
        f'<stop offset="0.55" stop-color="{BLENDER_ORANGE}"/>'
        f'<stop offset="1" stop-color="{BLENDER_BLUE}"/>'
        "</linearGradient></defs>"
    )
    inner = rounded_rect("", 225, gradient="g") + logo_paths(
        "#ffffff", "#ffffff", "#ffffff", head_op=0.92
    )
    return svg(inner, defs)


def v5_contorno():
    """5. Somente contorno (line-art)."""
    inner = logo_paths("none", "none", "none", stroke="#2b3a46", stroke_w=26)
    return svg(inner)


def v6_malha():
    """6. Cabeca em malha triangulada low-poly (foco em malha 3D)."""
    facets = facet_disc(
        HEAD_CX, HEAD_CY, HEAD_R,
        ["#3d6d99", "#2f5680", "#4b7fae", "#265787", "#5a8cba", "#37618c"],
    )
    inner = (
        facets
        + f'<path d="{BODY}" fill="{BLENDER_ORANGE}"/>'
        + f'<path d="{EYE}" fill="#12304d"/>'
    )
    return svg(inner)


def v7_neon():
    """7. Neon sobre fundo preto."""
    defs = (
        '<defs><filter id="glow" x="-40%" y="-40%" width="180%" height="180%">'
        '<feGaussianBlur stdDeviation="22" result="b"/>'
        '<feMerge><feMergeNode in="b"/><feMergeNode in="b"/>'
        '<feMergeNode in="SourceGraphic"/></feMerge></filter></defs>'
    )
    inner = (
        f'<rect width="{VIEW}" height="{VIEW}" fill="#07090c"/>'
        '<g filter="url(#glow)">'
        + logo_paths("#0b0e12", BLENDER_ORANGE, "#7fd8ff")
        + "</g>"
    )
    return svg(inner, defs)


def v8_badge_laranja():
    """8. Badge laranja polido, com o logo oficial contido e centralizado."""
    defs = (
        '<defs><linearGradient id="badge" x1="0" y1="0" x2="0.85" y2="1">'
        '<stop offset="0" stop-color="#f3942f"/>'
        '<stop offset="1" stop-color="#d96706"/>'
        '</linearGradient></defs>'
    )
    badge = (
        '<circle cx="512" cy="512" r="486" fill="url(#badge)"/>'
        '<circle cx="512" cy="512" r="462" fill="none" '
        'stroke="#ffffff" stroke-opacity="0.24" stroke-width="8"/>'
    )
    # The source logo is wider than the circular badge. Scale it around the
    # canvas center so all of its silhouette remains inside the badge.
    mark = (
        '<g transform="translate(112 112) scale(0.78)">'
        + logo_paths("#ffffff", "#ffffff", BLENDER_BLUE)
        + "</g>"
    )
    return svg(badge + mark, defs)


def v9_vidro():
    """9. Liquid glass: blobs coloridos desfocados atras do logo."""
    defs = (
        '<defs><filter id="blur" x="-50%" y="-50%" width="200%" height="200%">'
        '<feGaussianBlur stdDeviation="90"/></filter></defs>'
    )
    blobs = (
        '<g filter="url(#blur)" opacity="0.95">'
        + circle(300, 330, 240, "#f79a3d")
        + circle(760, 700, 260, "#2f5f92")
        + circle(700, 250, 170, "#7fc4f5")
        + "</g>"
    )
    glass = (
        '<rect x="60" y="60" width="904" height="904" rx="200" '
        'fill="#ffffff" opacity="0.16"/>'
    )
    inner = blobs + glass + logo_paths("#ffffff", "#ffffff", "#ffffff", head_op=0.98)
    return svg(inner, defs)


def v10_lowpoly():
    """10. Low-poly em tons de laranja."""
    facets = facet_disc(
        HEAD_CX, HEAD_CY, HEAD_R,
        ["#f2a154", "#e87d0d", "#f9bd7c", "#d96b06", "#f7a64d", "#ec8a2a"],
    )
    inner = (
        facets
        + f'<path d="{BODY}" fill="#c9610a"/>'
        + f'<path d="{EYE}" fill="#7a3a04"/>'
    )
    return svg(inner)


VARIANTS = [
    ("01-oficial-plano", "Oficial plano (referencia)", v1_oficial),
    ("02-monocromo-escuro", "Monocromo sobre fundo escuro", v2_monocromo_escuro),
    ("03-duotone", "Duotone azul/laranja em badge escuro", v3_duotone),
    ("04-gradiente", "Gradiente moderno", v4_gradiente),
    ("05-contorno", "Somente contorno (line-art)", v5_contorno),
    ("06-malha-lowpoly", "Cabeca em malha triangulada", v6_malha),
    ("07-neon", "Neon sobre preto", v7_neon),
    ("08-badge-laranja", "Badge circular laranja", v8_badge_laranja),
    ("09-liquid-glass", "Liquid glass", v9_vidro),
    ("10-lowpoly-laranja", "Low-poly em laranja", v10_lowpoly),
]


def main():
    from _render_svg import render

    for slug, title, fn in VARIANTS:
        outdir = os.path.join(HERE, slug)
        os.makedirs(outdir, exist_ok=True)
        svg_path = os.path.join(outdir, "icon.svg")
        with open(svg_path, "w", encoding="utf-8") as f:
            f.write(fn())
        render(svg_path, os.path.join(outdir, "icon-512.png"), 512)
        render(svg_path, os.path.join(outdir, "preview-128.png"), 128)
        print(f"ok  {slug}  ({title})")


if __name__ == "__main__":
    main()
