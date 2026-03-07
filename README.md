<p align="center">
  <img src="https://raw.githubusercontent.com/78/xiaozhi-esp32/main/docs/assets/logo.png" alt="AI Pentester Assistant" width="150"/>
</p>

<h1 align="center" style="color: #FF5722;">🛡️ Zhilly AI Pentester Assistant</h1>
<p align="center">
<b>AI-Powered, Portable Cyber Security & Cyber Interaction Tool</b><br>
<b>Yapay Zeka Destekli, Taşınabilir Siber Güvenlik ve Siber Etkileşim Aracı</b>
</p>

<p align="center">
  <a href="#english">🇬🇧 English</a> • <a href="#turkish">🇹🇷 Türkçe</a>
</p>

---

<h2 id="english" style="color: #fff; background-color: #333; padding: 10px; border-radius: 5px;">🇬🇧 English / English Version</h2>

## 📖 About the Project
This project is an advanced **AI Pentester Assistant** based on the [**Xiaozhi-esp32 (小智)**](https://github.com/78/xiaozhi-esp32) infrastructure, optimized for the **LilyGO T-Embed CC1101** hardware. It combines natural language AI interaction with physical hardware testing tools, making it a versitile companion for cybersecurity research and daily technical tasks.

## 🚀 Key Features

### 🤖 AI & Interaction
*   **Natural Conversation:** Speak to your assistant using the wake word **"Nihao Miaoban"**. It listens, learns, and responds in natural language.
*   **Emote Display:** High-quality eye animations on the ST7789 display provide emotional feedback and status indicators (listening, thinking, speaking).

### 📰 Smart News (News API)
*   Fetch the latest global or local headlines via voice commands.
    *   *Example:* "Tell me the latest technology news" or "What's the news in Turkey?"
*   Integrated via the `news.get_top_headlines` tool using NewsAPI.

### 📡 RF & IR Pentest Tools
Complete control over Sub-GHz and Infrared spectrums:
*   **RF Jammer:** Start/Stop signal jamming on specific Sub-GHz frequencies.
*   **RF Replay (Test):** Re-transmit `.sub` files stored on your SD card.
*   **IR Jammer:** Disrupt and evaluate infrared-controlled devices.
*   **TV-B-Gone:** Universal remote off-codes to switch off nearby TVs.
*   **IR Replay (Test):** Record and re-transmit infrared signals.
*   **RAW Capture:** Capture Sub-GHz signals with microsecond precision.
*   **SD Card Storage:** Save captured signals to the SD card for later use.

### ⌨️ Bad USB & HID (AI-Powered Voice Commands Only)
*   **Keyboard Emulation:** Your assistant can act as a standard USB keyboard when connected to a computer.
*   **Voice-Driven Support:** Run automated keystroke scripts (DuckyScript) stored on the SD card **exclusively via AI voice commands**. No manual interface is provided, ensuring a hands-free pentesting experience.
    *   *Voice Command:* "Run BadUSB script 'hello_world.txt'" or "Type this: 'I am your AI assistant'".
*   **HID Tools:** Fully integrated via `usb.bad_usb_run`, `usb.bad_usb_type`, and `usb.bad_usb_stop` tools.

### 🌈 LED Strip & Visual Effects
Control the 8x WS2812 RGB LED ring:
*   **Dynamic Control:** Set colors, brightness (0-8), and animations like **Blink** or **Scroll**.
    *   *Voice Command:* "Make the LEDs blue and set brightness to max."

### 🔋 System & Power
*   **Battery Status:** Ask about battery level or charging status.
*   **Deep Sleep:** Long-press the **Power (PWR)** button to enter deep sleep for maximum battery saving.
*   **Brightness Control:** Voice-adjust screen and LED intensity.

---

<h2 style="color: #9C27B0;">🛠️ Installation & Flashing</h2>

We provide a clean, comment-free codebase and pre-compiled binaries in the `flash_binaries/` folder.

```bash
# Flash the ready-made firmware using EspTool.py:
esptool.py -p /dev/ttyACM0 -b 460800 write_flash 0x0 flash_binaries/xiaozhi.bin 0x0800000 flash_binaries/expression_assets.bin
```

---

<br>

<h2 id="turkish" style="color: #fff; background-color: #333; padding: 10px; border-radius: 5px;">🇹🇷 Türkçe / Turkish Version</h2>

## 📖 Proje Hakkında
Bu proje, açık kaynaklı [**Xiaozhi-esp32 (小智)**](https://github.com/78/xiaozhi-esp32) altyapısını kullanan ve **LilyGO T-Embed CC1101** donanımı için optimize edilmiş, gelişmiş bir **Zhilly AI Pentester Asistanıdır**. Doğal dil etkileşimini fiziksel donanım test araçlarıyla birleştirerek siber güvenlik araştırmalarınızda size akıllı bir yol arkadaşı olur.

## 🚀 Öne Çıkan Özellikler

### 🤖 AI ve Etkileşim
*   **Doğal Sohbet:** Cihazı **"Nihao Miaoban"** diyerek uyandırın ve onunla doğal dilde konuşun. Sizi anlar, öğrenir ve yanıt verir.
*   **Emote Ekran:** ST7789 ekranındaki canlı göz animasyonları ile sosyal ve duygusal geri bildirim sağlar (dinleme, düşünme, konuşma durumları).

### 📰 Akıllı Haberler (News API)
*   Dünyadan veya Türkiye'den en güncel haber başlıklarını sesli komutla öğrenin.
    *   *Örnek:* "En son teknoloji haberlerini anlat" veya "Türkiye'deki güncel manşetler neler?"
*   NewsAPI üzerinden çalışan `news.get_top_headlines` aracı ile entegre edilmiştir.

### 📡 RF & IR Pentest Araçları
Sub-GHz ve Kızılötesi frekansları üzerinde tam kontrol:
*   **RF Jammer:** Belirli frekanslarda sinyal engelleyiciyi (jammer) başlatın veya durdurun.
*   **RF Replay (Test):** SD karttaki `.sub` dosyalarını tekrar yayınlayın (Transmit).
*   **IR Jammer:** Kızılötesi kontrollü cihazları test etmek için sinyal engelleyici.
*   **TV-B-Gone:** Yakındaki televizyonları kapatmak için evrensel kapatma sinyalleri.
*   **IR Replay (Test):** Kızılötesi sinyalleri kaydedin ve tekrar yayınlayın.
*   **RAW Capture:** RF sinyallerini mikro saniye hassasiyetiyle yakalayın.
*   **SD Kart Kaydı:** Yakalanan sinyalleri daha sonra kullanmak üzere SD karta kaydedin.

### ⌨️ Bad USB & HID (Sadece AI Destekli Sesli Komutlar)
*   **Klavye Simülasyonu:** Asistanınız bilgisayara bağlandığında kendini standart bir USB klavye olarak tanıtır.
*   **Ses Komutu Odaklı:** SD kartta saklanan DuckyScript betiklerini **sadece ve sadece yapay zeka sesli komutlarıyla** otomatik olarak çalıştırın. Manuel bir arayüz yoktur, tamamen sesle kontrol edilir.
    *   *Sesli Komut:* "BadUSB betiği 'selam.txt' çalıştır" veya "Şunu yaz: 'Ben senin yapay zeka asistanınım'".
*   **HID Araçları:** `usb.bad_usb_run`, `usb.bad_usb_type` ve `usb.bad_usb_stop` araçları ile tam uyumlu çalışır.

### 🌈 LED Halkası ve Efektler
8 adet WS2812 RGB LED'i kontrol edin:
*   **Dinamik Kontrol:** Renk, parlaklık (0-8) ve **Blink** veya **Scroll** gibi animasyonları ayarlayın.
    *   *Sesli Komut:* "LED'leri mavi yap ve parlaklığı en sona getir."

### 🔋 Sistem ve Güç
*   **Batarya Durumu:** Pil seviyesini ve şarj durumunu sorgulayın.
*   **Derin Uyku (Deep Sleep):** **Güç (PWR)** butonuna uzun basarak cihazı uyku moduna sokun.
*   **Parlaklık Kontrolü:** Ekran ve LED parlaklığını sesli komutla değiştirin.

---

<h2 style="color: #9C27B0;">🛠️ Kurulum ve Yükleme</h2>

`flash_binaries/` klasöründe hazır derlenmiş dosyaları ve temizlenmiş kaynak kodlarını bulabilirsiniz.

```bash
# Hazır firmware'i EspTool.py ile yükleyin:
esptool.py -p /dev/ttyACM0 -b 460800 write_flash 0x0 flash_binaries/xiaozhi.bin 0x0800000 flash_binaries/expression_assets.bin
```

---

### 📝 Contribution / Katkıda Bulunma
Siber güvenlik dünyası paylaştıkça büyür. Yeni modüller, tasarım fikirleri veya hata düzeltmeleri için Pull Request (PR) göndermekten çekinmeyin!

> *"Talk is cheap. Show me the code."* — Linus Torvalds

<br>
<hr>

<div align="center">
  <h2 style="color: red; animation: blinker 1.5s linear infinite;">
    ⚠️ EDUCATIONAL PURPOSE ONLY ⚠️
  </h2>
  <style>
    @keyframes blinker {
      50% { opacity: 0; }
    }
  </style>
  <p style="color: #ffcccc;">
    This tool is designed strictly for educational and ethical cybersecurity research. The creator assumes no responsibility for any unauthorized use.  
    <br>
    <i>Bu araç sadece eğitim ve etik siber güvenlik araştırmaları amacıyla tasarlanmıştır. Geliştirici, yetkisiz kullanımlardan ötürü hiçbir sorumluluk kabul etmez.</i>
  </p>
</div>
