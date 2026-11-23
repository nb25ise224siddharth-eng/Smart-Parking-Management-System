# SmartPark C Backend — Windows local build

This folder runs a native C HTTP server with SQLite on your Windows computer. It does not use Vercel, Render, Flask, Python, or cloud hosting.

## 1. Install the compiler and SQLite

Install MSYS2 from https://www.msys2.org/. Open the **MSYS2 UCRT64** terminal from the Start menu and run:

~~~bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3
~~~

## 2. Open the project folder

In the UCRT64 terminal, change directory to this c_backend folder. Example:

~~~bash
cd /c/Users/YOUR_NAME/path/to/Smart-Parking-Management-System/c_backend
~~~

## 3. Compile and run

~~~bash
gcc -std=c11 -O2 -Wall -Wextra -o smartpark.exe main.c -lsqlite3 -lws2_32 -lbcrypt
./smartpark.exe
~~~

Open http://127.0.0.1:8080 in your browser.

Demo administrator login: username admin, password admin123.

The app creates smartpark.db automatically and seeds parking slots P01–P10. The server binds only to 127.0.0.1, so it is local to this PC. Stop it with Ctrl+C.

This is a coursework/demo implementation. Do not expose it to the public internet or use real personal data without strengthening authentication and request handling. The original Flask files and main branch remain unchanged.