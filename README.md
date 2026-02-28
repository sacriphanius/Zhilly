<p align="center">
  <img src="https://raw.githubusercontent.com/78/xiaozhi-esp32/main/docs/assets/logo.png" alt="AI Pentester Assistant" width="150"/>
</p>

<h1 align="center" style="color: #FF5722;">🛡️ T-Embed AI Pentester Asistant</h1>
<p align="center">
<b>AI-Powered, Portable Cyber Security & Cyber Interaction Tool</b><br>
<b>Yapay Zeka Destekli, Taşınabilir Siber Güvenlik ve Siber Etkileşim Aracı</b>
</p>

<p align="center">
  <a href="#english">🇬🇧 English</a> • <a href="#turkish">🇹🇷 Türkçe</a>
</p>

---

<br>

<h2 id="english" style="color: #fff; background-color: #333; padding: 10px; border-radius: 5px;">🇬🇧 English / English Version</h2>

## 📖 About the Project

This project is a conceptually optimized **AI Pentester Assistant** powered by the open-source [**Xiaozhi-esp32 (小智)**](https://github.com/78/xiaozhi-esp32) infrastructure, specifically designed to run on the **LilyGO T-Embed CC1101** hardware.

Our goal is to build a smart device that you can carry with you at all times, interact with both software and hardware, and directly assist you in your cybersecurity research (or daily testing/hacking operations).

<br>

## ✨ Features & Development Roadmap

<h3 style="color: #4CAF50;">✅ Current (Level 1)</h3>
The assistant is currently in the **Voice Response System** stage.
- 🎙️ **Advanced Voice Communication:** Connects to Xiaozhi AI servers to communicate with you in natural language about cybersecurity, technical topics, or anything else.
- 🔊 **Custom Audio Drivers:** Hardware I2S speaker output and PDM microphone optimizations have been made specifically for the T-Embed CC1101 board.
- 👀 **Emote Display Experience:** Instead of a classic text interface, the device interacts with the user through emotional feedback (eye animations) on its ST7789-based display.

<h3 style="color: #2196F3;">🚀 Upcoming Features (Level 2 & Beyond)</h3>
This version prepares the foundation for the assistant to gain hardware testing capabilities. The upcoming **Modular Pentest** command sets will include:
- 📡 **RF (Sub-1GHz) Control:** RF analysis and packet replay capabilities via CC1101 integration.
- 📶 **WiFi Auditing:** Network discovery, vulnerability scanning tools, Deauth / Phishing simulation assistant.
- 🔮 **IR (Infrared):** IR signal copying and playback (universal remote analysis for TVs, projectors, split ACs).
- 🏷️ **NFC/RFID Reading:** Assisting with card cloning, reading, and emulation tests using PN532-based modules.

<br>

---

<h2 style="color: #9C27B0;">🛠️ Installation / Flashing</h2>

Compiling and flashing the project is very simple. Connect your T-Embed device to the computer and upload the **single file (merged-binary)** version we prepared.

```bash
# You can flash the ready-made firmware using EspTool.py:
esptool.py -p /dev/ttyACM0 -b 460800 write_flash 0x0 firmware/merged-binary.bin
```

*(Developers can compile and upload the project using the classic ESP-IDF command: `idf.py flash`)*

<br>

---

<h2 style="color: #E91E63;">🌐 Registration & Server Connection (Xiaozhi Integration)</h2>

For your assistant to come to life with your words, you need to register the project on the Xiaozhi AI platform.

<h3 style="color: #FF9800;">Step 1: WiFi Connection</h3>
1. When you power on the device, you will see a QR code or "Connecting to WiFi" menu on the screen.
2. The device will create a `xiaozhi-xxxx` network via *SmartConfig* or its own Hotspot.
3. Connect to this network from your phone, and enter your home/office WiFi password into your assistant via the portal that opens.

<h3 style="color: #FF9800;">Step 2: Device Registration</h3>
1. Go to the assistant control address broadcasted by Xiaozhi on the local network via computer or phone (you can get the IP address from the device's screen).
2. Go to the **Xiaozhi AI / FishAudio, etc.** service backend pages that serve as the project's config panel to register.
3. After logging in with OAuth (Github, Google, etc.), save the token or session settings provided by the system via the UI.

*(For a detailed guide on Xiaozhi servers and self-hosted backend solutions, check out the: [Xiaozhi Server Setup Documentation](https://github.com/78/xiaozhi-esp32))*

<br>
<br>
<br>

---

<h2 id="turkish" style="color: #fff; background-color: #333; padding: 10px; border-radius: 5px;">🇹🇷 Türkçe / Turkish Version</h2>

## 📖 Proje Hakkında

Bu proje, gücünü açık kaynaklı [**Xiaozhi-esp32 (小智)**](https://github.com/78/xiaozhi-esp32) altyapısından alan ve **LilyGO T-Embed CC1101** donanımında çalışmak üzere özel olarak optimize edilmiş bir **Yapay Zeka Pentester Asistanı** konseptidir.

Amacımız; her zaman yanınızda taşıyabileceğiniz, hem yazılım hem donanım olarak etkileşime geçebileceğiniz ve siber güvenlik araştırmalarınızda (veya günlük hack/test operasyonlarınızda) size doğrudan asistanlık yapabilecek akıllı bir cihaz inşa etmektir.

<br>

## ✨ Yetenekler & Geliştirme Yol Haritası

<h3 style="color: #4CAF50;">✅ Mevcut (Seviye 1)</h3>
Şu anda asistan **Sesli Yanıt Sistemi** aşamasındadır.
- 🎙️ **Gelişmiş Sesli İletişim:** Xiaozhi AI sunucularına bağlanarak siber güvenlik ve teknik konularda (veya herhangi bir konuda) sizinle doğal dille iletişim kurabilir.
- 🔊 **Özel Ses Sürücüleri:** T-Embed CC1101 board üzerinde donanımsal I2S hoparlör çıkışı ve PDM mikrofon optimizasyonları yapılmıştır.
- 👀 **Emote Ekran Deneyimi:** Klasik metin arayüzü yerine cihaz, ST7789 tabanlı ekranında kullanıcıyla duygusal geri bildirim (göz animasyonları) ile etkileşime girer.

<h3 style="color: #2196F3;">🚀 Gelecek Özellikler (Seviye 2 ve Sonrası)</h3>
Bu sürüm, asistanın donanımsal test yeteneklerini kazanacağı altyapıyı hazırlar. Yakında eklenecek olan **Modüler Pentest** komut setleri şunları içerecektir:
- 📡 **RF (Sub-1GHz) Kontrolü:** CC1101 entegrasyonu ile RF analizi ve paket replay yetenekleri.
- 📶 **WiFi Denetimi:** Ağ keşfi, zafiyet tarama araçları, Deauthing / Phishing simülasyon asistanı.
- 🔮 **IR (Kızılötesi):** IR sinyal kopyalama ve oynatma (TV, projektör, split klimalar için evrensel kumanda analizleri).
- 🏷️ **NFC/RFID Okuma:** PN532 tabanlı modüllerle kart klonlama, okuma ve emülasyon testlerine yardımcı olma.

<br>

---

<h2 style="color: #9C27B0;">🛠️ Kurulum / Flash İşlemi </h2>

Projeyi derlemek ve flashlamak oldukça basittir. T-Embed cihazınızı bilgisayara bağlayın ve hazırladığımız **tek dosya (merged-binary)** sürümünü yükleyin.

```bash
# EspTool.py kullanarak hazır firmware'i cihazınıza atabilirsiniz:
esptool.py -p /dev/ttyACM0 -b 460800 write_flash 0x0 firmware/merged-binary.bin
```
*(Geliştiriciler projeyi klasik ESP-IDF komutu olan `idf.py flash` ile derleyerek yükleyebilir.)*

<br>

---

<h2 style="color: #E91E63;">🌐 Kayıt ve Sunucu Bağlantısı (Xiaozhi Entegrasyonu)</h2>

Asistanınızın sizin sözlerinizle hayat bulabilmesi için projeyi Xiaozhi AI platformuna kaydetmeniz gerekir.

<h3 style="color: #FF9800;">Adım 1: WiFi Bağlantısı</h3>
1. Cihaza güç verdiğinizde, ekranında bir karekod veya "WiFi bağlanıyor" menüsü göreceksiniz.
2. Cihaz *SmartConfig* veya doğrudan yayına aldığı Hotspot üzerinden `xiaozhi-xxxx` ağı yaratacaktır.
3. Telefonunuzdan bu ağa bağlanın, açılan portal üzerinden kendi ev/iş WiFi ağınızın şifresini asistanınıza tanımlayın.

<h3 style="color: #FF9800;">Adım 2: Cihazın Kaydedilmesi</h3>
1. Bilgisayardan veya telefondan Xiaozhi'nin yerel ağ üzerinde yayınladığı asistan kontrol adresine girin (IP adresini cihazın ekranından alabilirsiniz).
2. Kayıt olmak için yönlendirdiği veya projenin config paneli olarak gelen **Xiaozhi AI / FishAudio vb.** servis backend sayfalarına gidin.
3. OAuth (Github, Google vs.) ile giriş yaptıktan sonra sistemin size verdiği token veya session ayarlarını UI üzerinden kaydedin. 

*(Xiaozhi sunucuları ve self-hosted backend çözümleri detaylı rehberi için: [Xiaozhi Sunucu Kurulum Dokümantasyonu](https://github.com/78/xiaozhi-esp32) adresini inceleyebilirsiniz.)*

<br>

---

### 📝 Contribution / Katkıda Bulunma
Siber güvenlik dünyası beraber büyür. Kodlama, tasarım veya yeni saldırı vektörü modülleri hakkında fikirleriniz var ise PR (*Pull Request*) göndermekten çekinmeyin! / The cybersecurity world grows together. If you have ideas about coding, design, or new attack vector modules, feel free to send a PR (*Pull Request*)!

> *"Talk is cheap. Show me the code."* — Linus Torvalds
