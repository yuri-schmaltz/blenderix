<!--
Keep this document short & concise,
linking to external resources instead of including content in-line.
See 'release/text/readme.html' for the end user read-me.
-->

> [!IMPORTANT]
> Cloning from this [GitHub mirror](https://github.com/blender/blender) may cause Git LFS errors. To avoid this, use `GIT_LFS_SKIP_SMUDGE=1` when doing your initial clone.  
> See [the documentation](https://developer.blender.org/docs/handbook/contributing/using_git/clone_update/#github-mirror) for full instructions.

BlenderiX (fork "mesh-only")
============================

Este repositório é um **fork enxuto do Blender** (versão interna 5.4.0 alpha) com um único foco:
**modelagem e manipulação de malhas 3D**. Recursos fora desse escopo (física,
compositor, sequenciador, movie clip, animação, render etc.) foram removidos,
preservando estruturas DNA/RNA. A compatibilidade com `.blend` de versões anteriores ainda precisa ser validada.

- Documento de estado e próximos passos: [`HANDOFF.md`](../HANDOFF.md)
- Tema padrão do aplicativo: [`themes/README.md`](../themes/README.md)
  (*Blender Themes 0023*, em `release/datafiles/userdef/userdef_default_theme.c`)
- Variantes de ícone do app: [`icons/options/README.md`](../icons/options/README.md)
  (10 variantes geradas a partir do logo oficial do Blender)
- Layout do repositório: `source/` (C/C++), `scripts/` (Python), `release/`
  (dados que vão junto do app), `themes/` (temas), `icons/` (ícones do app),
  `tools/` (utilitários).

Escopo atual do aplicativo
--------------------------

- Editores disponíveis: **3D View**, Outliner, Properties (Scene, Object,
  Modifiers, Data), File Browser, Info, Preferences, Status Bar e Top Bar.
- Tudo o que é fora de modelagem de malhas foi removido do código, mas o
  DNA/RNA foi preservado para manter as estruturas necessárias à leitura de `.blend`. As migrações legadas gerais foram removidas.
- Presets Python restantes: apenas o tema da interface e o keymap.

Blender
=======

Blender is the free and open source 3D creation suite.
It supports the entirety of the 3D pipeline—modeling, rigging, animation, simulation, rendering, compositing,
motion tracking and video editing.

![Blender screenshot](https://code.blender.org/wp-content/uploads/2018/12/springrg.jpg "Blender screenshot")

Project Pages
-------------

- [Main Website](http://www.blender.org)
- [Reference Manual](https://docs.blender.org/manual/en/latest/index.html)
- [User Community](https://www.blender.org/community/)

Development
-----------

- [Build Instructions](https://developer.blender.org/docs/handbook/building_blender/)
- [Code Review & Bug Tracker](https://projects.blender.org)
- [Developer Forum](https://devtalk.blender.org)
- [Developer Documentation](https://developer.blender.org/docs/)


License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3.
Individual files may have a different but compatible license.

See [blender.org/about/license](https://www.blender.org/about/license) for details.
