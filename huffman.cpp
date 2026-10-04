#include <cstdio>

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
