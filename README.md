# FrameInterp — Hafif GPU Çoklu Kare Artırıcı (x2 / x3 / x4)

Oyunu **DXGI ile VRAM'de yakalar**, **HLSL compute shader** (Lucas-Kanade optical flow + motion warping) ile
ara kareler üretir ve tıklama geçirgen, her zaman üstte bir DX11 overlay'e basar.
Intel iGPU / AMD / NVIDIA (DirectX 11) üzerinde çalışır. CPU↔GPU kopyası yok.

## Kullanıcılar için (kurulum yok)
1. [Releases](../../releases) sayfasından **`FrameInterp.exe`** dosyasını indirin. (Tek dosya; C++, Visual C++ Redistributable, shaders klasörü vb. gerekmez.)
2. Oyunu **Kenarlıksız Pencere (Borderless Windowed)** modunda açın.
3. `FrameInterp.exe`'ye çift tıklayın. Sağ alttaki sistem tepsisinde simge çıkar.

| Kısayol | İşlev |
|---|---|
| Ctrl+Alt+M | Çarpan: x2 → x3 → x4 |
| Ctrl+Alt+F | Aç / kapat |
| Ctrl+Alt+Q | Çıkış |

Tepsi simgesine tıklayınca aynı seçenekler menü olarak da açılır.
Komut satırı (isteğe bağlı): `FrameInterp.exe --mult 3 --monitor 1`

> Windows "SmartScreen" ilk açılışta uyarı verebilir (exe imzasız): *Ek bilgi → Yine de çalıştır*.

### Doğru ayar
Ekranda gösterilen FPS = oyun FPS × çarpan. Oyunu **monitör Hz ÷ çarpan** değerine sabitleyin:
- 120 Hz monitör: x2 → oyun 60, x4 → oyun 30
- 144 Hz monitör: x3 → oyun 48
- 180 Hz monitör: x3 → oyun 60

## Nasıl ucuz?
- Akış 1/4 çözünürlükte hesaplanır ve **kaynak kare başına yalnızca bir kez** (çarpan kaç olursa olsun).
- x3/x4'te ek ara kareler sadece hafif bir warp geçişi ister.
- Gradyan ve yapı tensörü iterasyon dışına alındı; iterasyonlarda piksel başına sadece 3 doku okuması var.

## Geliştiriciler için
Kendiniz derlemek zorunda değilsiniz: bu repoyu GitHub'a yükleyin → **Actions** exe'yi otomatik derler.
`v1.0` gibi bir etiket (tag) atarsanız exe **Releases**'e otomatik eklenir.

Elle: `build.bat` (Visual Studio 2019/2022 + CMake) → `dist\FrameInterp.exe`.
Shader'lar (`shaders/*.hlsl`) derleme sırasında exe'nin içine gömülür; ayrı klasör gerekmez.

## Sınırlar
- Windows 10 2004+ gerekir (overlay'in kendini yakalamasını `WDA_EXCLUDEFROMCAPTURE` engeller).
- Exclusive Fullscreen yakalanamaz → Borderless kullanın.
- Bir kaynak kare gecikmesi eklenir (A→B arası kareler için B beklenir).
- Oyun motoru hareket vektörleri kullanılmaz (ekran görüntüsünden akış çıkarılır): HUD/yazılar ve çok hızlı hareketlerde artefakt olabilir; çarpan arttıkça artar. Hatalı akış düz harmanlamaya düşer.
- Anti-cheat'li oyunlarda kullanım riski size aittir.

Lisans: MIT
