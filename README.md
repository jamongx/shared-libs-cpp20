# shared-libs-cpp20 (PDK 4.2)

A modern **C++20** systems library for building multithreaded network servers on Linux.

> PDK 4.2 has graduated to C++20 (std::jthread, std::format, std::stop_token, std::span). The legacy C++17 baseline lives on the `pdk-4.0-cpp17` branch.

Originally developed for real-time streaming servers (3G video call, multimedia RingBack Tone) in production telecom environments. Modernized in 4.x: legacy C++03/pthreads → C++17 (4.0) → C++20 (4.2).

## Features

| Component | Description |
|-----------|-------------|
| **Threading** | `std::thread`-based Thread/QThread, event queues, producer-consumer pattern |
| **Synchronization** | `std::mutex`, `std::condition_variable`, `std::atomic`, RAII locks |
| **Networking** | TCP/UDP server with per-connection threads, Unix/Inet/Raw sockets |
| **Database** | PostgreSQL connection pool with RAII guards (`std::optional`, move-only) |
| **IPC** | POSIX shared memory, System V message queues, semaphores |
| **Logging** | Async rotating file logger (Meyers singleton, producer-consumer queue) |
| **Utilities** | INI parser, binary archive, circular queue, timer management, ICMP monitor |

## Modern C++ Highlights

- **C++20 standard**, no compiler extensions
- **`std::jthread` + `std::stop_source`** — RAII threading with cooperative cancellation (PDK 4.2)
- **`std::format` logging** — type-safe `PDK_LOG_*_FMT` macros (PDK 4.2)
- **Templated `IniFile::get<T>`** — `std::optional<T>` typed config access (PDK 4.2)
- **RAII everywhere** — `std::scoped_lock`, `PgGuard` (scoped DB connection), `File`, `Archive`
- **Smart pointers** — `std::unique_ptr` for owned resources, move-only types
- **Standard threading** — `std::mutex`, `std::condition_variable`, `std::atomic` with explicit memory ordering
- **Modern idioms** — `std::optional`, `std::function`, `[[nodiscard]]`, `noexcept`, lambda captures, move semantics
- **Code quality** — clang-format (Google-based), clang-tidy, `-Wall -Wextra -Wpedantic`
- **Deprecated in 4.2** — `pdk::Mutex` / `pdk::AutoLock` (replaced by `std::mutex` / `std::scoped_lock`); legacy `Queue<P, T>` (replaced by `Queue<T>`)

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_WITH_POSTGRESQL` | ON | Enable PostgreSQL connection pool |
| `BUILD_TESTING` | OFF | Build GoogleTest tests |

```bash
# Without PostgreSQL
cmake -B build -DBUILD_WITH_POSTGRESQL=OFF

# With tests
cmake -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build
```

## Requirements

- CMake 3.16+
- GCC 13+ / Clang 16+ (C++20 with `<format>` and `<stop_token>`)
- Linux
- PostgreSQL dev libraries (optional, for connection pool)

## Project Structure

```
shared-libs-cpp20/
├── include/          # Public headers (25 .hpp files)
│   ├── jthread.hpp   # Thread, Mutex, Semaphore, AutoLock
│   ├── qthread.hpp   # Event-driven thread with message queue
│   ├── sock.hpp      # Socket hierarchy (TCP, UDP, Unix, Raw)
│   ├── tcpserver.hpp # Multi-threaded TCP server
│   ├── pgpool.hpp    # PostgreSQL connection pool (RAII)
│   ├── logger.hpp    # Async rotating file logger
│   ├── sharedmemory.hpp # POSIX shared memory + IPC queues
│   └── ...
├── src/              # Implementation files (13 .cpp files)
├── test/             # GoogleTest tests
├── CMakeLists.txt
├── .clang-format     # Google-based C++20 formatting
└── .clang-tidy       # Static analysis rules
```

## License

MIT
