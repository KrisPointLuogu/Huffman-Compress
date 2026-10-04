#include <cstdio>
#include <cstddef>
#include <vector>
#include <queue>
#include <string>

const size_t IO_BUF_SIZE = 65536;

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
    fprintf(stderr, "huffman: not implemented yet\n");
    return 1;
}
