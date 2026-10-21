# Legacy sketch: clorela_os_v14

Source: [walcorg/clorela_os_v14](https://github.com/walcorg/clorela_os_v14) (1 commit, marked unstable).

## What it was

A single Qt Widgets window (qmake, `QMainWindow` + Qt Designer `.ui`, 800 × 600) whose only goal was the navigation menus. No window behind any menu item was implemented.

## Menu tree found in `clorela_os_v14.ui`

| Menu | Items |
|---|---|
| CUENTA (Account) | Iniciar sesión · Mi perfil ▸ (Datos de usuario, Address wallet) · Iniciar carga (F1) · Detener carga (F5) · Cerrar sesión |
| VISTA (View) | UI ▸ (Gráfica, Numérica) · Idioma ▸ (Inglés, Español, Francés) |
| CONFIGURAR (Configure) | Configurar WiFi · Configurar server |
| OPCIONES (Options) | Buscar actualizaciones · Propiedades de conexión |
| AYUDA (Help) | Acerca de Clorela · Acerca de WALCORG · Reportar un problema |

The tree is carried over unchanged into `src/ui/mainwindow.ui`.

## Problems fixed in the port

| v14 | Clorela-OS |
|---|---|
| Build outputs committed (`build-clorela_os_v14-Desktop-Debug/`: Makefile, `.o`, binary, `moc_*`, `ui_*.h`) | Ignored by `.gitignore`; out-of-source CMake builds |
| Qt Creator user file committed (`clorela_os_v14.pro.user`) | Ignored |
| qmake `.pro` | CMake (`qt_standard_project_setup`), which is what the Yocto `qt6-cmake` class expects |
| "F1" / "F5" only written in the menu text, no real shortcut | Real `QAction` shortcuts |
| WiFi item named `actionCONFIGURAR_SERVER`, server item `actionCONFIGURAR_SERVER_2` | `actionConfigureWifi`, `actionConfigureServer` |
| Typo "ADDRESS WALLETT" | "Wallet address" |
| Spanish UI strings hard-coded in the `.ui` | English source strings + `.ts` translations (ES, EN, FR), switchable at runtime |
| Fixed 800 × 600 window | Full screen on the node (`--fullscreen` or `ui/fullscreen=true`) |

## Not carried over

Nothing functional existed. The 2021 image (`core-image-sato`, "4th generation node") and the "Chorella OS" name are legacy and are not used.
