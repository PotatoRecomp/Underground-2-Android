# Need for Speed: Underground 2 untuk Android
[English](README.md) | **Bahasa Indonesia** | [Español](README.es.md) | [Português](README.pt.md)

Port Android native tidak resmi dari *Need for Speed: Underground 2* versi Xbox orisinal, dengan kontrol sentuh, dukungan controller dan keyboard, serta opsi grafis tambahan.

Kode x86 game ini direkompilasi secara statis ke C++ dengan [XboxXBE-Recomp](https://github.com/TGP482/XboxXBE-Recomp), lalu dibangun untuk arm64. Gambar ditampilkan lewat Vulkan, sedangkan jendela, input, dan audio lewat SDL3. Tidak ada emulator yang dipakai. Host Android-nya (renderer Vulkan, DirectSound, kontrol sentuh, panel pengaturan) berasal dari [MostWanted-Android](https://github.com/RanggaCasper/MostWanted-Android).

> [!NOTE]
> Proyek ini tidak menyertakan aset game apa pun. Kamu harus menyediakan file dari salinan game milikmu sendiri yang didapat secara legal.

> [!WARNING]
> Masih tahap awal pengembangan. Lihat [Status](#status).

## Instalasi
1. Pasang APK-nya.
2. Buka **Underground 2** dan izinkan **akses semua file** saat diminta (di Android 10, izin **penyimpanan**).
3. Salin game ke `/storage/emulated/0/Underground2`, atau ke folder `Underground2` di bagian paling atas kartu SD: bisa berupa image disc (`.iso`), atau `default.xbe` dengan folder `NFSUNDER` (dan `B3`) dari disc di sebelahnya.

Save disimpan di `/storage/emulated/0/Underground2/hdd` dan log di `/storage/emulated/0/Underground2/logs`. Pengaturan (`underground2.toml`, `android.toml`, `layout.toml`) dan cache shader ada di `Android/data/com.racas.underground2/files`.

Disc yang didukung adalah rilis USA (title `4541005A`, `default.xbe` SHA-1 `428b5a70b9400f19cc5be7679db6c72a6f80a1c9`).

## Fitur
Panel pengaturan dibuka dari ikon roda gigi di kiri atas layar (atau F10). Tombol kembali Android berfungsi sebagai tombol Start.

- **Grafis**: render resolution, aspect ratio, V-Sync, max frame rate, FPS counter, shader preloading, anisotropic filtering, anti-aliasing (FXAA, SSAA 2x), bloom, fog, dan screen filter.
- **Kontrol sentuh**: controller Xbox lengkap di layar, dengan tata letak, transparansi, ukuran, setir miring (gyro), dan editor posisi.
- **Controller** apa pun yang didukung SDL3, dengan getaran dan pengaturan ulang tombol. **Keyboard** bisa diatur ulang.
- **My Music**: lagu sendiri dari `/storage/emulated/0/Underground2/music`.

### Menggunakan custom music (Android)
1. Salin lagu ke `/storage/emulated/0/Underground2/music`; subfolder juga dibaca.
2. Buka ikon roda gigi → **MY MUSIC**, lalu pilih **My Music** sebagai sumber.
3. Atur volume melalui slider musik menu dan musik balapan di **Audio game**. **My Music** membisukan bus EA Trax otomatis; pilih **EA Trax** untuk mengembalikan musik bawaan.
4. Ketuk lagu untuk memutarnya. Gunakan **Next**, **Random/Sequential**, dan mode **Menus/Racing/All/Off** sesuai kebutuhan.
5. Setelah menambah atau menghapus lagu, tekan **Rescan songs**. Folder kosong juga dipindai otomatis setiap 5 detik.

Format yang dicari: MP3, M4A, AAC, OGG/OGA, Opus, FLAC, dan WAV; kemampuan decode mengikuti codec perangkat Android.
Urutan dan mode lagu disimpan dalam `music/playlist.txt`. Nama `Artis - Judul.mp3` ditampilkan sebagai artis dan judul.

## Status
Image hasil rekompilasi dan host-nya sudah lengkap dan bisa di-build; pekerjaan khusus game masih harus dilakukan.

- `xbe_symbols` menemukan 173 dari 238 fungsi XDK dan semua 12 simbol data. Sebelas lagi diselesaikan manual di `config/game.toml`, masing-masing dicek dengan kodenya.
- Patch khusus Most Wanted (pengaturan di menu OPTIONS game, switch kualitas, cheat, mobil tambahan, texture pack, replay, kamera sinematik, menu bisa disentuh, editor HUD, RESET, font game, kunci file system untuk HP multi-core) memakai alamat milik game itu dan sudah dihapus. Masing-masing harus ditemukan lagi di kode Underground 2 sebelum bisa dikembalikan.
- Belum diuji di perangkat.

## Membangun
Butuh PC Windows: Visual Studio 2022 (*Desktop development with C++*), Android SDK Platform 34, NDK `28.2.13676358`, CMake `3.31.6`, Java 17–21, dan Python. Taruh `default.xbe` atau image disc (`.iso`) di `assets/` atau `iso/`, lalu jalankan `build.bat`. Hasilnya `app/build/outputs/apk/release/app-release.apk`. Build pertama memakan waktu 30 sampai 90 menit.

## Lisensi
[GNU General Public License v3.0](LICENSE). SDK di `sdk/` memakai lisensi MIT-nya sendiri.

*Need for Speed* adalah merek dagang Electronic Arts Inc. Proyek ini tidak berafiliasi dengan maupun didukung oleh mereka.
