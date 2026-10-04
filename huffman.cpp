#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <vector>
#include <queue>
#include <string>
#include <set>
#include <sys/stat.h>
#include <dirent.h>
#include <utime.h>

const size_t IO_BUF_SIZE = 65536;

struct Entry {
    std::string path;
    unsigned long long size;
    unsigned int mode;
    long long mtime;
    unsigned int flags;
};

struct Node {
    long long freq;
    int symbol;
    Node *left;
    Node *right;
};

bool node_cmp(const Node *a, const Node *b)
{
    if (a->freq != b->freq)
        return a->freq > b->freq;
    return a->symbol > b->symbol;
}

Node *new_node(std::vector<Node> &pool, long long freq, int symbol,
               Node *left, Node *right)
{
    Node n;
    n.freq = freq;
    n.symbol = symbol;
    n.left = left;
    n.right = right;
    pool.push_back(n);
    return &pool.back();
}

Node *build_tree(const long long freq[256], std::vector<Node> &pool)
{
    std::priority_queue<Node *, std::vector<Node *>,
                        bool (*)(const Node *, const Node *)> pq(node_cmp);
    for (int i = 0; i < 256; ++i)
        if (freq[i] > 0)
            pq.push(new_node(pool, freq[i], i, 0, 0));

    if (pq.empty())
        return 0;

    while (pq.size() > 1) {
        Node *a = pq.top();
        pq.pop();
        Node *b = pq.top();
        pq.pop();
        pq.push(new_node(pool, a->freq + b->freq, -1, a, b));
    }
    return pq.top();
}

void gen_codes(const Node *n, const std::string &prefix, std::string codes[256])
{
    if (!n)
        return;
    if (!n->left && !n->right) {
        codes[n->symbol] = prefix.empty() ? std::string("0") : prefix;
        return;
    }
    gen_codes(n->left, prefix + "0", codes);
    gen_codes(n->right, prefix + "1", codes);
}

struct BitWriter {
    FILE *out;
    unsigned char cur;
    int used;
    unsigned char buf[IO_BUF_SIZE];
    size_t len;
    size_t total;
};

void bw_init(BitWriter *w, FILE *out)
{
    w->out = out;
    w->cur = 0;
    w->used = 0;
    w->len = 0;
    w->total = 0;
}

bool bw_flush_buffer(BitWriter *w)
{
    if (w->len > 0) {
        if (fwrite(w->buf, 1, w->len, w->out) != w->len)
            return false;
        w->len = 0;
    }
    return true;
}

bool bw_put_raw(BitWriter *w, unsigned char b)
{
    w->buf[w->len++] = b;
    w->total++;
    if (w->len == IO_BUF_SIZE)
        return bw_flush_buffer(w);
    return true;
}

bool bw_put_bit(BitWriter *w, int bit)
{
    w->cur = (unsigned char)((w->cur << 1) | (bit & 1));
    if (++w->used == 8) {
        bool ok = bw_put_raw(w, w->cur);
        w->cur = 0;
        w->used = 0;
        return ok;
    }
    return true;
}

bool bw_put_byte(BitWriter *w, unsigned char b)
{
    for (int i = 7; i >= 0; --i)
        if (!bw_put_bit(w, (b >> i) & 1))
            return false;
    return true;
}

bool bw_flush(BitWriter *w)
{
    if (w->used > 0) {
        if (!bw_put_raw(w, (unsigned char)(w->cur << (8 - w->used))))
            return false;
        w->cur = 0;
        w->used = 0;
    }
    return bw_flush_buffer(w);
}

struct BitReader {
    FILE *in;
    unsigned char cur;
    int left;
    unsigned char buf[IO_BUF_SIZE];
    size_t pos;
    size_t len;
    bool eof;
};

void br_init(BitReader *r, FILE *in)
{
    r->in = in;
    r->cur = 0;
    r->left = 0;
    r->pos = 0;
    r->len = 0;
    r->eof = false;
}

int br_get_bit(BitReader *r)
{
    if (r->left == 0) {
        if (r->pos >= r->len) {
            r->len = fread(r->buf, 1, IO_BUF_SIZE, r->in);
            r->pos = 0;
            if (r->len == 0) {
                r->eof = true;
                return -1;
            }
        }
        r->cur = r->buf[r->pos++];
        r->left = 8;
    }
    r->left--;
    return (r->cur >> r->left) & 1;
}

void br_align(BitReader *r)
{
    r->left = 0;
}

std::string normalize_path(const std::string &in)
{
    std::string p = in;
    while (p.size() >= 2 && p[0] == '.' && p[1] == '/')
        p.erase(0, 2);
    while (!p.empty() && p[p.size() - 1] == '/')
        p.erase(p.size() - 1);
    return p;
}

bool path_is_safe(const std::string &p)
{
    if (p.empty() || p[0] == '/')
        return false;
    size_t i = 0;
    while (i < p.size()) {
        size_t j = p.find('/', i);
        if (j == std::string::npos)
            j = p.size();
        if (p.compare(i, j - i, "..") == 0)
            return false;
        i = j + 1;
    }
    return true;
}

bool stat_entry(const std::string &path, Entry &e)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    e.path = normalize_path(path);
    e.size = (unsigned long long)st.st_size;
    e.mode = (unsigned int)st.st_mode;
    e.mtime = (long long)st.st_mtime;
    e.flags = 0;
    return true;
}

bool write_u8(FILE *f, unsigned int v)
{
    unsigned char b = (unsigned char)(v & 0xff);
    return fwrite(&b, 1, 1, f) == 1;
}

bool write_u16(FILE *f, unsigned int v)
{
    unsigned char b[2];
    b[0] = (unsigned char)(v & 0xff);
    b[1] = (unsigned char)((v >> 8) & 0xff);
    return fwrite(b, 1, 2, f) == 2;
}

bool write_u32(FILE *f, unsigned long long v)
{
    unsigned char b[4];
    for (int i = 0; i < 4; ++i)
        b[i] = (unsigned char)((v >> (8 * i)) & 0xff);
    return fwrite(b, 1, 4, f) == 4;
}

bool write_u64(FILE *f, unsigned long long v)
{
    unsigned char b[8];
    for (int i = 0; i < 8; ++i)
        b[i] = (unsigned char)((v >> (8 * i)) & 0xff);
    return fwrite(b, 1, 8, f) == 8;
}

bool read_u8(FILE *f, unsigned int *v)
{
    unsigned char b;
    if (fread(&b, 1, 1, f) != 1)
        return false;
    *v = b;
    return true;
}

bool read_u16(FILE *f, unsigned int *v)
{
    unsigned char b[2];
    if (fread(b, 1, 2, f) != 2)
        return false;
    *v = (unsigned int)b[0] | ((unsigned int)b[1] << 8);
    return true;
}

bool read_u32(FILE *f, unsigned long long *v)
{
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4)
        return false;
    unsigned long long r = 0;
    for (int i = 0; i < 4; ++i)
        r |= (unsigned long long)b[i] << (8 * i);
    *v = r;
    return true;
}

bool read_u64(FILE *f, unsigned long long *v)
{
    unsigned char b[8];
    if (fread(b, 1, 8, f) != 8)
        return false;
    unsigned long long r = 0;
    for (int i = 0; i < 8; ++i)
        r |= (unsigned long long)b[i] << (8 * i);
    *v = r;
    return true;
}

bool write_entry(FILE *f, const Entry &e)
{
    if (e.path.size() > 65535)
        return false;
    if (!write_u16(f, (unsigned int)e.path.size()))
        return false;
    if (e.path.size() > 0 &&
        fwrite(e.path.data(), 1, e.path.size(), f) != e.path.size())
        return false;
    if (!write_u64(f, e.size))
        return false;
    if (!write_u32(f, e.mode))
        return false;
    if (!write_u64(f, (unsigned long long)e.mtime))
        return false;
    if (!write_u8(f, e.flags & 0xff))
        return false;
    return true;
}

bool read_entry(FILE *f, Entry &e)
{
    unsigned int plen = 0;
    if (!read_u16(f, &plen))
        return false;
    e.path.assign(plen, '\0');
    if (plen > 0 && fread(&e.path[0], 1, plen, f) != plen)
        return false;
    if (!read_u64(f, &e.size))
        return false;
    unsigned long long mode = 0;
    if (!read_u32(f, &mode))
        return false;
    e.mode = (unsigned int)mode;
    unsigned long long mtime = 0;
    if (!read_u64(f, &mtime))
        return false;
    e.mtime = (long long)mtime;
    unsigned int flags = 0;
    if (!read_u8(f, &flags))
        return false;
    e.flags = flags;
    return true;
}

void write_tree(BitWriter *w, const Node *n)
{
    if (!n)
        return;
    if (!n->left && !n->right) {
        bw_put_bit(w, 0);
        bw_put_byte(w, (unsigned char)n->symbol);
        return;
    }
    bw_put_bit(w, 1);
    write_tree(w, n->left);
    write_tree(w, n->right);
}

int br_get_byte(BitReader *r)
{
    int v = 0;
    for (int i = 0; i < 8; ++i) {
        int b = br_get_bit(r);
        if (b < 0)
            return -1;
        v = (v << 1) | b;
    }
    return v;
}

Node *read_tree(BitReader *r, std::vector<Node> &pool)
{
    int bit = br_get_bit(r);
    if (bit < 0)
        return 0;
    if (bit == 0) {
        int sym = br_get_byte(r);
        if (sym < 0)
            return 0;
        return new_node(pool, 0, sym, 0, 0);
    }
    Node *left = read_tree(r, pool);
    if (!left)
        return 0;
    Node *right = read_tree(r, pool);
    if (!right)
        return 0;
    return new_node(pool, 0, -1, left, right);
}

bool collect_input(const std::string &path, std::vector<Entry> &entries)
{
    struct stat st;
    if (lstat(path.c_str(), &st) != 0)
        return false;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path.c_str());
        if (!d)
            return false;
        struct dirent *de;
        bool ok = true;
        while ((de = readdir(d)) != 0) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            std::string child = path + "/" + de->d_name;
            if (!collect_input(child, entries)) {
                ok = false;
                break;
            }
        }
        closedir(d);
        return ok;
    }
    if (S_ISREG(st.st_mode)) {
        Entry e;
        if (!stat_entry(path, e))
            return false;
        entries.push_back(e);
        return true;
    }
    return true;
}

void freq_sink(void *ctx, int symbol)
{
    long long *freq = (long long *)ctx;
    freq[symbol]++;
}

struct EncodeCtx {
    BitWriter *w;
    const std::string *codes;
    bool ok;
};

void encode_sink(void *ctx, int symbol)
{
    EncodeCtx *e = (EncodeCtx *)ctx;
    if (!e->ok)
        return;
    const std::string &c = e->codes[symbol];
    for (size_t k = 0; k < c.size(); ++k)
        if (!bw_put_bit(e->w, c[k] == '1')) {
            e->ok = false;
            return;
        }
}

typedef void (*SymbolSink)(void *ctx, int symbol);

bool count_runs(const std::string &path, unsigned long long *runs,
                unsigned long long *bytes)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    unsigned char buf[IO_BUF_SIZE];
    size_t n;
    int cur = -1;
    unsigned long long total = 0, rc = 0;
    bool ok = true;
    while ((n = fread(buf, 1, IO_BUF_SIZE, f)) > 0) {
        for (size_t i = 0; i < n; ++i) {
            total++;
            if ((int)buf[i] != cur) {
                cur = buf[i];
                rc++;
            }
        }
    }
    if (ferror(f))
        ok = false;
    fclose(f);
    *runs = rc;
    *bytes = total;
    return ok;
}

bool walk_file(const std::string &path, bool use_rle, void *ctx, SymbolSink sink)
{
    FILE *f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    unsigned char buf[IO_BUF_SIZE];
    size_t n;
    int cur = -1;
    unsigned long long count = 0;
    bool ok = true;
    while ((n = fread(buf, 1, IO_BUF_SIZE, f)) > 0) {
        for (size_t i = 0; i < n; ++i) {
            int b = buf[i];
            if (!use_rle) {
                sink(ctx, b);
            } else if (count == 0) {
                cur = b;
                count = 1;
            } else if (b == cur && count < 255) {
                count++;
            } else {
                sink(ctx, cur);
                sink(ctx, (int)count);
                cur = b;
                count = 1;
            }
        }
    }
    if (use_rle && count > 0) {
        sink(ctx, cur);
        sink(ctx, (int)count);
    }
    if (ferror(f))
        ok = false;
    fclose(f);
    return ok;
}

int pack(const char *out_path, int n_inputs, char **inputs)
{
    std::vector<Entry> entries;
    std::set<std::string> seen;
    for (int i = 0; i < n_inputs; ++i) {
        std::string p = normalize_path(inputs[i]);
        if (!path_is_safe(p)) {
            fprintf(stderr, "huffman: unsafe input path: %s\n", inputs[i]);
            return 1;
        }
        std::vector<Entry> local;
        if (!collect_input(p, local)) {
            fprintf(stderr, "huffman: cannot read input: %s\n", inputs[i]);
            return 1;
        }
        for (size_t k = 0; k < local.size(); ++k) {
            if (!seen.insert(local[k].path).second) {
                fprintf(stderr, "huffman: duplicate archive path: %s\n",
                        local[k].path.c_str());
                return 1;
            }
            entries.push_back(local[k]);
        }
    }

    for (size_t k = 0; k < entries.size(); ++k) {
        unsigned long long runs = 0, bytes = 0;
        if (!count_runs(entries[k].path, &runs, &bytes)) {
            fprintf(stderr, "huffman: cannot read input: %s\n",
                    entries[k].path.c_str());
            return 1;
        }
        entries[k].flags = (2 * runs < bytes) ? 1u : 0u;
    }

    long long freq[256];
    for (int i = 0; i < 256; ++i)
        freq[i] = 0;
    for (size_t k = 0; k < entries.size(); ++k)
        if (!walk_file(entries[k].path, entries[k].flags & 1, freq, freq_sink)) {
            fprintf(stderr, "huffman: cannot read input: %s\n",
                    entries[k].path.c_str());
            return 1;
        }

    std::vector<Node> pool;
    pool.reserve(512);
    Node *root = build_tree(freq, pool);
    std::string codes[256];
    if (root)
        gen_codes(root, "", codes);

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "huffman: cannot open output: %s\n", out_path);
        return 1;
    }

    bool ok = true;
    ok = ok && fwrite("HUF1", 1, 4, out) == 4;
    ok = ok && write_u8(out, 2);
    ok = ok && write_u32(out, (unsigned long long)entries.size());
    for (size_t k = 0; ok && k < entries.size(); ++k)
        ok = write_entry(out, entries[k]);

    long tree_len_pos = 0, tree_start = 0, tree_end = 0;
    BitWriter w;
    if (ok) {
        tree_len_pos = ftell(out);
        ok = write_u64(out, 0);
        tree_start = ftell(out);
        bw_init(&w, out);
        if (root)
            write_tree(&w, root);
        ok = ok && bw_flush(&w);
        tree_end = ftell(out);
    }
    if (ok) {
        long tree_len = tree_end - tree_start;
        fseek(out, tree_len_pos, SEEK_SET);
        ok = write_u64(out, (unsigned long long)tree_len);
        fseek(out, tree_end, SEEK_SET);
    }
    if (ok) {
        bw_init(&w, out);
        EncodeCtx ec;
        ec.w = &w;
        ec.codes = codes;
        ec.ok = true;
        for (size_t k = 0; ok && k < entries.size(); ++k) {
            if (!walk_file(entries[k].path, entries[k].flags & 1, &ec, encode_sink))
                ok = false;
            if (!ec.ok)
                ok = false;
        }
        ok = ok && bw_flush(&w);
    }
    if (fclose(out) != 0)
        ok = false;
    if (!ok) {
        remove(out_path);
        fprintf(stderr, "huffman: write error\n");
        return 1;
    }

    unsigned long long original = 0;
    for (size_t k = 0; k < entries.size(); ++k)
        original += entries[k].size;
    struct stat st;
    unsigned long long compressed = 0;
    if (stat(out_path, &st) == 0)
        compressed = (unsigned long long)st.st_size;
    double ratio = original ? (double)compressed / (double)original : 0.0;
    fprintf(stderr, "huffman: %llu files, %llu bytes -> %llu bytes (ratio %.3f)\n",
            (unsigned long long)entries.size(), original, compressed, ratio);
    return 0;
}

bool make_dirs(const std::string &path)
{
    size_t pos = 0;
    while (true) {
        size_t slash = path.find('/', pos);
        if (slash == std::string::npos)
            break;
        std::string dir = path.substr(0, slash);
        if (!dir.empty() && mkdir(dir.c_str(), 0777) != 0 && errno != EEXIST)
            return false;
        pos = slash + 1;
    }
    return true;
}

int decode_symbol(BitReader *r, const Node *root)
{
    if (!root)
        return -1;
    if (!root->left && !root->right) {
        if (br_get_bit(r) < 0)
            return -1;
        return root->symbol;
    }
    const Node *n = root;
    while (n->left || n->right) {
        int b = br_get_bit(r);
        if (b < 0)
            return -1;
        n = b ? n->right : n->left;
    }
    return n->symbol;
}

bool push_byte(unsigned char *buf, size_t *used, unsigned char b, FILE *out)
{
    buf[(*used)++] = b;
    if (*used == IO_BUF_SIZE) {
        if (fwrite(buf, 1, *used, out) != *used)
            return false;
        *used = 0;
    }
    return true;
}

bool decode_stream(BitReader *r, const Node *root, unsigned long long size,
                   bool use_rle, FILE *out)
{
    if (!root)
        return size == 0;
    unsigned char buf[IO_BUF_SIZE];
    size_t used = 0;
    bool ok = true;
    if (!use_rle) {
        for (unsigned long long i = 0; ok && i < size; ++i) {
            int sym = decode_symbol(r, root);
            if (sym < 0)
                ok = false;
            else if (!push_byte(buf, &used, (unsigned char)sym, out))
                ok = false;
        }
    } else {
        unsigned long long produced = 0;
        while (ok && produced < size) {
            int sym = decode_symbol(r, root);
            int cnt = (sym < 0) ? -1 : decode_symbol(r, root);
            if (sym < 0 || cnt < 1 ||
                produced + (unsigned long long)cnt > size) {
                ok = false;
                break;
            }
            for (int i = 0; i < cnt && ok; ++i)
                if (!push_byte(buf, &used, (unsigned char)sym, out))
                    ok = false;
            produced += (unsigned long long)cnt;
        }
    }
    if (ok && used > 0 && fwrite(buf, 1, used, out) != used)
        ok = false;
    return ok;
}

int unpack(const char *in_path, const char *out_dir)
{
    FILE *in = fopen(in_path, "rb");
    if (!in) {
        fprintf(stderr, "huffman: cannot open input: %s\n", in_path);
        return 1;
    }

    char magic[4];
    unsigned int version = 0;
    unsigned long long count = 0;
    bool ok = true;
    ok = ok && fread(magic, 1, 4, in) == 4 && memcmp(magic, "HUF1", 4) == 0;
    ok = ok && read_u8(in, &version);
    if (!ok) {
        fclose(in);
        fprintf(stderr, "huffman: bad archive header: %s\n", in_path);
        return 1;
    }
    if (version != 2) {
        fclose(in);
        fprintf(stderr, "huffman: unsupported version %u: %s\n", version,
                in_path);
        return 1;
    }
    if (!read_u32(in, &count)) {
        fclose(in);
        fprintf(stderr, "huffman: bad archive header: %s\n", in_path);
        return 1;
    }

    std::vector<Entry> entries(count);
    for (unsigned long long i = 0; ok && i < count; ++i)
        ok = read_entry(in, entries[i]);
    for (unsigned long long i = 0; ok && i < count; ++i)
        if (!path_is_safe(entries[i].path)) {
            fprintf(stderr, "huffman: unsafe archive path: %s\n",
                    entries[i].path.c_str());
            ok = false;
        }
    unsigned long long tree_len = 0;
    ok = ok && read_u64(in, &tree_len);
    if (!ok) {
        fclose(in);
        fprintf(stderr, "huffman: truncated archive: %s\n", in_path);
        return 1;
    }

    std::vector<Node> pool;
    pool.reserve(512);
    Node *root = 0;
    BitReader r;
    br_init(&r, in);
    if (tree_len > 0) {
        root = read_tree(&r, pool);
        if (!root) {
            fclose(in);
            fprintf(stderr, "huffman: corrupt tree: %s\n", in_path);
            return 1;
        }
        br_align(&r);
    }

    for (unsigned long long i = 0; ok && i < count; ++i) {
        std::string full = std::string(out_dir) + "/" + entries[i].path;
        if (!make_dirs(full)) {
            fprintf(stderr, "huffman: cannot create directory for: %s\n",
                    full.c_str());
            ok = false;
            break;
        }
        FILE *out = fopen(full.c_str(), "wb");
        if (!out) {
            fprintf(stderr, "huffman: cannot create file: %s\n", full.c_str());
            ok = false;
            break;
        }
        ok = decode_stream(&r, root, entries[i].size,
                           (entries[i].flags & 1) != 0, out);
        if (fclose(out) != 0)
            ok = false;
        if (!ok) {
            fprintf(stderr, "huffman: corrupt data at: %s\n",
                    entries[i].path.c_str());
            break;
        }
        chmod(full.c_str(), entries[i].mode & 07777);
        struct utimbuf ut;
        ut.actime = (time_t)entries[i].mtime;
        ut.modtime = (time_t)entries[i].mtime;
        utime(full.c_str(), &ut);
    }

    if (fclose(in) != 0)
        ok = false;
    if (!ok)
        return 1;
    return 0;
}

static void print_usage(const char *prog)
{
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s c <out.huf> <input1> [input2 ...]\n", prog);
    fprintf(stderr, "  %s d <in.huf> <outdir>\n", prog);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }
    std::string cmd = argv[1];
    if (cmd == "c") {
        if (argc < 4) {
            print_usage(argv[0]);
            return 2;
        }
        return pack(argv[2], argc - 3, argv + 3);
    }
    if (cmd == "d") {
        if (argc != 4) {
            print_usage(argv[0]);
            return 2;
        }
        return unpack(argv[2], argv[3]);
    }
    print_usage(argv[0]);
    return 2;
}
