# Zhilly Firmware Detaylı Kullanım Kılavuzu (User Manual)

Bu kılavuz, siber güvenlik araştırmacıları ve teknoloji meraklıları için Zhilly firmware'inin gelişmiş özelliklerini ve kod bazlı çalışma prensiplerini detaylandırmaktadır.

---

## 1. Desteklenen Modeller ve Durum

Firmware, genelleştirilmiş bir yapıya sahip olsa da belirli donanımlar için özel optimizasyonlar içerir:

-   **LilyGO T-Embed CC1101 (Birincil):** Tüm RF, IR, BadUSB ve AI özellikleri tam kapasite çalışır.
-   **LilyGO T-Watch S3:** Şu an için sadece **AI Sohbet** özelliği aktiftir. Diğer Pentest araçları geliştirme aşamasındadır.
-   **Diğer Modeller:** `main/boards` klasöründe yer alan 90'dan fazla board tanımı temel AI fonksiyonlarını destekleme potansiyeline sahiptir, ancak donanım (CC1101, IR LED vb.) eksikliği nedeniyle pentest modülleri kısıtlı olabilir.

---

## 2. Yapay Zeka Sesli Komut Sistemi (MCP)

Zhilly, gücünü **Model Context Protocol (MCP)** üzerinden donanım kontrol araçlarından alır.

-   **Uyandırma:** "Nihao Miaoban"
-   **Dil Desteği:** Türkçe ve İngilizce'yi akıcı bir şekilde konuşur ve anlar.
-   **Göz Animasyonları:** ST7789 ekranı üzerinden dinleme (listening), düşünme (thinking) ve konuşma (speaking) durumlarını görsel olarak belirtir.

---

## 3. RF & Sub-GHz Araçları (CC1101)

`cc1101_service.cc` üzerinden kontrol edilen bu modül, 300-928 MHz frekans aralığında çalışır.

### Kullanım ve Komutlar:
-   **RF Jammer:** "RF jammer'ı başlat" (Varsayılan olarak sonsuz, limit istenirse "10 saniye boyunca jammer'ı çalıştır" denilebilir).
-   **RF Replay:** "SD karttaki 'garaj.sub' dosyasını tekrar yayınla." (Dosya yolu otomatik olarak `/sdcard/sub` veya root'ta aranır).
-   **Tesla Port Opener:** "Tesla şarj portunu aç." (433.92 MHz, AM650 protokolü sabitlenmiş özel sinyal gönderir).
-   **RAW Capture (Kayıt):** Dinleme modunda yakalanan son sinyali SD karta kaydetmek için "Son yakalamayı 'test.sub' olarak kaydet" komutu verilir.

### SD Kart Yapısı:
-   `/sdcard/sub/`: RF sinyal dosyaları (.sub). Flipper Zero formatı ile uyumludur.
-   `/sdcard/cc1101_presets.json`: Özel frekans ve modülasyon ayarları.

---

## 4. Kızılötesi (IR) Araçları

`ir_service.cc` üzerinden kontrol edilen IR modülü, TV ve AC kontrolü gibi işlemleri gerçekleştirir.

-   **TV-B-Gone:** "Etraftaki televizyonları kapat." (Bölgeye göre 'eu' veya 'us' kodlarını sırayla gönderir).
-   **IR Jammer:** "IR sinyallerini engelle." (Sweep, basic ve random modlarında çalışabilir).
-   **IR Replay:** SD karttaki `.ir` dosyalarından komut gönderir: "Samsung TV için ses açma komutunu gönder."

---

## 5. BadUSB & Savaş Modu (Combat Mode)

`bad_usb_service.cc` içerisinde yer alan **Savaş Modu**, cihazın tüm kaynaklarını klavye simülasyonuna yönlendirir.

### Savaş Modu (Combat Mode) Nedir?
BadUSB komutu verildiğinde cihaz otomatik olarak "Savaş Moduna" girer. Bu modda:
-   Ses servisi durdurulur.
-   Tüm RF ve IR işlemlerine son verilir.
-   Cihaz kendini bilgisayara bir HID Klavye olarak tanıtır (TinyUSB).
-   Wi-Fi ve Mikrofon açık kalarak asistanın emir almaya devam etmesi sağlanır.

### Klavye Düzenleri:
Varsayılan olarak `en_US` kullanılır. Değiştirmek için:
*"Klavye düzenini Türkçe yap"* -> `tr_TR`

---

## 6. LED Halkası (WS2812)

8 adet adreslenebilir RGB LED ile görsel durum bildirimleri:
-   "LED'leri kırmızı yap."
-   "Parlaklığı 5 seviyesine getir."
-   "Mavi renkte kaydırma efekti yap."

---

## 7. Kurulum ve Derleme (Flaşlama)

`flash_binaries/` klasöründeki dosyalar `esptool.py` ile yüklenebilir:

```bash
esptool.py -p [PORT] -b 460800 write_flash 0x0 xiaozhi.bin 0x0800000 expression_assets.bin
```

> **Önemli:** Cihaz ilk açıldığında Wi-Fi bilgilerini girmek için seri terminali (115200 baud) kullanabilir veya varsa ekran üzerindeki PIN kodunu takip edebilirsiniz.

---

## 8. Güvenlik ve Etik Kullanım

Bu firmware sadece eğitim ve siber güvenlik testi amacıyla geliştirilmiştir. Yetkisiz sistemlere karşı kullanımı yasal sorumluluk doğurabilir.
