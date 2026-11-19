# Need for Speed: Underground 2 para Android
[English](README.md) | [Bahasa Indonesia](README.id.md) | [Español](README.es.md) | **Português**

Um port nativo e não oficial para Android da versão original de Xbox de *Need for Speed: Underground 2*, com controles por toque, suporte a controle e teclado, e opções gráficas ampliadas.

O código x86 do jogo é recompilado estaticamente para C++ antecipadamente com o [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) e depois compilado para arm64. A renderização passa pelo Vulkan, e a janela, a entrada e o áudio pelo SDL3. Nenhum emulador é usado. O host Android (renderizador Vulkan, DirectSound, controles por toque, painel de configurações) vem do [MostWanted-Android](https://github.com/RanggaCasper/MostWanted-Android), que por sua vez o traz do [Black-Android](https://github.com/RanggaCasper/Black-Android).

> [!NOTE]
> Este projeto não inclui nenhum arquivo do jogo. Você precisa fornecer os arquivos da sua própria cópia do jogo, adquirida legalmente, para instalar ou compilar.

> [!WARNING]
> Trabalho em estágio inicial. Veja [Status](#status).

## Instalação
1. Instale o APK.
2. Abra o **Underground 2** e permita o **acesso a todos os arquivos** quando solicitado (no Android 10, a permissão de **armazenamento**). O jogo não inicia sem ela.
3. Copie o jogo para `/storage/emulated/0/Underground2`, ou para uma pasta `Underground2` na raiz de um cartão SD: a imagem do disco (`.iso`), ou o `default.xbe` com a pasta `NFSUNDER` do disco (e `B3`) ao lado. O app cria a pasta no armazenamento do celular assim que o acesso é concedido, e inicia o jogo assim que encontra os arquivos.

Os saves ficam em `/storage/emulated/0/Underground2/hdd` e os logs em `/storage/emulated/0/Underground2/logs`. As configurações (`underground2.toml`, `android.toml`, `layout.toml`) e o cache de shaders ficam em `Android/data/com.racas.underground2/files`.

O disco suportado é a versão dos EUA (título `4541005A`, `default.xbe` SHA-1 `428b5a70b9400f19cc5be7679db6c72a6f80a1c9`, 4.038.656 bytes, XDK 5849).

## Recursos
O painel de configurações abre pelo ícone de engrenagem no canto superior esquerdo da tela (ou F10 no teclado). O botão Voltar do Android corresponde ao botão Start do controle.

- **Gráficos**: resolução de renderização (20% a 200% da altura da tela), proporção de tela, V-Sync, limite de quadros, contador de FPS, pré-carregamento de shaders, filtragem anisotrópica, anti-aliasing (FXAA, SSAA 2x), bloom, neblina e filtros de tela.
- **Controles por toque**: um controle de Xbox completo na tela, com layouts, opacidade, tamanho, direção por inclinação e um editor de posições.
- **Controle**: qualquer controle suportado pelo SDL3, com vibração e remapeamento. **Teclado**, totalmente remapeável.
- **Geral**: saída de áudio (automática, mono, estéreo, 5.1), pausa em segundo plano, idioma do painel de configurações.
- **Minhas músicas**: suas próprias músicas de `/storage/emulated/0/Underground2/music` (MP3, M4A/AAC, OGG, Opus, FLAC, WAV).

## Status
A imagem recompilada e o host estão completos e compilam; o trabalho específico do jogo ainda está por fazer.

- O `xbe_symbols` encontra 173 das 238 funções do XDK que o host conhece e todos os 12 símbolos de dados. As demais não estão linkadas nesta imagem. Outras onze são resolvidas manualmente em `config/game.toml` (`D3DDevice_Reset`, `SetPalette`, `BeginPush`/`EndPush`, `LoadVertexShaderProgram`, `IsBusy`, o teste de visibilidade, `IDirectSoundBuffer_Pause`/`SetEG`), cada uma conferida no código, além de `D3D_FvfShader`.
- O recompilador cobre 20.797 funções (1.047.117 instruções, 497 tabelas de salto).
- Os patches do Most Wanted (configurações dentro do menu OPTIONS do jogo, opções de qualidade de renderização, cheats, carros adicionais, pacotes de texturas, replay, câmera cinematográfica, menus por toque, editor de HUD, RESET, a fonte do jogo no painel, travas do sistema de arquivos para celulares multinúcleo) usavam os endereços daquele jogo e foram removidos. Cada um precisa ser encontrado novamente no código do Underground 2 antes de poder voltar.
- Ainda não testado em um dispositivo.

Os logs ficam em `/storage/emulated/0/Underground2/logs`. Inclua o `underground2.log`, e o arquivo de crash se houver, ao relatar um problema.

## Requisitos do sistema
- **Android** 10 ou superior, arm64.
- **GPU** com driver Vulkan 1.1, como Adreno 6xx ou superior, Mali-G7x ou superior, ou Xclipse.
- **Memória**: 4 GB.
- **Armazenamento**: 4 GB para os arquivos do jogo e os saves.

## Compilação
A compilação requer um PC com Windows.

1. Clone este repositório.
2. Coloque o `default.xbe` em `assets/`, ou a imagem do disco (`.iso`) em `assets/` ou `iso/`. A compilação extrai o `default.xbe` dela.
3. Instale o [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) com *Desenvolvimento para desktop com C++*, o [Android Studio](https://developer.android.com/studio) com Android SDK Platform 34, NDK `28.2.13676358` e CMake `3.31.6`, Java 17 a 21 (`JAVA_HOME`), e o [Python](https://www.python.org/).
4. Execute `build.bat`. Ele compila o recompilador, recompila o xbe em `generated/game` e depois gera `app/build/outputs/apk/release/app-release.apk` com o Gradle. A primeira compilação compila o jogo inteiro para arm64 e leva de 30 a 90 minutos.

Instale com:
```
adb install -r app/build/outputs/apk/release/app-release.apk
```

## Créditos
- [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp) de TGP482: o recompilador estático e o runtime portátil (kernel do Xbox, bibliotecas do XDK e Direct3D 8) sobre os quais este port é construído.
- [MostWanted-Android](https://github.com/RanggaCasper/MostWanted-Android), [Black-Android](https://github.com/RanggaCasper/Black-Android) e [ShaolinMonks-Android](https://github.com/RanggaCasper/ShaolinMonks-Android) de RanggaCasper: o host Android sobre o qual este port é construído.
- [bagasromadon](https://github.com/bagasromadon): design dos botões.
- [SDL](https://github.com/libsdl-org/SDL), [glslang](https://github.com/KhronosGroup/glslang), [toml++](https://github.com/marzer/tomlplusplus) e [Dear ImGui](https://github.com/ocornut/imgui).
- [xemu](https://github.com/xemu-project/xemu) e [Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded), que o XboxXBE-Recomp usou como referência do comportamento do hardware e da API do Xbox.

## Licença
Este projeto é licenciado sob a [Licença Pública Geral GNU v3.0](LICENSE). O SDK em `sdk/` mantém sua própria licença MIT (`sdk/LICENSE`), e as bibliotecas acima as suas próprias.

*Need for Speed* é uma marca registrada da Electronic Arts Inc. Este projeto não é afiliado nem endossado por ela.
