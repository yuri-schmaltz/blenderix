# Handoff — Blender "mesh-only" fork (yuri-schmaltz/blender)

> Documento de retomada para o próximo agente/sessão.
> Repo: `/home/yuri/Documents/blender` — fork de **Blender 5.0.4 (BLENDER_VERSION 504)**,
> branch `main`, remote `origin` = `https://github.com/yuri-schmaltz/blender.git`.
> Última atualização: **2026-11-26**.

---

## 1. Objetivo

O fork é um **Blender enxuto para modelagem/manipulação de malhas 3D**. Tudo que não
serve a esse objetivo foi removido (implementação), mas o **DNA/RNA/enums** foram
mantidos para que arquivos `.blend` antigos continuem carregando e versionando.

Protocolo do projeto (regra de ouro):

> **Remove a implementação, mantém DNA/RNA/enum para compatibilidade de `.blend`.**
> Referência prática: commit `5d10e11434f` (remoção do editor Spreadsheet).

Marca do aplicativo: **BlenderiX** (logo, metainfo e ícone próprios — ver §7).

---

## 2. Estado atual (2026-11-26)

### Editores registrados no app (os que ainda funcionam)

`OUTLINER`, `VIEW_3D`, `PROPERTIES` (`space_buttons`), `FILE_BROWSER`, `INFO`,
`USERPREF`, `STATUSBAR`, `TOPBAR` — 8 editores, registrados em
`source/blender/editors/space_api/spacetypes.cc`.

### Diretórios de editores que **não são compilados** (restam só headers/sobras)

| Diretório | O que sobrou |
|---|---|
| `source/blender/editors/asset/` | headers `ED_asset_*.hh` (sem implementação) |
| `source/blender/editors/sculpt_paint/` | `paint_intern.hh`, `paint_cursor.hh` + subpastas `mesh/`, `curves/`, `grease_pencil/` |
| `source/blender/editors/space_graph/` | `ED_space_graph.hh` |
| `source/blender/editors/space_image/` | `image_intern.hh` |
| `source/blender/editors/space_sequencer/` | `sequencer_intern.hh`, `sequencer_scopes.hh` |
| `source/blender/editors/uvedit/` | `uvedit_*.hh` |

### Módulos "stub" (implementação já removida; só headers ficaram)

`source/blender/CMakeLists.txt` cria alvos `INTERFACE` (vazios) para:
`bf_animrig`, `bf_asset_system`, `bf_compositor`, `bf_freestyle`, `bf_functions`,
`bf_ikplugin`, `bf_nodes`, `bf_render`, `bf_sequencer`, `bf_shader_fx`,
`bf_simulation`.

### Superfície de UI (Python) que sobrou

- `scripts/startup/bl_ui/`: `space_view3d.py`, `space_view3d_toolbar.py`,
  `space_view3d_sidebar.py`, `space_topbar.py`, `space_properties.py`,
  `space_outliner.py`, `space_filebrowser.py`, `space_info.py`,
  `space_statusbar.py`, `space_userpref.py`, `space_project.py`,
  `properties_{object,scene,collection,data_mesh,data_empty,data_modifier}.py`,
  `anim.py` (só o que o Object/Modifiers usam).
- `scripts/startup/bl_operators/`: `add_mesh_torus.py`, `bmesh/`, `mesh.py`,
  `object*.py`, `file.py`, `view3d.py`, `wm.py`, `userpref.py`, `presets.py`.
- `scripts/presets/`: apenas `interface_theme/` (o tema) e `keyconfig/`.

### O que foi removido nas últimas sessões

| Commit | Removido |
|---|---|
| `9bcf9a17a2d` | Datablock `Text` + **todos os módulos não-mesh** restantes (~1,69M linhas, 9541 arquivos) |
| `fe7913ea3bc` | Abas do editor de Properties que não são de mesh |
| `cd0791d0ea6` | **Tema padrão** passa a ser o *Blender Themes 0023* |
| `8aeb1282183` | Exceção de LFS escopada para o zip do tema |
| `cd14925c0f5` | Assets do fork reorganizados (zip do tema → `themes/`, `.deb` fora da raiz) + este documento reescrito |
| `05eb7d2e20d` | **Build**: include paths de módulos que não existem mais (mantaflow, cycles, iksolver, alembic, usd, freestyle, rigidbody, slim, renderdoc, compositor, shader_fx, grease pencil, spz) — 16 arquivos, 75 linhas |
| `a9277a29f5a` | **Cleanup**: 189 headers órfãos dos módulos/editores já sem implementação (~15.270 linhas) |
| `f09abed04d2` | **Python**: presets de câmera/cloth/fluid/hair/tracking/render/sequencer/EEVEE/node/grease pencil + operadores e módulos de UI correspondentes (181 arquivos, 3.838 linhas). Sobram só `interface_theme` e `keyconfig` |
| `3f5731a04ba` | **UI**: seção *Animation* das preferências, gizmo de navegação do viewport, aba *Animation* da sidebar; opções de ferramenta e botões de tipo de seleção movidos para o header do 3D viewport |
| (em andamento) | Editor **Properties** enxuto (só Scene/Object/Modifiers/Data), `properties_workspace.py` removido, menu *Preferences* próprio no topo, `ANIM_MT_rotation_mode_convert` removido do painel de rotação |

### Tema padrão

- Preset: `scripts/presets/interface_theme/Blender_Themes_0023.xml`
- Fonte (zip): `themes/Blender Themes 0023.zip` + [`themes/README.md`](themes/README.md)
- Tema compilado: `release/datafiles/userdef/userdef_default_theme.c` (`U_theme_default`)
- Gerador: `tools/utils/blender_theme_as_c.py`
- As preferências salvas do usuário (`~/.config/blender/5.4/config/userpref.blend`) também
  foram atualizadas com o tema — sem isso o tema salvo sobrescreve o padrão.

---

## 3. O que ainda é "sobra" (próximos alvos)

1. **Código morto ainda compilado** (ligado, mas inalcançável):
   - `source/blender/editors/transform/transform_convert_sequencer*.cc`,
     `transform_convert_tracking*.cc`, `transform_snap_sequencer.cc`
     (operadores de transform de strip/movieclip).
   - `source/blender/editors/include/ED_clip.hh` e `ED_sequencer.hh` (headers stub).
   - Shaders GPU do sequencer: `source/blender/gpu/shaders/gpu_shader_sequencer_*.bsl.hh`
     (ainda listados em `source/blender/gpu/CMakeLists.txt` e em `gpu_shader_builtin.cc`).
2. **RNA de recursos removidos** (mantido de propósito, mas candidato à remoção):
   `rna_sequencer.cc`, `rna_sequencer_api.cc`, `rna_movieclip.cc`.
3. **Keymaps de editores inexistentes**: `scripts/presets/keyconfig/keymap_data/blender_default.py`
   ainda tem entradas `GRAPH_EDITOR`, `DOPESHEET_EDITOR`, `IMAGE_EDITOR`,
   `NODE_EDITOR`, `NLA_EDITOR`.
4. **Headers não utilizados** nos diretórios stub (§2) — a limpeza é repetível
   (é preciso um algoritmo *fixpoint*: remover só o header que ninguém inclui,
   nem include de outro header removido). O commit `a9277a29f5a` fez a primeira
   passada; ainda sobram os headers listados em §2.
5. **UI Python**: `scripts/startup/bl_ui/anim.py`, `properties_paint_common.py`,
   `properties_grease_pencil_common.py` e `properties_physics_geometry_nodes.py`
   continuam na árvore, mas servem a painéis que não existem mais — revisar e
   remover o que ficou órfão.
6. **Testes**: a suíte `tests/` foi removida no commit `9bcf9a17a2d`.
7. **Arquivo solto na raiz**: `icon.png` (1 MB) está na raiz do repo e não é
   referenciado por nenhum script — mover para `icons/` ou remover (a regra é
   não commitar binários avulsos na raiz).

---

## 4. Como compilar e validar

Build local em `/home/yuri/Documents/build` (fora do repo):

```bash
cmake -S . -B ../build -G Ninja \
  -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 \
  -DPYTHON_VERSION=3.13 -DWITH_LIBS_PRECOMPILED=OFF -DWITH_EMBREE=OFF \
  -DWITH_CYCLES=OFF -DWITH_CODEC_FFMPEG=OFF -DWITH_CODEC_SNDFILE=OFF \
  -DWITH_ALEMBIC=OFF -DWITH_USD=OFF -DWITH_MATERIALX=OFF -DWITH_OPENIMAGEDENOISE=OFF \
  -DWITH_INTERNATIONAL=OFF -DWITH_DRACO=OFF -DWITH_KTX=OFF
cd ../build && ninja blender && ninja install
```

- **Python ≥ 3.13** é obrigatório (`CMakeLists.txt` faz `FATAL_ERROR` abaixo disso).
- O binário fica em `../build/bin/blender`; os scripts instalados em `../build/bin/5.4/scripts`.
- O link usa `-Wl,--unresolved-symbols=ignore-all`
  (`source/creator/CMakeLists.txt`), por isso código removido pode continuar referenciado
  sem quebrar o link — **cuidado**: chamar um operador morto dá crash em runtime.
- Teste rápido de sanidade:
  ```bash
  ../build/bin/blender -b --factory-startup --python-expr \
    "import bpy; bpy.ops.mesh.primitive_cube_add(); bpy.ops.object.mode_set(mode='EDIT'); \
     bpy.ops.mesh.subdivide(); print(len(bpy.context.object.data.vertices))"
  ```

---

## 5. Regras/armadilhas

- **Git LFS em fork público**: o GitHub **não aceita novos objetos LFS** em forks
  (`can not upload new objects to public fork`). O `Blender Themes 0023.zip` e o
  `blenderix.icns` são blobs comuns por causa disso (exceções em `.gitattributes`).
- **Nunca** commitar `.deb`/binários soltos na raiz (já removido o
  `libegl-dev_1.7.0-1build1_amd64.deb`).
- **DNA**: `DNA_space_types.h` (`SpaceText`, `SpaceScript`, `SpaceConsole`),
  `DNA_space_enums.h`, `DNA_sequence_types.h`, `DNA_theme_types.h` — **manter**;
  o versioning legado depende deles.
- `dna_rename_defs.h` — usado na leitura de `.blend` antigos; não remover sem revisar.
- `source/blender/compositor` (framework) e `source/blender/nodes/geometry` — o
  Geometry Nodes ainda depende dos tipos declarados nesses headers; não apagar os
  headers em uso.
- Commits devem incluir o trailer:
  `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`.
- Push: o GitHub CLI não está autenticado; use o askpass do VS Code
  (`extensions/git/dist/askpass.sh` + `VSCODE_GIT_IPC_HANDLE` do terminal) ou `gh auth login`.
- PR real vai para `projects.blender.org`; o GitHub é espelho
  (`.github/pull_request_template.md` existe).

---

## 6. Comandos úteis

```bash
cd /home/yuri/Documents/blender
git --no-pager log --oneline -10
git --no-pager status
git --no-pager diff
```

Auditorias rápidas:

```bash
# headers incluídos por ninguém (candidatos a remoção)
git grep -h -o -E '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"][^">]+[>"]' HEAD \
  -- '*.cc' '*.hh' '*.h' | sed -E 's/.*[<"]([^">]+)[>"].*/\1/' | sort -u

# módulos com implementação já removida
grep -n "add_library(\${_stub_target} INTERFACE)" -A20 source/blender/CMakeLists.txt
```

---

## 7. Marca e assets do BlenderiX

- Ícone/metainfo do app: `release/datafiles/icons_svg/blenderix.svg`,
  `release/freedesktop/org.blenderix.BlenderiX.metainfo.xml`,
  `release/freedesktop/blender.desktop`,
  `release/darwin/Blender.app/Contents/Resources/blenderix.icns`,
  `release/windows/icons/winblender.ico`.
- Logo em SVG (ícone do Blender reescrito como "BlenderiX"):
  `release/darwin/blender_liquid_glass.icon/Assets/Blender Logo.svg`.
- **10 variantes de ícone** derivadas do logo oficial, geradas por script:
  [`icons/options/README.md`](icons/options/README.md) (`icons/options/<slug>/icon.svg`,
  `icon-512.png`, `preview-128.png` + folha de contato).
  Regerar com `python3 icons/options/generate_icons.py` e
  `python3 icons/options/_contact_sheet.py` (rasterização via `librsvg`/`cairo`, sem pip).
- Tema padrão do app: seção 2.
