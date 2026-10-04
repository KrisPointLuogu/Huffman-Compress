# Huffman

A tiny lossless compressor based on Huffman coding. It packs one or more
files and directories into a single `.huf` archive and restores them later.

## Build

```
make
```

This produces the `huffman` executable with `g++ -std=c++11 -O2`.

## Usage

```
huffman c <out.huf> <input1> [input2 ...]
huffman d <in.huf>  <outdir>
```

Compress one or more files or directories:

```
huffman c backup.huf notes.txt photos
```

Decompress into a directory:

```
huffman d backup.huf restored
```

Input paths are stored as given, relative to the current directory. Absolute
paths, `..` components and duplicate archive paths are rejected. Empty
directories are not stored.

## Testing

```
make test
```

`test/roundtrip.sh` builds a variety of inputs, compresses and decompresses
them, then compares content, mode and mtime. It also checks the error paths.

## Archive format

All integers are little endian and fixed width.

```
magic        4 bytes  "HUF1"
version      1 byte
file count   4 bytes
entry * n:
  path len   2 bytes
  path       path len bytes
  size       8 bytes
  mode       4 bytes
  mtime      8 bytes
tree len     8 bytes
tree         tree len bytes, preorder bitstream
data         compressed bitstream, zero padded to a full byte
```

The tree is written in preorder: an internal node is a `1` bit followed by
both children, a leaf is a `0` bit followed by the raw symbol byte. Codes are
emitted most significant bit first. Decoding stops after each entry's stored
size, so trailing padding bits are ignored.

## Limitations

- Works on regular files only; symlinks and special files are skipped.
- Inputs must be relative paths.
- The whole archive header is kept in memory, though file contents are
  streamed in 64 KB chunks.
