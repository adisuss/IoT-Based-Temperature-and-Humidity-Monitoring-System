---

### **IoT-Based Temperature and Humidity Monitoring System**

**Deskripsi:**  
Proyek ini adalah sistem **Monitoring Suhu dan Kelembapan berbasis IoT** yang menggunakan **ESP32** untuk membaca data suhu dan kelembapan. Data dikirim secara real-time ke **Firebase** dan ditampilkan pada aplikasi mobile berbasis **Flutter**. Selain itu, data juga otomatis tersimpan di **Google Sheets** untuk analisis lebih lanjut. Sistem ini dilengkapi dengan fitur **notifikasi** yang memberikan peringatan saat suhu berada di luar batas yang ditentukan, serta grafik data untuk visualisasi historis.

**Fitur Utama:**  
✅ **Wi-Fi Provisioning** – Memungkinkan pengguna untuk menambahkan perangkat baru ke sistem melalui Wi-Fi dengan mudah dan cepat.  
✅ **Realtime Monitoring** – Data suhu dan kelembapan ditampilkan secara langsung di aplikasi mobile berbasis Flutter.  
✅ **Data Logging** – Data suhu dan kelembapan otomatis disimpan di **Google Sheets** untuk akses dan analisis lebih lanjut.  
✅ **Notifikasi** – Pemberitahuan dikirim ke aplikasi jika suhu berada di bawah atau di atas 25°C, memberikan peringatan kepada pengguna.  
✅ **Grafik Data** – Menampilkan riwayat suhu dan kelembapan dalam bentuk grafik interaktif yang dapat digunakan untuk analisis tren.

**Teknologi yang Digunakan:**  
🔹 **ESP32** – Mikrokontroler utama yang menghubungkan sensor suhu dan kelembapan serta mengirim data ke cloud.  
🔹 **DHT11** – Sensor suhu dan kelembapan yang digunakan untuk mengukur kondisi lingkungan secara akurat.  
🔹 **Firebase** – Backend cloud untuk menyimpan data, menyediakan autentikasi pengguna, dan mengirim push notifications.  
🔹 **Flutter** – Framework untuk membuat aplikasi mobile yang menampilkan data secara real-time dan interaktif.  
🔹 **Google Sheets API** – Menyimpan data suhu dan kelembapan secara otomatis dalam format spreadsheet untuk keperluan analisis lebih lanjut.
