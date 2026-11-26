# Temas

Este diretório guarda os arquivos-fonte dos temas usados pelo **BlenderiX**.

## `Blender Themes 0023.zip`

Arquivo original do tema "Blender Themes 0023" (Blender Themes Shop). É o mesmo
conteúdo que está no repositório como
`scripts/presets/interface_theme/Blender_Themes_0023.xml`, extraído do zip.

- **Uso no BlenderiX**: este é o **tema padrão** do aplicativo. O tema compilado
  vive em `release/datafiles/userdef/userdef_default_theme.c` (constante
  `U_theme_default`), gerado a partir do preset XML.
- **Instalação manual** (para usar em um Blender oficial): copie o XML para
  `<config>/blender/<versão>/scripts/presets/interface_theme/` e selecione-o em
  *Edit > Preferences > Interface > Themes*.
- **Regerar o tema padrão** depois de editar o XML:

  ```bash
  # aplica o preset e salva as preferências
  ./bin/blender --python-expr "
  import bpy
  bpy.ops.preferences.theme_install(filepath='scripts/presets/interface_theme/Blender_Themes_0023.xml')
  bpy.ops.wm.save_userpref()"

  # converte as preferências salvas no tema padrão compilado
  python3 tools/utils/blender_theme_as_c.py ~/.config/blender/5.4/config/userpref.blend
  ```

  Depois é só recompilar (`ninja blender` em `../build`).

## Por que o zip não usa Git LFS

O GitHub não permite enviar novos objetos LFS para forks públicos
(`can not upload new objects to public fork`). O arquivo é pequeno (5 KB) e por
isso é armazenado como blob comum — a exceção está registrada em
`.gitattributes`.
