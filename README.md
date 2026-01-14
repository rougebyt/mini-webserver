### README.md


# mini-webserver

Tiny, single-file HTTP/1.1 static file server written in pure C 

Uses **epoll** for efficient non-blocking I/O on Linux.

### Features

- Serves static files from a directory
- Handles GET requests only
- Proper MIME types
- 404 & 403 responses
- Basic protection against directory traversal (`../`)
- Clean, readable code (educational purpose)
- Very small footprint

### Quick Start (Linux / WSL2 / Docker)

```bash
# Build
make

# Run (default port 8080, root = ./public)
./mini-webserver

# Run on custom port and custom root directory
./mini-webserver 9000 ./public
```

Open browser: http://localhost:8080/

### Using Docker

```bash
docker build -t mini-webserver .
docker run -p 8080:8080 mini-webserver
```

### Windows Users

The current version uses `epoll` → Linux only.

**Recommended options:**

1. **Best & easiest**: Use **WSL2** (Windows Subsystem for Linux)
   - `wsl --install` → install Ubuntu → run inside WSL as normal Linux

2. **Docker Desktop** with WSL2 backend (also very convenient)

3. Native Windows version (using `select()` instead of epoll) is possible but not included in this repo.
   Let me know if you want a separate `select()`-based branch/version.

### Project Goals

- Demonstrate clean systems programming in C
- Show efficient I/O multiplexing with epoll
- Keep codebase small and understandable (<500 LOC)
- Serve as a learning example for network programming

### Build Requirements (Linux/WSL)

- gcc / clang
- make

### License

MIT

Made with ❤️ during the 3-Month Challenge  
© 2026 Moibon Dereje (@rougeByt)


