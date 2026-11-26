# Opções de ícone do fork (derivadas do logo oficial do Blender)

10 variantes de ícone geradas a partir da geometria oficial do Blender
(`release/darwin/blender_liquid_glass.icon/Assets/Blender Logo.svg`, viewBox
0 0 1024 1024 — três paths: cabeça/olho branco, corpo laranja `#e87d0d`, olho
azul `#265787`).

Cada variante vive em uma pasta própria:

```
icons/options/<slug>/icon.svg        # vetor 1024x1024 (fonte)
icons/options/<slug>/icon-512.png    # raster 512x512
icons/options/<slug>/preview-128.png  # raster 128x128
icons/options/contact-sheet.png       # folha de contato com as 10
```

| # | Slug | Descrição |
|---|------|-----------|
| 01 | `01-oficial-plano` | Logo oficial, cor plana (referência / baseline) |
| 02 | `02-monocromo-escuro` | Silhueta monocromática branca sobre fundo escuro (#101418) |
| 03 | `03-duotone` | Cabeça azul + corpo laranja sobre badge escuro arredondado |
| 04 | `04-gradiente` | Badge com gradiente laranja → azul, logo branco por cima |
| 05 | `05-contorno` | Line-art: só contornos, traço 26px |
| 06 | `06-malha-lowpoly` | Cabeça substituída por malha triangular (azul) — foco em malha 3D |
| 07 | `07-neon` | Laranja/ciano neon com filtro de brilho sobre preto |
| 08 | `08-badge-laranja` | Badge circular laranja em gradiente, anel interno sutil e logo oficial branco/azul centralizado |
| 09 | `09-liquid-glass` | Blobs coloridos desfocados + camada de vidro, logo branco |
| 10 | `10-lowpoly-laranja` | Cabeça facetada em tons de laranja |

## Regenerar

```sh
cd icons/options
python3 generate_icons.py    # SVGs + PNGs
python3 _contact_sheet.py   # contact-sheet.png
```

O rasterizador (`_render_svg.py`) usa `librsvg` + `cairo` via `ctypes`, sem
dependências de pip. Ajuste as cores ou formas em `generate_icons.py` e rode de
novo.

## Como aplicar no sistema

Escolha uma variante e substitua o arquivo correspondente do build:

- Linux: `release/freedesktop/icons/scalable/apps/blender.svg`
  (e `release/freedesktop/icons/symbolic/apps/blender-symbolic.svg` para o ícone
  monocromático do menu).
- macOS: `release/darwin/blender_liquid_glass.icon/` (`.icon` bundle + PNGs).
- Windows: `release/windows/icons/` (`.ico` / PNGs gerados a partir do SVG).

Depois de substituir, regenere os recursos de ícone do build para que o CMake
copie o novo arquivo.
