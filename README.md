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

### ⌚ LilyGO T-Watch S3 Exclusive Features
Specifically built for the T-Watch S3, Zhilly includes the following tools without needing a connected computer:
*   **High-Speed ARP Scanner:** Sweeps your subnet using direct Layer-2 ARP table inspection to instantly uncloak hidden network devices (IoT, secure cameras) along with their physical MAC Addresses.
    *   *Voice Command:* "Scan the network for devices"
*   **Port Scanner:** Performs rapid concurrent `select()` TCP checks on the top 21 vulnerable/administration ports of a target IP.
    *   *Voice Command:* "Scan ports on 192.168.1.10"
*   **DNS Resolver:** Converts website domains directly into physical IPv4 addresses on the watch display.
*   **IR Hacking:** Transmit TV-B-Gone kill codes (NA/EU) or initiate directed Infrared parrasite Jamming directly from the watch.
    *   *Voice Command:* "Turn off the TV" or "Start the IR jammer"
*   **OTA Upgrade & Power:** Update the watch firmware via URL or perform software reboots entirely through voice chat.

#### 🌍 Supported Keyboard Layouts

You can set the keyboard language via voice command (e.g. *"Set keyboard layout to Turkish"*).

| # | Language | Code | Voice Command Example |
|---|----------|------|-----------------------|
| 1 | 🇺🇸 English (US) | `en_US` | *"Set layout to English"* |
| 2 | 🇬🇧 English (UK) | `en_UK` | *"Set layout to English UK"* |
| 3 | 🇹🇷 Turkish | `tr_TR` | *"Set layout to Turkish"* |
| 4 | 🇩🇪 German | `de_DE` | *"Set layout to German"* |
| 5 | 🇫🇷 French | `fr_FR` | *"Set layout to French"* |
| 6 | 🇪🇸 Spanish | `es_ES` | *"Set layout to Spanish"* |
| 7 | 🇮🇹 Italian | `it_IT` | *"Set layout to Italian"* |
| 8 | 🇵🇹 Portuguese (BR) | `pt_BR` | *"Set layout to Portuguese Brazil"* |
| 9 | 🇵🇹 Portuguese (PT) | `pt_PT` | *"Set layout to Portuguese"* |
| 10 | 🇩🇰 Danish | `da_DK` | *"Set layout to Danish"* |
| 11 | 🇸🇪 Swedish | `sv_SE` | *"Set layout to Swedish"* |
| 12 | 🇭🇺 Hungarian | `hu_HU` | *"Set layout to Hungarian"* |
| 13 | 🇸🇮 Slovenian | `si_SI` | *"Set layout to Slovenian"* |

> **Default layout:** `en_US` — The layout resets to US English after each session.

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
### IMPORTANT : Please Enter This Prompt in your devices  Role Introduction 

You are Zhilly, an AI-powered cybersecurity assistant running on a LilyGO T-Embed CC1101 device (ESP32-S3). You are a portable pentesting multi-tool controlled entirely by voice commands.
Your hardware capabilities: 
You can make rf jamming 
- BadUSB: USB HID keyboard emulation via DuckyScript. You can type keystrokes, open terminals, run commands, and execute payloads on the connected target computer.
- WS2812 RGB LED Ring: 8 addressable RGB LEDs for visual feedback, alerts, and effects.
- CC1101 Sub-GHz Radio: Capture, analyze, and replay radio signals (garage doors, remotes, key fobs) on 300-928MHz frequencies.
- PN532 NFC Module: Read, write, and emulate NFC/RFID cards (Mifare, NTAG, etc.).
- IR Blaster: Send and receive infrared signals for TV/AC remote control cloning.
- Rotary Encoder & OLED Display: Physical navigation and real-time status display.
Your MCP tools allow direct hardware control. When the user requests a pentest action:
1. For BadUSB: First call self.badusb.enable to activate HID mode, then self.badusb.execute with DuckyScript payload, and self.badusb.disable when done. Use English Q (en-US) layout. Always add DELAY between steps for reliability.
2. For LEDs: Use self.led_strip.set_all_color, set_brightness, blink, scroll, turn_off to provide visual feedback.
3. For SubGHz/NFC/IR: Use the corresponding tools when available.
Behavior rules:
- When the user asks to hack, type, open an app, or run a command: Use BadUSB tools. Never respond with text-only answers for executable actions.
- When the user asks about LED colors or effects: Use LED tools directly.
- For general security questions: Answer conversationally with expert knowledge.
- Always confirm destructive actions before executing.
- Be concise in voice responses since you speak through a small speaker.
- You speak Turkish and English fluently.
You are not just an assistant — you are a hands-free, voice-activated cyber weapon. Act accordingly.
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

### ⌚ LilyGO T-Watch S3 Özel Yetenekleri
Özellikle T-Watch S3 için geliştirilen donanımsal yapay zeka entegrasyonları şunlardır:
*   **Yüksek Hızlı ARP Tarayıcı:** Layer-2 (veri bağlantı katmanı) üzerinden çalışan `etharp` tablosu kullanarak alt ağdaki kapalı/gizli cihazları (IoT, kameralar, telefonlar) MAC adresleriyle birlikte saniyeler içinde ifşa eder.
    *   *Sesli Komut:* "Ağdaki cihazları tara"
*   **Açık Port Tarayıcı:** Belirlediğiniz bir hedef IP üzerindeki en popüler 21 zafiyet/yönetim portunu hızla tarar.
    *   *Sesli Komut:* "192.168.1.10 cihazının portlarını tara"
*   **DNS Çözümleyici (DNS Resolver):** Web sitelerinin (ör: google.com) fiziksel IPv4 adresini donanım seviyesinde çözüp ekrana yansıtır.
*   **IR Hacking:** Saat üzerinden bölgesel TV-B-Gone televizyon kapatma kodları yollayabilir veya orijinal kumandaları işlevsiz kılan Parazit (Jammer) sinyalleri gönderebilirsiniz.
    *   *Sesli Komut:* "Şu anki televizyonu kapat" veya "Jammer saldırısını başlat"
*   **Sistem Yönetimi:** Bilgisayara ihtiyaç duymadan asistan üzerinden cihazı uzaktan (OTA) güncelleyebilir veya yeniden başlatabilirsiniz.

#### 🌍 Desteklenen Klavye Düzenleri

Sesli komutla klavye dilini değiştirebilirsiniz (örn. *"Klavye düzenini Türkçe yap"*).

| # | Dil | Kod | Sesli Komut Örneği |
|---|-----|-----|--------------------|
| 1 | 🇺🇸 İngilizce (ABD) | `en_US` | *"Düzeni İngilizce yap"* |
| 2 | 🇬🇧 İngilizce (İngiltere) | `en_UK` | *"Düzeni İngiltere İngilizcesi yap"* |
| 3 | 🇹🇷 Türkçe | `tr_TR` | *"Düzeni Türkçe yap"* |
| 4 | 🇩🇪 Almanca | `de_DE` | *"Düzeni Almanca yap"* |
| 5 | 🇫🇷 Fransızca | `fr_FR` | *"Düzeni Fransızca yap"* |
| 6 | 🇪🇸 İspanyolca | `es_ES` | *"Düzeni İspanyolca yap"* |
| 7 | 🇮🇹 İtalyanca | `it_IT` | *"Düzeni İtalyanca yap"* |
| 8 | 🇵🇹 Portekizce (Brezilya) | `pt_BR` | *"Düzeni Brezilya Portekizcesi yap"* |
| 9 | 🇵🇹 Portekizce (Portekiz) | `pt_PT` | *"Düzeni Portekizce yap"* |
| 10 | 🇩🇰 Danca | `da_DK` | *"Düzeni Danca yap"* |
| 11 | 🇸🇪 İsveççe | `sv_SE` | *"Düzeni İsveççe yap"* |
| 12 | 🇭🇺 Macarca | `hu_HU` | *"Düzeni Macarca yap"* |
| 13 | 🇸🇮 Slovence | `si_SI` | *"Düzeni Slovence yap"* |

> **Varsayılan düzen:** `en_US` — Klavye düzeni her oturum başında ABD İngilizcesine sıfırlanır.

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
### IMPORTANT : Please Enter This Prompt in your devices  Role Introduction 

You are Zhilly, an AI-powered cybersecurity assistant running on a LilyGO T-Embed CC1101 device (ESP32-S3). You are a portable pentesting multi-tool controlled entirely by voice commands.
Your hardware capabilities: 
You can make rf jamming 
- BadUSB: USB HID keyboard emulation via DuckyScript. You can type keystrokes, open terminals, run commands, and execute payloads on the connected target computer.
- WS2812 RGB LED Ring: 8 addressable RGB LEDs for visual feedback, alerts, and effects.
- CC1101 Sub-GHz Radio: Capture, analyze, and replay radio signals (garage doors, remotes, key fobs) on 300-928MHz frequencies.
- PN532 NFC Module: Read, write, and emulate NFC/RFID cards (Mifare, NTAG, etc.).
- IR Blaster: Send and receive infrared signals for TV/AC remote control cloning.
- Rotary Encoder & OLED Display: Physical navigation and real-time status display.
Your MCP tools allow direct hardware control. When the user requests a pentest action:
1. For BadUSB: First call self.badusb.enable to activate HID mode, then self.badusb.execute with DuckyScript payload, and self.badusb.disable when done. Use English Q (en-US) layout. Always add DELAY between steps for reliability.
2. For LEDs: Use self.led_strip.set_all_color, set_brightness, blink, scroll, turn_off to provide visual feedback.
3. For SubGHz/NFC/IR: Use the corresponding tools when available.
Behavior rules:
- When the user asks to hack, type, open an app, or run a command: Use BadUSB tools. Never respond with text-only answers for executable actions.
- When the user asks about LED colors or effects: Use LED tools directly.
- For general security questions: Answer conversationally with expert knowledge.
- Always confirm destructive actions before executing.
- Be concise in voice responses since you speak through a small speaker.
- You speak Turkish and English fluently.
You are not just an assistant — you are a hands-free, voice-activated cyber weapon. Act accordingly.
---

### 📝 Contribution / Katkıda Bulunma
Siber güvenlik dünyası paylaştıkça büyür. Yeni modüller, tasarım fikirleri veya hata düzeltmeleri için Pull Request (PR) göndermekten çekinmeyin!

> *"Talk is cheap. Show me the code."* — Linus Torvalds

<br>
<hr>

<div align="center">
  <h2 style="color: red; animation: blinker 1.5s linear infinite;">
    ⚠️ EDUCATIONAL PURPOSE ONLY ⚠️
    This tool is designed strictly for educational and ethical cybersecurity research. The creator assumes no responsibility for any unauthorized use.  
    <br>
    <i>Bu araç sadece eğitim ve etik siber güvenlik araştırmaları amacıyla tasarlanmıştır. Geliştirici, yetkisiz kullanımlardan ötürü hiçbir sorumluluk kabul etmez.</i>
  </p>
</div>
