# User namespaces example

A simple rust multi-call program to start up a program in a new user namespace that maps to root

```
cargo build
ln -s $(pwd)/target/debug/userns-example parent
./parent
```
or 
```
cargo run -- parent
```