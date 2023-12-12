
#include "./io.h"

#include "../util.h"

char *vm_io_vformat(char *fmt, va_list ap) {
    size_t alloc = 16;
    char *buf = vm_malloc(sizeof(char) * alloc);
    size_t len = 0;
    va_list ap_copy;
    while (true) {
        int avail = alloc - len;
        va_copy(ap_copy, ap);
        int written = vsnprintf(&buf[len], avail, fmt, ap_copy);
        va_end(ap_copy);
        if (avail <= written) {
            buf = vm_realloc(buf, sizeof(char) * alloc);
            continue;
        }
        len += written;
        return buf;
    }
}

char *vm_io_format(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *r = vm_io_vformat(fmt, ap);
    va_end(ap);
    return r;
}

char *vm_io_read(const char *filename) {
    void *file = fopen(filename, "rb");
    if (file == NULL) {
        return NULL;
    }
    size_t nalloc = 512;
    char *ops = vm_malloc(sizeof(char) * nalloc);
    size_t nops = 0;
    size_t size;
    for (;;) {
        if (nops + 256 >= nalloc) {
            nalloc = (nops + 256) * 2;
            ops = vm_realloc(ops, sizeof(char) * nalloc);
        }
        size = fread(&ops[nops], 1, 256, file);
        nops += size;
        if (size < 256) {
            break;
        }
    }
    ops[nops] = '\0';
    fclose(file);
    return ops;
}

static void vm_indent(FILE *out, size_t indent, const char *prefix) {
    while (indent-- > 0) {
        fprintf(out, "    ");
    }
    fprintf(out, "%s", prefix);
}

void vm_io_print_lit(FILE *out, vm_std_value_t value) {
    switch (value.tag) {
        case VM_TAG_NIL: {
            fprintf(out, "nil");
            break;
        }
        case VM_TAG_BOOL: {
            fprintf(out, "%s", value.value.b ? "true" : "false");
            break;
        }
        case VM_TAG_I8: {
            fprintf(out, "%" PRIi8, value.value.i8);
            break;
        }
        case VM_TAG_I16: {
            fprintf(out, "%" PRIi16, value.value.i16);
            break;
        }
        case VM_TAG_I32: {
            fprintf(out, "%" PRIi32, value.value.i32);
            break;
        }
        case VM_TAG_I64: {
            fprintf(out, "%" PRIi64, value.value.i64);
            break;
        }
        case VM_TAG_F32: {
            fprintf(out, "%f", value.value.f32);
            break;
        }
        case VM_TAG_F64: {
            fprintf(out, "%f", value.value.f64);
            break;
        }
        case VM_TAG_FFI: {
            fprintf(out, "<function: %p>", value.value.ffi);
            break;
        }
        case VM_TAG_STR: {
            fprintf(out, "\"%s\"", value.value.str);
            break;
        }
    }
}

void vm_io_debug(FILE *out, size_t indent, const char *prefix, vm_std_value_t value, vm_io_debug_t *link) {
    size_t up = 1;
    while (link != NULL) {
        if (value.tag == link->value.tag) {
            if (value.value.all == link->value.value.all) {
                vm_indent(out, indent, prefix);
                fprintf(out, "<ref %zu>\n", up);
                return;
            }
        }
        up += 1;
        link = link->next;
    }
    vm_io_debug_t next = (vm_io_debug_t){
        .next = link,
        .value = value,
    };
    switch (value.tag) {
        case VM_TAG_NIL: {
            vm_indent(out, indent, prefix);
            fprintf(out, "nil\n");
            break;
        }
        case VM_TAG_BOOL: {
            vm_indent(out, indent, prefix);
            if (value.value.b) {
                fprintf(out, "true\n");
            } else {
                fprintf(out, "false\n");
            }
            break;
        }
        case VM_TAG_I8: {
            vm_indent(out, indent, prefix);
            fprintf(out, "%" PRIi8 "\n", value.value.i8);
            break;
        }
        case VM_TAG_I16: {
            vm_indent(out, indent, prefix);
            fprintf(out, "%" PRIi16 "\n", value.value.i16);
            break;
        }
        case VM_TAG_I32: {
            vm_indent(out, indent, prefix);
            fprintf(out, "%" PRIi32 "\n", value.value.i32);
            break;
        }
        case VM_TAG_I64: {
            vm_indent(out, indent, prefix);
            fprintf(out, "%" PRIi64 "\n", value.value.i64);
            break;
        }
        case VM_TAG_F32: {
            vm_indent(out, indent, prefix);
            fprintf(out, "%f (.bits = 0x%"PRIX32")\n", value.value.f32, value.value.i32);
            break;
        }
        case VM_TAG_F64: {
            vm_indent(out, indent, prefix);
            fprintf(out, "%f (.bits = 0x%"PRIX64")\n", value.value.f64, value.value.i64);
            break;
        }
        case VM_TAG_STR: {
            vm_indent(out, indent, prefix);
            fprintf(out, "\"%s\"\n", value.value.str);
            break;
        }
        case VM_TAG_CLOSURE: {
            vm_indent(out, indent, prefix);
            fprintf(out, "<closure: %p>\n", value.value.all);
            break;
        }
        case VM_TAG_FUN: {
            vm_indent(out, indent, prefix);
            fprintf(out, "<code: %p>\n", value.value.all);
            break;
        }
        case VM_TAG_TAB: {
            vm_table_t *tab = value.value.table;
            if (tab == NULL) {
                vm_indent(out, indent, prefix);
                fprintf(out, "table(NULL)\n");
                break;
            }
            vm_indent(out, indent, prefix);
            fprintf(out, "table(%p) {\n", tab);
            size_t len = 1 << tab->alloc;
            for (size_t i = 0; i < len; i++) {
                vm_pair_t p = tab->pairs[i];
                switch (p.key_tag) {
                    case 0:
                    case VM_TAG_NIL: {
                        // vm_std_value_t val = (vm_std_value_t){
                        //     .tag = p.val_tag,
                        //     .value = p.val_val,
                        // };
                        // vm_io_debug(out, indent + 1, "nil = ", val, &next);
                        break;
                    }
                    case VM_TAG_BOOL: {
                        vm_std_value_t val = (vm_std_value_t){
                            .tag = p.val_tag,
                            .value = p.val_val,
                        };
                        if (value.value.b) {
                            vm_io_debug(out, indent + 1, "true = ", val, &next);
                        } else {
                            vm_io_debug(out, indent + 1, "false = ", val, &next);
                        }
                        break;
                    }
                    // case VM_TAG_I64: {
                    //     vm_std_value_t val = (vm_std_value_t){
                    //         .tag = p.val_tag,
                    //         .value = p.val_val,
                    //     };
                    //     char buf[64];
                    //     snprintf(buf, 63, "%" PRIi64 " = ", p.key_val.i64);
                    //     vm_io_debug(out, indent + 1, buf, val, &next);
                    //     break;
                    // }
                    case VM_TAG_I8: {
                        vm_std_value_t val = (vm_std_value_t){
                            .tag = p.val_tag,
                            .value = p.val_val,
                        };
                        char buf[64];
                        snprintf(buf, 63, "%" PRIi8 " = ", p.key_val.i8);
                        vm_io_debug(out, indent + 1, buf, val, &next);
                        break;
                    }
                    case VM_TAG_I16: {
                        vm_std_value_t val = (vm_std_value_t){
                            .tag = p.val_tag,
                            .value = p.val_val,
                        };
                        char buf[64];
                        snprintf(buf, 63, "%" PRIi16 " = ", p.key_val.i16);
                        vm_io_debug(out, indent + 1, buf, val, &next);
                        break;
                    }
                    case VM_TAG_I32: {
                        vm_std_value_t val = (vm_std_value_t){
                            .tag = p.val_tag,
                            .value = p.val_val,
                        };
                        char buf[64];
                        snprintf(buf, 63, "%" PRIi32 " = ", p.key_val.i32);
                        vm_io_debug(out, indent + 1, buf, val, &next);
                        break;
                    }
                    case VM_TAG_I64: {
                        vm_std_value_t val = (vm_std_value_t){
                            .tag = p.val_tag,
                            .value = p.val_val,
                        };
                        char buf[64];
                        snprintf(buf, 63, "%" PRIi64 " = ", p.key_val.i64);
                        vm_io_debug(out, indent + 1, buf, val, &next);
                        break;
                    }
                    case VM_TAG_F64: {
                        vm_std_value_t val = (vm_std_value_t){
                            .tag = p.val_tag,
                            .value = p.val_val,
                        };
                        char buf[64];
                        snprintf(buf, 63, "%f = ", p.key_val.f64);
                        vm_io_debug(out, indent + 1, buf, val, &next);
                        break;
                    }
                    case VM_TAG_STR: {
                        vm_std_value_t val = (vm_std_value_t){
                            .tag = p.val_tag,
                            .value = p.val_val,
                        };
                        char buf[64];
                        snprintf(buf, 63, "%s = ", p.key_val.str);
                        vm_io_debug(out, indent + 1, buf, val, &next);
                        break;
                    }
                    default: {
                        vm_indent(out, indent + 1, "");
                        fprintf(out, "pair {\n");
                        vm_std_value_t key = (vm_std_value_t){
                            .tag = p.key_tag,
                            .value = p.key_val,
                        };
                        vm_io_debug(out, indent + 2, "key = ", key, &next);
                        vm_std_value_t val = (vm_std_value_t){
                            .tag = p.val_tag,
                            .value = p.val_val,
                        };
                        vm_io_debug(out, indent + 2, "val = ", val, &next);
                        vm_indent(out, indent + 1, "");
                        fprintf(out, "}\n");
                    }
                }
            }
            vm_indent(out, indent, "");
            fprintf(out, "}\n");
            break;
        }
        case VM_TAG_FFI: {
            vm_indent(out, indent, prefix);
            fprintf(out, "<function: %p>\n", value.value.all);
            break;
        }
        default: {
            vm_indent(out, indent, prefix);
            fprintf(out, "<T%zu: %p>\n", (size_t)value.tag, value.value.all);
            __builtin_trap();
        }
    }
}
