#include "jvm.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heap.h"
#include "read_class.h"

/** The name of the method to invoke to run the class file */
const char MAIN_METHOD[] = "main";
/**
 * The "descriptor" string for main(). The descriptor encodes main()'s signature,
 * i.e. main() takes a String[] and returns void.
 * If you're interested, the descriptor string is explained at
 * https://docs.oracle.com/javase/specs/jvms/se12/html/jvms-4.html#jvms-4.3.2.
 */
const char MAIN_DESCRIPTOR[] = "([Ljava/lang/String;)V";

/**
 * Represents the return value of a Java method: either void or an int or a reference.
 * For simplification, we represent a reference as an index into a heap-allocated array.
 * (In a real JVM, methods could also return object references or other primitives.)
 */
typedef struct {
    /** Whether this returned value is an int */
    bool has_value;
    /** The returned value (only valid if `has_value` is true) */
    int32_t value;
} optional_value_t;

/**
 * Runs a method's instructions until the method returns.
 *
 * @param method the method to run
 * @param locals the array of local variables, including the method parameters.
 *   Except for parameters, the locals are uninitialized.
 * @param class the class file the method belongs to
 * @param heap an array of heap-allocated pointers, useful for references
 * @return an optional int containing the method's return value
 */
optional_value_t execute(method_t *method, int32_t *locals, class_file_t *class,
                         heap_t *heap) {
    size_t pc = 0;
    u1 *bytecode = method->code.code;
    int32_t *stack = calloc(method->code.max_stack, sizeof(int32_t));
    size_t idx = 0;
    while (pc < method->code.code_length) {
        u1 instruction = bytecode[pc];
        switch (instruction) {
            default:
                pc += 1;
                break;
            case i_bipush:;
                stack[idx] = (int32_t)(int8_t) bytecode[pc + 1];
                pc += 2;
                idx += 1;
                break;
            case i_iadd:;
                stack[idx - 2] = stack[idx - 1] + stack[idx - 2];
                idx -= 1;
                pc += 1;
                break;
            case i_return:;
                free(stack);
                optional_value_t result = {.has_value = false};
                return result;
            case i_getstatic:;
                pc += 3;
                break;
            case i_invokevirtual:;
                idx -= 1;
                printf("%i\n", stack[idx]);
                pc += 3;
                break;
            case i_iconst_m1:
            case i_iconst_0:
            case i_iconst_1:
            case i_iconst_2:
            case i_iconst_3:
            case i_iconst_4:
            case i_iconst_5:
                stack[idx] = (int32_t)(int8_t)(instruction - i_iconst_0);
                idx += 1;
                pc += 1;
                break;
            case i_sipush:;
                stack[idx] = (signed short) ((bytecode[pc + 1] << 8) | bytecode[pc + 2]);
                idx += 1;
                pc += 3;
                break;
            case i_isub:;
                stack[idx - 2] = stack[idx - 2] - stack[idx - 1];
                idx -= 1;
                pc += 1;
                break;
            case i_imul:;
                stack[idx - 2] = stack[idx - 1] * stack[idx - 2];
                idx -= 1;
                pc += 1;
                break;
            case i_idiv:;
                assert(stack[idx - 1] != 0);
                stack[idx - 2] = stack[idx - 2] / stack[idx - 1];
                idx -= 1;
                pc += 1;
                break;
            case i_irem:;
                assert(stack[idx - 1] != 0);
                stack[idx - 2] = stack[idx - 2] % stack[idx - 1];
                idx -= 1;
                pc += 1;
                break;
            case i_ineg:;
                stack[idx - 1] = -1 * stack[idx - 1];
                pc += 1;
                break;
            case i_ishl:;
                assert(stack[idx - 1] >= 0);
                stack[idx - 2] = stack[idx - 2] << stack[idx - 1];
                idx -= 1;
                pc += 1;
                break;
            case i_ishr:;
                assert(stack[idx - 1] >= 0);
                stack[idx - 2] = stack[idx - 2] >> stack[idx - 1];
                idx -= 1;
                pc += 1;
                break;
            case i_iushr:;
                assert(stack[idx - 1] >= 0);
                stack[idx - 2] = (u4) stack[idx - 2] >> stack[idx - 1];
                idx -= 1;
                pc += 1;
                break;
            case i_iand:;
                stack[idx - 2] = stack[idx - 1] & stack[idx - 2];
                idx -= 1;
                pc += 1;
                break;
            case i_ior:;
                stack[idx - 2] = stack[idx - 1] | stack[idx - 2];
                idx -= 1;
                pc += 1;
                break;
            case i_ixor:;
                stack[idx - 2] = stack[idx - 1] ^ stack[idx - 2];
                idx -= 1;
                pc += 1;
                break;
            case i_iload:;
                stack[idx] = locals[bytecode[pc + 1]];
                pc += 2;
                idx += 1;
                break;
            case i_istore:;
                locals[bytecode[pc + 1]] = stack[idx - 1];
                idx -= 1;
                pc += 2;
                break;
            case i_iinc:;
                locals[bytecode[pc + 1]] += (int8_t) bytecode[pc + 2];
                pc += 3;
                break;
            case i_iload_0:
            case i_iload_1:
            case i_iload_2:
            case i_iload_3:
                stack[idx] = locals[instruction - i_iload_0];
                pc += 1;
                idx += 1;
                break;
            case i_istore_0:
            case i_istore_1:
            case i_istore_2:
            case i_istore_3:
                locals[instruction - i_istore_0] = stack[idx - 1];
                idx -= 1;
                pc += 1;
                break;
            case i_ldc:;
                stack[idx] =
                    ((CONSTANT_Integer_info *) class->constant_pool[bytecode[pc + 1] - 1]
                         .info)
                        ->bytes;
                pc += 2;
                idx += 1;
                break;
            case i_ifeq:;
                if (stack[idx - 1] == 0) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 1;
                    break;
                }
                pc += 3;
                idx -= 1;
                break;
            case i_ifne:;
                if (stack[idx - 1] != 0) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 1;
                    break;
                }
                pc += 3;
                idx -= 1;
                break;
            case i_iflt:;
                if (stack[idx - 1] < 0) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 1;
                    break;
                }
                pc += 3;
                idx -= 1;
                break;
            case i_ifge:;
                if (stack[idx - 1] >= 0) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 1;
                    break;
                }
                pc += 3;
                idx -= 1;
                break;
            case i_ifgt:;
                if (stack[idx - 1] > 0) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 1;
                    break;
                }
                pc += 3;
                idx -= 1;
                break;
            case i_ifle:;
                if (stack[idx - 1] <= 0) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 1;
                    break;
                }
                pc += 3;
                idx -= 1;
                break;
            case i_if_icmpeq:;
                if (stack[idx - 1] == stack[idx - 2]) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 2;
                    break;
                }
                pc += 3;
                idx -= 2;
                break;
            case i_if_icmpne:;
                if (stack[idx - 1] != stack[idx - 2]) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 2;
                    break;
                }
                pc += 3;
                idx -= 2;
                break;
            case i_if_icmplt:;
                if (stack[idx - 2] < stack[idx - 1]) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 2;
                    break;
                }
                pc += 3;
                idx -= 2;
                break;
            case i_if_icmpge:;
                if (stack[idx - 2] >= stack[idx - 1]) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 2;
                    break;
                }
                pc += 3;
                idx -= 2;
                break;
            case i_if_icmpgt:;
                if (stack[idx - 2] > stack[idx - 1]) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 2;
                    break;
                }
                pc += 3;
                idx -= 2;
                break;
            case i_if_icmple:;
                if (stack[idx - 2] <= stack[idx - 1]) {
                    pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                    idx -= 2;
                    break;
                }
                pc += 3;
                idx -= 2;
                break;
            case i_goto:;
                pc += (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]);
                break;
            case i_ireturn:;
                optional_value_t conditional_result = {.has_value = true,
                                                       .value = stack[idx - 1]};
                free(stack);
                return conditional_result;
            case i_invokestatic:;
                method_t *method_call = find_method_from_index(
                    (int16_t)(bytecode[pc + 1] << 8 | bytecode[pc + 2]), class);
                int32_t *local_queue =
                    calloc((method_call->code.max_locals), sizeof(int32_t));
                // stack to queue means highest idx in stack goes to lowest idx in queue
                for (int32_t i = get_number_of_parameters(method_call) - 1; i > -1; i--) {
                    local_queue[i] = stack[idx - 1];
                    idx -= 1;
                }
                optional_value_t method_call_result =
                    execute(method_call, local_queue, class, heap);
                if (method_call_result.has_value) {
                    stack[idx] = method_call_result.value;
                    idx += 1;
                }
                pc += 3;
                free(local_queue);
                break;
            case i_nop:;
                pc += 1;
                break;
            case i_dup:;
                stack[idx] = stack[idx - 1];
                idx += 1;
                pc += 1;
                break;
            case i_newarray:;
                assert(stack[idx - 1] >= 0);
                int32_t *new_arr = calloc(stack[idx - 1] + 1, sizeof(int32_t));
                // stores the length in the first idx
                new_arr[0] = stack[idx - 1];
                // initialize to default type, int32_t = 0
                for (int32_t i = 1; i <= stack[idx - 1]; i++) {
                    new_arr[i] = 0;
                }
                int32_t reference = heap_add(heap, new_arr);
                stack[idx - 1] = reference;
                pc += 2;
                break;
            case i_arraylength:;
                stack[idx - 1] = heap_get(heap, stack[idx - 1])[0];
                pc += 1;
                break;
            case i_areturn:;
                optional_value_t reference_result = {.has_value = true,
                                                     .value = stack[idx - 1]};
                free(stack);
                return reference_result;
            case i_iastore:;
                int32_t *store_arr = heap_get(heap, stack[idx - 3]);
                store_arr[stack[idx - 2] + 1] = stack[idx - 1];
                pc += 1;
                idx -= 3;
                break;
            case i_iaload:;
                stack[idx - 2] = heap_get(heap, stack[idx - 2])[stack[idx - 1] + 1];
                idx -= 1;
                pc += 1;
                break;
            case i_aload:;
                stack[idx] = locals[bytecode[pc + 1]];
                pc += 2;
                idx += 1;
                break;
            case i_astore:;
                locals[bytecode[pc + 1]] = stack[idx - 1];
                pc += 2;
                idx -= 1;
                break;
            case i_aload_0:
            case i_aload_1:
            case i_aload_2:
            case i_aload_3:
                stack[idx] = locals[instruction - i_aload_0];
                pc += 1;
                idx += 1;
                break;
            case i_astore_0:
            case i_astore_1:
            case i_astore_2:
            case i_astore_3:
                locals[instruction - i_astore_0] = stack[idx - 1];
                pc += 1;
                idx -= 1;
                break;
        }
    }
    free(stack);
    optional_value_t result = {.has_value = false};
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <class file>\n", argv[0]);
        return 1;
    }

    // Open the class file for reading
    FILE *class_file = fopen(argv[1], "r");
    assert(class_file != NULL && "Failed to open file");

    // Parse the class file
    class_file_t *class = get_class(class_file);
    int error = fclose(class_file);
    assert(error == 0 && "Failed to close file");

    // The heap array is initially allocated to hold zero elements.
    heap_t *heap = heap_init();

    // Execute the main method
    method_t *main_method = find_method(MAIN_METHOD, MAIN_DESCRIPTOR, class);
    assert(main_method != NULL && "Missing main() method");
    /* In a real JVM, locals[0] would contain a reference to String[] args.
     * But since TeenyJVM doesn't support Objects, we leave it uninitialized. */
    int32_t locals[main_method->code.max_locals];
    // Initialize all local variables to 0
    memset(locals, 0, sizeof(locals));
    optional_value_t result = execute(main_method, locals, class, heap);
    assert(!result.has_value && "main() should return void");

    // Free the internal data structures
    free_class(class);

    // Free the heap
    heap_free(heap);
}
