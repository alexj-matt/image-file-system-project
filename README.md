# 💻 Image File System
*A systems programming project from the Computer Systems course at EPFL*  

This project implements an **image-oriented file system (ImgFS)** with a **client-server architecture**.  
Starting from a low-level TCP server, we gradually built a simplified **HTTP server**, added support for the **ImgFS API** and extended it into a **multithreaded web service** exposing full CRUD operations.  

---

## ⚙️ Functionalities

- **Create (Insert):** Upload an image into the ImgFS over HTTP (`POST /imgfs/insert`)  
- **Read:** Retrieve an image in different resolutions (`GET /imgfs/read?res=small&img_id=...`)  
- **Update:** Insert can overwrite an existing image with the same ID  
- **Delete:** Remove an image (`/imgfs/delete`)  
- **List:** Return available images in **JSON format** (`/imgfs/list`)  
- **Web Interface:** Provided `index.html` allows testing via a browser  
- **Multithreading:** Server can handle multiple clients simultaneously  

---

## 🛠️ Tech Stack

- **C programming** (socket programming, HTTP parsing, multithreading with `pthread`)  
- **libjson-c** for JSON serialization  
- **libvips** for image processing  

---

## 📂 Explore the Project

- `done/` — completed source code  
- `provided/` — starter code and support material from the course  
- `grading/` — grading scripts and tests  

---

## 🙌 Course

This project was completed as part of the **Computer Systems** course at **EPFL**, focusing on  
- layered system design
- network protocols
- building reliable services with concurrency

---
