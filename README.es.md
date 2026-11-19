# Need for Speed: Underground 2 para Android
[English](README.md) | [Bahasa Indonesia](README.id.md) | **Español** | [Português](README.pt.md)

Un port nativo y no oficial para Android de la versión original de Xbox de *Need for Speed: Underground 2*, con controles táctiles, soporte para mando y teclado, y opciones gráficas ampliadas.

El código x86 del juego se recompila estáticamente a C++ por adelantado con [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) y luego se compila para arm64. El dibujado va por Vulkan, y la ventana, la entrada y el audio por SDL3. No se usa ningún emulador. El host de Android (renderizador Vulkan, DirectSound, controles táctiles, panel de ajustes) viene de [MostWanted-Android](https://github.com/RanggaCasper/MostWanted-Android), que a su vez lo trae de [Black-Android](https://github.com/RanggaCasper/Black-Android).

> [!NOTE]
> Este proyecto no incluye ningún recurso del juego. Debes aportar los archivos de tu propia copia del juego, obtenida legalmente, para instalarlo o compilarlo.

> [!WARNING]
> Trabajo en una fase temprana. Consulta [Estado](#estado).

## Instalación
1. Instala el APK.
2. Abre **Underground 2** y permite el **acceso a todos los archivos** cuando se te pida (en Android 10, el permiso de **almacenamiento**). El juego no puede iniciarse sin él.
3. Copia el juego en `/storage/emulated/0/Underground2`, o en una carpeta `Underground2` en la raíz de una tarjeta SD: ya sea la imagen del disco (`.iso`), o `default.xbe` con la carpeta `NFSUNDER` del disco (y `B3`) a su lado. La app crea la carpeta en el almacenamiento del teléfono una vez concedido el acceso, e inicia el juego en cuanto encuentra los archivos.

Las partidas guardadas están en `/storage/emulated/0/Underground2/hdd` y los registros en `/storage/emulated/0/Underground2/logs`. Los ajustes (`underground2.toml`, `android.toml`, `layout.toml`) y la caché de shaders están en `Android/data/com.racas.underground2/files`.

El disco compatible es la versión de EE. UU. (título `4541005A`, `default.xbe` SHA-1 `428b5a70b9400f19cc5be7679db6c72a6f80a1c9`, 4.038.656 bytes, XDK 5849).

## Características
El panel de ajustes se abre desde el icono del engranaje en la esquina superior izquierda de la pantalla (o con F10 en un teclado). El botón Atrás de Android equivale al botón Start del mando.

- **Gráficos**: resolución de renderizado (del 20% al 200% de la altura de la pantalla), relación de aspecto, V-Sync, límite de fotogramas, contador de FPS, precarga de shaders, filtrado anisotrópico, antialiasing (FXAA, SSAA 2x), bloom, niebla y filtros de pantalla.
- **Controles táctiles**: un mando de Xbox completo en pantalla, con distribuciones, opacidad, tamaño, dirección por inclinación y un editor de posiciones.
- **Mando**: cualquier mando compatible con SDL3, con vibración y reasignación de botones. **Teclado**, totalmente reasignable.
- **General**: salida de audio (automática, mono, estéreo, 5.1), pausa en segundo plano, idioma del panel de ajustes.
- **Mi música**: tus propias canciones desde `/storage/emulated/0/Underground2/music` (MP3, M4A/AAC, OGG, Opus, FLAC, WAV).

## Estado
La imagen recompilada y el host están completos y compilan; el trabajo específico del juego aún está por hacer.

- `xbe_symbols` encuentra 173 de las 238 funciones del XDK que conoce el host y los 12 símbolos de datos. El resto no están enlazadas en esta imagen. Otras once se resuelven a mano en `config/game.toml` (`D3DDevice_Reset`, `SetPalette`, `BeginPush`/`EndPush`, `LoadVertexShaderProgram`, `IsBusy`, la prueba de visibilidad, `IDirectSoundBuffer_Pause`/`SetEG`), cada una verificada contra el código, además de `D3D_FvfShader`.
- El recompilador cubre 20.797 funciones (1.047.117 instrucciones, 497 tablas de saltos).
- Los parches de Most Wanted (ajustes dentro del menú OPTIONS del juego, opciones de calidad de renderizado, trucos, coches añadidos, paquetes de texturas, repeticiones, cámara cinemática, menús táctiles, editor del HUD, RESET, la fuente del juego en el panel, bloqueos del sistema de archivos para teléfonos multinúcleo) usaban las direcciones de ese juego y se han eliminado. Cada uno debe localizarse de nuevo en el código de Underground 2 antes de poder volver.
- Aún no se ha probado en un dispositivo.

Los registros están en `/storage/emulated/0/Underground2/logs`. Incluye `underground2.log`, y el archivo de fallo si lo hay, al informar de un problema.

## Requisitos del sistema
- **Android** 10 o posterior, arm64.
- **GPU** con controlador Vulkan 1.1, como Adreno 6xx o superior, Mali-G7x o superior, o Xclipse.
- **Memoria**: 4 GB.
- **Almacenamiento**: 4 GB para los archivos del juego y las partidas guardadas.

## Compilación
Para compilar se necesita un PC con Windows.

1. Clona este repositorio.
2. Coloca `default.xbe` en `assets/`, o la imagen del disco (`.iso`) en `assets/` o `iso/`. La compilación extrae `default.xbe` de ella.
3. Instala [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) con *Desarrollo para el escritorio con C++*, [Android Studio](https://developer.android.com/studio) con Android SDK Platform 34, NDK `28.2.13676358` y CMake `3.31.6`, Java 17 a 21 (`JAVA_HOME`), y [Python](https://www.python.org/).
4. Ejecuta `build.bat`. Compila el recompilador, recompila el xbe en `generated/game` y luego genera `app/build/outputs/apk/release/app-release.apk` con Gradle. La primera compilación compila todo el juego para arm64 y tarda entre 30 y 90 minutos.

Instálalo con:
```
adb install -r app/build/outputs/apk/release/app-release.apk
```

## Créditos
- [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) de TGP482: el recompilador estático y el runtime portable (kernel de Xbox, bibliotecas del XDK y Direct3D 8) sobre los que se basa este port.
- [MostWanted-Android](https://github.com/RanggaCasper/MostWanted-Android), [Black-Android](https://github.com/RanggaCasper/Black-Android) y [ShaolinMonks-Android](https://github.com/RanggaCasper/ShaolinMonks-Android) de RanggaCasper: el host de Android sobre el que se basa este port.
- [bagasromadon](https://github.com/bagasromadon): diseño de los botones.
- [SDL](https://github.com/libsdl-org/SDL), [glslang](https://github.com/KhronosGroup/glslang), [toml++](https://github.com/marzer/tomlplusplus) y [Dear ImGui](https://github.com/ocornut/imgui).
- [xemu](https://github.com/xemu-project/xemu) y [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded), que XboxXBE-Recomp usó como referencia del comportamiento del hardware y la API de Xbox.

## Licencia
Este proyecto está bajo la [Licencia Pública General de GNU v3.0](LICENSE). El SDK en `sdk/` mantiene su propia licencia MIT (`sdk/LICENSE`), y las bibliotecas anteriores las suyas.

*Need for Speed* es una marca comercial de Electronic Arts Inc. Este proyecto no está afiliado ni respaldado por ellos.
