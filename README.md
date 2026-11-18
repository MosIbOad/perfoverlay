# perfoverlay

Lightweight, no-injection FPS and hardware overlay for Windows. Shows FPS, 1% / 0.1% lows, GPU/CPU usage and temps, and upscaler info (DLSS/FSR/XeSS) in a single exe. Built with C++20 and CMake.

## Display Modes & Anti-Cheat Compatibility

- **Windowed & Borderless Windowed (Pencereli / Çerçevesiz Pencereli)**: Tam olarak desteklenir. Overlay, şeffaf, tıklama geçiren (click-through) harici bir Direct3D 11 penceresi olarak oyunun üzerinde görüntülenir.
- **Exclusive Fullscreen (Özel Tam Ekran)**: **Desteklenmez**. Windows DWM (Masaüstü Pencere Yöneticisi) mimarisi gereği, oyun sürecine DLL enjeksiyonu veya Direct3D hook yapılmadan özel tam ekran uygulamaların üzerine harici pencere çizilemez. `perfoverlay` kullanırken oyun içi görüntü modunuzu **Borderless Windowed (Çerçevesiz Pencereli)** veya **Windowed (Pencereli)** olarak ayarlayınız.
- **Anti-Cheat Güvenliği**: Oyun sürecine ASLA DLL enjekte edilmez, API hook atılmaz ve oyunun Direct3D aygıtına dokunulmaz. FPS verisi harici ETW (PresentMon yaklaşımı) ile, donanım telemetrisi ise resmi API'lerle (AMD ADLX / Windows PDH) toplanır. Ban riski taşımaz.
- **Kısayol Tuşu**: **F10** tuşuna basarak overlay penceresini dilediğiniz zaman anında gizleyebilir veya tekrar gösterebilirsiniz.

## Upscaler & Frame Generation Detection

`perfoverlay`, aktif oyunun dosyalarında mevcut olan upscaler (DLSS, FSR, XeSS) ve Frame Generation kütüphanelerini tespit eder.

### Çalışma Prensibi & Anti-Cheat Güvenliği
1. Aktif oyun penceresinin çalıştırılabilir dosya yolu `QueryFullProcessImageName` ile alınır. Bu işlem için yalnızca en düşük yetki seviyesi olan `PROCESS_QUERY_LIMITED_INFORMATION` talep edilir; anti-cheat sistemlerini tetikleyebilecek `PROCESS_VM_READ` veya `EnumProcessModules` gibi bellek okuma çağrıları **kesinlikle kullanılmaz**.
2. Exe'nin bulunduğu klasör ve alt klasörleri derinlik sınırı ile taranır (`nvngx_dlss.dll`, `nvngx_dlssg.dll`, `amd_fidelityfx_*.dll`, `ffx_fsr2_api_*.dll`, `ffx_fsr3_api_*.dll`, `libxess.dll` vb.).
3. Her oyun için tarama sonucu önbelleğe alınır; kare başına veya sürekli dosya sistemi okuması yapılmaz.

### Bilinen Sınırlamalar
- **Anlık Aktiflik Durumu**: Overlay arayüzünde "Detected in game files" dürüst etiketi kullanılır. Oyun sürecine hook atılmadığı ve belleği okunmadığı için, ilgili teknolojinin (özellikle Frame Generation veya DLSS/FSR) oyun içi grafik ayarları menüsünden o an **aktif edilip edilmediği bu sürümde tespit edilmemektedir**. Yalnızca oyunun ilgili upscaler kütüphanelerini barındırdığı doğrulanır.

## Metrics & 1% / 0.1% Low Methodology

`perfoverlay` implements the **Average of the Worst 1% / 0.1%** (Expected Shortfall / 1% Average Low) method for reporting 1% Low and 0.1% Low FPS.

### Calculation Method

1. All frame times in the rolling observation window are sorted in ascending order.
2. The slowest `ceil(N * 0.01)` frames (for 1% low) and `ceil(N * 0.001)` frames (for 0.1% low) are isolated.
3. We compute the arithmetic mean of these worst frame times ($T_{\text{worst\_avg}}$).
4. The low FPS is calculated as:
   $$\text{Low FPS} = \frac{1000.0}{T_{\text{worst\_avg}}}$$

### Rationale: Average of Worst 1% vs. Quantile Threshold

There are two primary ways to define 1% Low in game performance analysis:

1. **Quantile Threshold (e.g., 99th Percentile Cutoff)**:
   - Takes only the single boundary value between the top 99% and bottom 1% frames ($1000 / T_{99}$).
   - **Drawback**: It is completely blind to the severity of frame pacing anomalies beyond the threshold. A mild 25 ms hiccup and a severe 500 ms stutter freeze produce identical 99th percentile numbers if their rank is the same.

2. **Average of the Worst 1% (Implemented in `perfoverlay`)**:
   - Computes the mean frame time of all frames in the worst 1% tail.
   - **Advantage**: Directly penalizes long and severe stutters proportionally to their actual duration.
   - **Industry Standard**: Widely adopted by leading benchmark authorities (Gamers Nexus, Digital Foundry, PresentMon) as the most faithful representation of player-perceived stutter and smoothness.
