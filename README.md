# perfoverlay

[![Build & Test](https://github.com/berkan/perfoverlay/actions/workflows/build.yml/badge.svg)](https://github.com/berkan/perfoverlay/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11%20x64-0078D6?logo=windows)](https://microsoft.com/windows)
[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)

**perfoverlay**, Windows için geliştirilmiş, tek çalıştırılabilir (`single-executable`), harici ve **kesinlikle oyun içine DLL enjekte etmeyen / API hook atmayan** modern bir FPS ve donanım izleme aracıdır (MSI Afterburner + RivaTuner Statistics Server alternatifi).

C++20 standardı, Direct3D 11, Dear ImGui ve Windows ETW (Event Tracing for Windows) mimarisi üzerine inşa edilmiştir.

---

## Ekran Görüntüsü (Preview)

```text
+-------------------------------------------------------------+
| [Waiting for game...]                                       |
| FPS: 142.5  |  FrameTime: 7.02 ms                           |
| 1% Low: 104.2 FPS  |  0.1% Low: 78.5 FPS                    |
| CPU: 24.3% (58°C)  |  GPU: 94.1% (64°C)                     |
| VRAM: 9840 / 16384 MB  |  Clock: 2450 MHz  |  Power: 215 W  |
| Upscaler: DLSS (FrameGen), FSR 3 [Detected in game files]   |
+-------------------------------------------------------------+
```
*(Görsel önizleme: Şeffaf, tıklama geçiren Direct3D 11 overlay penceresi oyunun üzerinde görünür)*

---

## Özellik Tablosu (Feature Matrix)

| Özellik / Metrik | Açıklama | Kaynak & Yöntem | Anti-Cheat Etkisi |
| :--- | :--- | :--- | :--- |
| **FPS & Frame Time** | Anlık kare hızı ve milisaniye cinsinden gecikme | ETW (Microsoft-Windows-DxgKrnl / DXGI) | **Sıfır Enjeksiyon** (Dışarıdan izleme) |
| **1% & 0.1% Low FPS** | En kötü %1 ve %0.1'lik dilimin ortalama FPS karşılığı | `FrameTimeBuffer` matematiksel kuyruk | Harici matematiksel hesaplama |
| **GPU Kullanımı & Sıcaklık** | GPU % yükü ve çekirdek sıcaklığı (°C) | AMD ADLX SDK (Yedek: Windows PDH) | Resmi donanım API'si |
| **VRAM, Saat Hızı & Güç** | Kullanılan/Toplam VRAM, MHz ve Watt | AMD ADLX SDK | Resmi donanım API'si |
| **CPU Kullanımı** | Toplam işlemci yükü (%) | Windows Performance Data Helper (PDH) | Resmi Windows API'si |
| **CPU Sıcaklığı** | İşlemci paket sıcaklığı (°C) | LibreHardwareMonitor Web Server (`localhost:8085`) | **Sürücüsüz** (Kernel driver yok) |
| **Upscaler Tespiti** | DLSS, DLSS-G, FSR ve XeSS varlığı | Pasif oyun dizini taraması | Yalnızca dosya sistemi okuması |
| **Overlay Penceresi** | Şeffaf, always-on-top, click-through UI | Win32 + D3D11 + DWM + ImGui | Oyun belleğine dokunmaz |
| **Sistem Tepsisi & Ayarlar** | Tepsi ikonu ve etkileşimli ayarlar penceresi | Windows Shell Notify + ImGui Dialog | Bağımsız Win32 penceresi |

---

## "Zero-Injection" ve Anti-Cheat Güvenliği

> [!IMPORTANT]
> **Dürüst Bilgilendirme**: `perfoverlay`, oyun süreçlerine kod enjekte etmeyerek ban riskini geleneksel araçlara (RTSS, SpecialK, ReShade vb.) kıyasla **dramatik şekilde azaltır**. Ancak, hiçbir üçüncü taraf yazılım çekirdek düzeyindeki (Ring-0 / Kernel) anti-cheat sistemlerine karşı **%100 mutlak garanti veremez**. Yazılımı kendi sorumluluğunuzda kullanınız.

### Neden Çok Daha Güvenli?
1. **DLL Enjeksiyonu YOK**: Oyunun adres alanına (Virtual Address Space) hiçbir yabancı DLL enjekte edilmez (`CreateRemoteThread`, `SetWindowsHookEx` veya DLL injection yöntemleri ASLA kullanılmaz).
2. **API Hooking YOK**: Oyunun çağırdığı `Present`, `Present1`, `ResizeBuffers` gibi DirectX veya Vulkan fonksiyon tablolarına (VTable / IAT) kanca takılmaz.
3. **ETW (Event Tracing for Windows) Yaklaşımı**: Kare süreleri, Intel PresentMon ve Windows Performance Analyzer'ın kullandığı resmi Windows çekirdek ETW olayları (`Microsoft-Windows-DxgKrnl` ve `Microsoft-Windows-DXGI`) dinlenerek tamamen süreç dışından (out-of-process) toplanır.
4. **En Düşük İşlem Yetkisi**: Aktif oyunun exe yolu taranırken yalnızca `PROCESS_QUERY_LIMITED_INFORMATION` talep edilir; bellek okuma (`PROCESS_VM_READ`) yetkisi istenmez.
5. **Kernel Sürücüsü Yüklemez**: Anti-cheat sistemlerinin ve Windows 11 Bellek Bütünlüğü'nün (HVCI) kara listeye aldığı güvensiz çekirdek sürücüleri (WinRing0 vb.) projede kesinlikle yer almaz.

---

## Kısayollar ve Kullanım

| İşlem | Yöntem |
| :--- | :--- |
| **Overlay Aç / Gizle** | Global `F10` kısayol tuşu |
| **Sistem Tepsisi Menüsü** | Görev çubuğundaki bildirim simgesine sağ tık: *Göster/Gizle*, *Ayarlar...*, *Ayarlar dosyasını aç*, *Çıkış* |
| **Etkileşimli Ayarlar** | Tepsi simgesinden "Ayarlar..." veya CLI: `perfoverlay.exe --settings` |
| **Konsol Teşhis Modu** | Komut isteminde: `perfoverlay.exe --debug` |
| **Penceresiz (Headless) Mod** | Otomasyon / testler için: `perfoverlay.exe --headless` |
| **Belirli Frame Sonra Çıkış** | `perfoverlay.exe -n 120` (120 frame sonra otomatik kapanır) |

---

## Bilinen Sınırlamalar

1. **Exclusive Fullscreen (Özel Tam Ekran) Desteklenmez**:
   - `perfoverlay`, oyunun grafik hattını kırmak yerine Windows Masaüstü Pencere Yöneticisi (DWM) kompozisyonunu kullanır.
   - Bu nedenle oyunlarınızı **Borderless Windowed (Çerçevesiz Pencereli)** veya **Windowed (Pencereli)** modunda çalıştırmanız gereklidir. Exclusive Fullscreen modundaki oyunlar DWM katmanını baypas ettiği için üzerlerine harici pencere çizilemez.
2. **Frame Generation ve Upscaler Anlık Durumu**:
   - Oyun klasöründeki kütüphaneleri tarayarak `DLSS`, `DLSS FrameGen`, `FSR` veya `XeSS` kütüphanelerinin varlığını doğrular.
   - Arayüzde **"Detected in game files"** etiketi gösterilir; çünkü oyun sürecinin belleği okunmadığı için teknolojinin oyun menüsünden anlık olarak açılıp açılmadığı bu sürümde tespit edilmez.
3. **CPU Sıcaklığı Harici Araç Gereksinimi**:
   - Güvenlik ve anti-cheat ban risklerini önlemek adına `perfoverlay` kendi bünyesinde kernel driver yüklemez.
   - CPU sıcaklığını görmek için açık kaynaklı **LibreHardwareMonitor** uygulamasının "Remote Web Server" özelliği (`http://localhost:8085`) kullanılır. Sunucu kapalıysa değer sessizce `N/A` olarak gösterilir.

---

## Yapılandırma (`config.json`)

Kullanıcı tercihleri `%APPDATA%\perfoverlay\config.json` dosyasında saklanır:

```json
{
  "overlay": {
    "corner": "TopLeft",
    "offsetX": 20,
    "offsetY": 20,
    "backgroundOpacity": 0.35,
    "fontSizeScale": 1.0
  },
  "metrics": {
    "showFps": true,
    "showFrameTime": true,
    "showOnePercentLow": true,
    "showZeroPointOnePercentLow": true,
    "showCpuUsage": true,
    "showCpuTemp": true,
    "showGpuUsage": true,
    "showGpuTemp": true,
    "showVram": true,
    "showGpuClock": true,
    "showGpuPower": true,
    "showUpscaler": true
  },
  "general": {
    "toggleHotkey": "F10",
    "fpsUpdateIntervalMs": 250,
    "sensorUpdateIntervalMs": 500,
    "cpuTempProvider": "auto"
  }
}
```

- **Bozuk Dosya Koruması**: Dosya bozulursa veya JSON sözdizimi geçersizse çökmek yerine `config.json.corrupted_<zaman>.bak` adıyla yedeklenir ve otomatik olarak temiz varsayılan ayarlara dönülür.

---

## Sorun Giderme (Troubleshooting)

### 1. ETW Yetki Hatası (Win32 Error 5 / Access Denied)
Gerçek zamanlı FPS ölçümü Windows ETW çekirdek oturumu gerektirir:
- **Çözüm 1**: `perfoverlay.exe` uygulamasını **Yönetici Olarak Çalıştırın** (Run as Administrator).
- **Çözüm 2 (Yöneticisiz Kullanım)**: Kullanıcı hesabınızı kalıcı olarak `Performance Log Users` grubuna ekleyin:
  ```cmd
  net localgroup "Performance Log Users" "%USERNAME%" /add
  ```
  *(Komutu bir defaya mahsus yönetici komut satırında çalıştırın, ardından oturumunuzu kapatıp tekrar açın).*

### 2. GPU Telemetrisi Görünmüyor veya `N/A`
- **AMD Radeon GPU'lar**: AMD Software: Adrenalin Edition sürücüsü kurulu olmalıdır (`amdadlx64.dll` otomatik yüklenir).
- **NVIDIA / Intel GPU'lar**: AMD ADLX bulunamadığında Windows PDH sayaçları devreye girer ve toplam GPU kullanım oranını raporlar.

### 3. CPU Sıcaklığı `N/A` Görünüyor
- [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases) uygulamasını açın.
- Üst menüden `Options -> Remote Web Server -> Run` seçeneğini aktif edin (Port: `8085`).

---

## Derleme Talimatları (Building from Source)

### Gereksinimler
- Windows 10/11 x64
- Visual Studio 2022 veya 2025/2026 (Desktop Development with C++ bileşeni ile)
- CMake 3.25+
- vcpkg paket yöneticisi (`VCPKG_ROOT` ortam değişkeni tanımlı)

### Derleme Adımları
```powershell
# 1. Depoyu klonlayın
git clone https://github.com/berkan/perfoverlay.git
cd perfoverlay

# 2. Release konfigürasyonu ve derleme (Statik CRT /MT ile tek exe)
cmake --preset windows-release
cmake --build --preset windows-release

# 3. Birim testleri çalıştırın (Catch2)
ctest --preset windows-release --output-on-failure
```

Çalıştırılabilir tek dosya `build/windows-release/Release/perfoverlay.exe` altında oluşturulur.

---

## Lisans

Bu proje [MIT Lisansı](LICENSE) altında sunulmaktadır.
