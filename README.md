# MM FKZ API Plugin

Exposes API endpoint calls as natives, tracks player's/server's realtime status and tracks messages for crosschat.

This is built to work with [FKZ API](https://github.com/FemboyKZ/api)

## Natives

Other plugins can read the FKZ API through the natives exposed by this plugin.
Include [`include/IFKZApi.h`](include/IFKZApi.h)

## Build

### AMBuild

#### Prerequisites

- This repository is cloned recursively (ie. has submodules)
- [python3](https://www.python.org/)
- [ambuild](https://github.com/alliedmodders/ambuild), make sure `ambuild` command is available via the `PATH` environment variable;
- MSVC (VS build tools)/Clang installed for Windows/Linux.

```bash
mkdir -p build && cd build
python3 ../configure.py --enable-optimize
ambuild
```

### Docker

```bash
docker compose run --rm build
```

## Credits

- [zer0.k's mm sample plugin fork](https://github.com/zer0k-z/mm_misc_plugins)
- [cs2kz-metamod](https://github.com/KZGlobalTeam/cs2kz-metamod)
