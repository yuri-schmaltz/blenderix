# Handoff — Blender "mesh-only" fork (yuri-schmaltz/blenderix)

> Documento de retomada para o próximo agente/sessão.
> Repo: `/home/yuri/Documents/blenderix` — BlenderiX **1.0**, versão interna **5.4.0 alpha**,
> branch `main`, remote `origin` = `https://github.com/yuri-schmaltz/blenderix.git`.
> Última atualização: **2026-11-26**.

---

## 1. Objetivo

O fork é um **Blender enxuto para modelagem/manipulação de malhas 3D**. Tudo que não
serve a esse objetivo foi removido (implementação), mas o **DNA/RNA/enums** foram
mantidos. A leitura executa migrações de malha e de propriedades para arquivos
legados. A migração legada geral continua removida.

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

Use o [workflow Linux](.github/workflows/build.yml) como referência de dependências e
opções. Ele usa Ubuntu 26.04, GCC 14 e Python 3.14. Em um clone limpo, com
Git LFS e as dependências do workflow instaladas, prepare os recursos e as
versões de Eigen e OpenImageIO declaradas no código. Os assets são obtidos
do remoto do BlenderiX, com o servidor do Blender como alternativa.

```bash
build_files/ci/restore_lfs.sh
eigen_prefix="$(mktemp -d)"
cmake -DEIGEN_PREFIX="$eigen_prefix" -P build_files/ci/install_eigen.cmake
oiio_prefix="$(mktemp -d)"
cmake -DOIIO_PREFIX="$oiio_prefix" -P build_files/ci/install_openimageio.cmake
```

Use as opções do passo **Configure CMake (mesh-only)**, com
`Eigen3_DIR=$eigen_prefix/share/eigen3/cmake` e
`OpenImageIO_DIR=$oiio_prefix/lib/cmake/OpenImageIO`. Depois, compile e valide a
instalação.

```bash
cmake --build ../build --target bf_intern_dualcon --parallel
cmake --build ../build --parallel
install_dir="$(mktemp -d)"
cmake --install ../build --prefix "$install_dir"
mkdir -p "$install_dir/lib"
cp -a "$oiio_prefix/lib/"libOpenImageIO*.so* "$install_dir/lib/"
build_files/ci/test_installed.sh "$install_dir/blender"
```

O artefato do CI contém a instalação em um `.tar.gz`, que preserva a
permissão de execução. Esse pacote usa as bibliotecas de sistema do Ubuntu
26.04, inclusive a biblioteca compartilhada do Python 3.14. A
[prévia Linux v1.0.0-preview.1](https://github.com/yuri-schmaltz/blenderix/releases/tag/v1.0.0-preview.1)
foi publicada a partir do commit `feaa4ac` e testada em uma instalação mínima
do Ubuntu 26.04 com as dependências de execução descritas na release.

O CI também roda `build_files/ci/test_legacy.sh "$install_dir/blender"` com as
amostras versionadas em `build_files/ci/fixtures/`. Para verificar outra versão
anterior ou regenerar as amostras, use o Blender da versão correspondente.
Cada caso é verificado em um processo separado do BlenderiX instalado.

```bash
fixtures="$(mktemp -d)"
"$upstream_blender" -b --factory-startup --python-exit-code 1 \
  --python build_files/ci/legacy_blend.py -- "$fixtures"
for case in mesh modifier linked mixed; do
  "$install_dir/blender" -b --factory-startup --python-exit-code 1 \
    --python build_files/ci/legacy_blend.py -- "$fixtures" --case "$case"
done
```

A matriz passou no BlenderiX com amostras provenientes do Blender 3.6.0 e
4.5.0. Cada versão passou pelos casos de malha, Bevel, biblioteca vinculada e
cena mista. O caso misto preservou a malha ao abrir, salvar e reabrir
e descartou a câmera e a animação. A amostra do Blender 3.6 com Auto Smooth
também passou. A migração grava as arestas sharp na malha original, de modo
que o ângulo fica aplicado à malha base e não é recalculado sobre modificadores
posteriores. Esses resultados se limitam às amostras.

O teste gráfico precisa de uma sessão de desktop ou de um servidor X virtual.
Ele redesenha o viewport, edita uma malha, aplica um modificador, verifica
importação e exportação de OBJ, STL e PLY, e reabre o arquivo salvo em outro
processo.

```bash
export BLENDERIX_GUI_DIR="$(mktemp -d)"
"$install_dir/blender" --factory-startup --python-exit-code 1 \
  --python build_files/ci/graphical_acceptance.py
BLENDERIX_GUI_REOPEN=1 "$install_dir/blender" --factory-startup \
  "$BLENDERIX_GUI_DIR/acceptance.blend" --python-exit-code 1 \
  --python build_files/ci/graphical_acceptance.py
```

A aceitação gráfica no CI usa `--debug-memory` e falha se o aplicativo
registrar blocos não liberados no encerramento. Após restaurar a liberação
dos shaders e dos motores de viewport, os testes de edição e reabertura
terminaram sem esse aviso no ambiente de validação local.

O link ainda usa `-Wl,--unresolved-symbols=ignore-all` em
`source/creator/CMakeLists.txt`. Referências a implementações removidas
podem ser identificadas configurando um segundo diretório de build com as
mesmas opções e `-DWITH_STRICT_BLENDER_LINK=ON`. A primeira execução identificou
1.883 símbolos indefinidos distintos, principalmente em código de subsistemas
removidos. A restauração da biblioteca interna de funções do Blender
(`0cb6498d8a0c4b965ab0f5c55830034594c66812`), com headers idênticos aos já
presentes no fork, eliminou as referências indefinidas a `blender::fn`. O
diagnóstico do commit `feaa4ac` encontrou 1.836 símbolos indefinidos distintos
no build estrito usual e 1.777 com descarte de seções não referenciadas. Esse
segundo resultado usa `-ffunction-sections -fdata-sections` em C e C++ e
`-Wl,--gc-sections` no linker. O CMake com `WITH_GTESTS=ON` gerou alvos incompletos e o CTest
listou zero testes.

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
cd /home/yuri/Documents/blenderix
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
