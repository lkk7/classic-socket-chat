# classic-socket-chat

A primitive chat server that lets multiple clients connect to a server and chat.
It's rather a quick proof of concept – this was just a way to refresh my knowledge about sockets and put it somewhere.
A guide that helped me: [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/).
It uses sockets and `poll()` to handle events – it's not suitable for Windows.

- Not safe.
- Not efficient.
- Not suitable for any professional usage.
- The code is weird. Sockets and `poll` introduce C-style code and it wasn't my main objective to fully wrap around it with C++ features, but I did mix these two styles.
- Does NOT handle more connections than the `poll` size allows.

## Installation and usage

```
mkdir build && cd build
cmake ..
make
```

Now there are `server` and `client` executables (`client` should be run with server's address as the argument)
