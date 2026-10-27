#include <assert.h>
#include <stdio.h>
#include "../runtime/nvn/vulkan/precise_vertex.h"

int main(int argc, char **argv) {
    if (argc == 3) {
        FILE *f = fopen(argv[2], "rb"); assert(f);
        assert(!fseek(f, 0, SEEK_END)); long length = ftell(f); assert(length > 0);
        rewind(f); char *data = malloc((size_t)length + 1); assert(data);
        assert(fread(data, 1, length, f) == (size_t)length); fclose(f); data[length] = 0;
        if (!strcmp(argv[1], "source")) {
            char *qualified = precise_vertex_source(data); assert(qualified);
            fputs(qualified, stdout); free(qualified);
        } else {
            assert(!strcmp(argv[1], "check"));
            bool ok = !(length % 4) && precise_vertex_protected((uint32_t *)data, length / 4);
            free(data); return ok ? 0 : 1;
        }
        free(data); return 0;
    }
    char *s = precise_vertex_source("#version 460\nlayout(location=0) in vec4 p;\nlayout(location=0) out vec4 c;\n\tuint r0;\nfloat f(uint x) { return uintBitsToFloat(x); }\nprecise uint r1;\n// float comment\n");
    assert(s && strstr(s, "#version 460\nlayout(location=0) in vec4 p;\nprecise layout(location=0) out vec4 c;\n\tprecise uint r0;\nprecise float f("));
    assert(!strstr(s, "precise precise")); free(s);
    uint32_t words[] = {0x07230203, 0x00010300, 0, 10, 0,
        (3u << 16) | 71, 3, 42, (5u << 16) | 133, 1, 3, 4, 5};
    assert(precise_vertex_protected(words, sizeof words / 4));
    words[7] = 0; assert(!precise_vertex_protected(words, sizeof words / 4));
    words[7] = 42; words[10] = 10; assert(!precise_vertex_protected(words, sizeof words / 4));
    words[10] = 3; words[8] = (9u << 16) | 133;
    assert(!precise_vertex_protected(words, sizeof words / 4));
    assert(!precise_vertex_protected(NULL, 0));
    puts("Precision source transformation and malformed SPIR-V guards passed");
}
