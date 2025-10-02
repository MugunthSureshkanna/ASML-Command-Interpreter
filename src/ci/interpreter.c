#include "interpreter.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "command_type.h"
#include "mem.h"

static bool    cond_holds(Interpreter *intr, BranchCondition cond);
static int64_t fetch_number_value(Interpreter *intr, Operand *op, bool is_im);
static bool    print_base(Interpreter *intr, Command *cmd);
// Function for binary conversion
static void to_binary_string(uint64_t num, char *bit_string, size_t bit_string_size);

void interpreter_init(Interpreter *intr, LabelMap *map) {
    if (!intr) {
        return;
    }

    intr->had_error  = false;
    intr->label_map  = map;
    intr->is_greater = false;
    intr->is_equal   = false;
    intr->is_less    = false;
    intr->the_stack  = NULL;

    for (size_t i = 0; i < NUM_VARIABLES; i++) {
        intr->variables[i] = 0;
    }
}

void interpret(Interpreter *intr, Command *commands) {
    if (!intr || !commands) {
        return;
    }

    Command *current = commands;
    while (current && !intr->had_error) {
        bool jumped = false;
        switch (current->type) {
            // STUDENT TODO: process the commands and take actions as appropriate
            case CMD_MOV: {
                int64_t value = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                if (intr->had_error) {
                    break;
                }
                intr->variables[current->destination.num_val] = value;
                break;
            }
            case CMD_ADD:
            case CMD_SUB: {
                uint64_t a = (uint64_t)fetch_number_value(intr, &current->val_a, false);
                uint64_t b = (uint64_t)fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                if (intr->had_error) {
                    break;
                }
                uint64_t result;
                if (current->type == CMD_ADD) {
                    result = a + b; 
                } else {
                    result = a - b; 
                }
                int64_t dest_index = current->destination.num_val;
                if (dest_index < 0 || dest_index >= NUM_VARIABLES) {
                    intr->had_error = true; 
                    break;
                }
                intr->variables[dest_index] = (int64_t)result; 
                break;
            }
            case CMD_CMP:
            case CMD_CMP_U: {
                int64_t a = fetch_number_value(intr, &current->val_a, false);
                int64_t b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                if (intr->had_error) {
                    break;
                }
                intr->is_greater = (current->type == CMD_CMP) ? (a > b) : ((uint64_t)a > (uint64_t)b);
                intr->is_equal = (a == b);
                intr->is_less = !(intr->is_greater || intr->is_equal);
                break;
            }
            case CMD_PRINT:
                if (!print_base(intr, current)) {
                    intr->had_error = true;
                }
                break;
            case CMD_AND:
            case CMD_EOR:
            case CMD_ORR: {
                int64_t a = fetch_number_value(intr, &current->val_a, false);
                int64_t b = fetch_number_value(intr, &current->val_b, false);
                if (intr->had_error) {
                    break;
                }

                int64_t result = (current->type == CMD_AND) ? (a & b) : 
                                 (current->type == CMD_EOR) ? (a ^ b) : (a | b);
                intr->variables[current->destination.num_val] = result;
                break;
            }
            case CMD_LSL:
            case CMD_LSR:
            case CMD_ASR: {
                int64_t a = fetch_number_value(intr, &current->val_a, false);
                int64_t b = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                if (intr->had_error) {
                    break;
                }
                if (b < 0 || b > 63) {  
                    intr->had_error = true;
                    break;
                }
                int64_t result;
                if (current->type == CMD_LSL) {
                    result = a << b;
                } else if (current->type == CMD_LSR) {
                    result = (uint64_t)a >> b; // Logical shift fills with zeros
                } else { 
                    result = a >> b; // Shift preserves sign bit
                }
                intr->variables[current->destination.num_val] = result;
                break;
            }
            case CMD_LOAD: {
                uint8_t *dest_index = (uint8_t*) &intr->variables[current->destination.num_val];
                size_t offset = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                size_t bytes = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                intr->variables[current->destination.num_val] = 0;
                if (!mem_load(dest_index, offset, bytes)) {
                    intr->had_error = true;
                    break;
                }
                break;
            }
            case CMD_STORE: {
                int64_t src_index = current->destination.num_val;
                int64_t bytes = fetch_number_value(intr, &current->val_a, current->is_a_immediate);
                int64_t offset = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                // Validate byte size
                if (bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8) {
                    intr->had_error = true;
                    break;
                }
                // Validate memory bounds
                if (offset + bytes > MEM_CAPACITY) {
                    intr->had_error = true;
                    break;
                }
                uint8_t buffer[8] = {0};
                int64_t value = intr->variables[src_index];
                // Store bytes in little-endian order
                for (int i = 0; i < bytes; i++) {
                    buffer[i] = (value >> (i * 8)) & 0xFF;
                }
                if (!mem_store(buffer, (size_t)offset, (size_t)bytes)) {
                    intr->had_error = true;
                    break;
                }
                break;
            }
            case CMD_PUT: {
                int64_t offset = fetch_number_value(intr, &current->val_b, current->is_b_immediate);
                const char *str = current->val_a.str_val;
                if (!intr->had_error && str) {
                    size_t str_len = strlen(str) + 1;
                    for (size_t i = 0; i < str_len; i++) {
                        if (!mem_store((uint8_t *)&str[i], offset + i, 1)) {
                            intr->had_error = true;
                            break;
                        }
                    }
                } else {
                    intr->had_error = true;
                }
                break;
            }
            case CMD_BRANCH: {
                if (cond_holds(intr, current->branch_condition)) {
                    const char *label = current->val_a.str_val;
                    Entry *entry = get_label(intr->label_map, (char *)label);
                    if (!entry) {
                        if (strncmp(label, ".L", 2) == 0) {
                            current = NULL;
                            jumped = true;
                            break;
                        }
                        intr->had_error = true;
                        printf("Label not found: %s\n", label);
                        break;
                    }
                    current = entry->command; 
                } else {
                    current = current->next;  
                }
                jumped = true;
                break;
            }
            case CMD_CALL: {
                const char *label = current->val_a.str_val;
                Entry *target = get_label(intr->label_map, (char*)label);
                if (!target) {
                    intr->had_error = true;
                    printf("Label not found: %s\n", label);
                    break;
                }
                // Allocate a new stack frame
                StackEntry *stack_entry = malloc(sizeof(StackEntry));
                if (!stack_entry) {
                    intr->had_error = true;
                    break;
                }
                // Save all registers and the return address
                memcpy(stack_entry->variables, intr->variables, sizeof(int64_t) * NUM_VARIABLES);
                stack_entry->command = current->next;
                stack_entry->next = intr->the_stack;
                intr->the_stack = stack_entry;
                current = target->command;  // Jump to function label
                jumped = true;
                break;
            }
            case CMD_RET: {
                if (!intr->the_stack) {
                    current = NULL;  // No stack frame to return to -> end execution
                    jumped = true;   
                    break;
                }
                StackEntry *stack_entry = intr->the_stack;
                intr->the_stack = stack_entry->next;
                // Restore all registers except x0
                memcpy(&intr->variables[1], &stack_entry->variables[1], sizeof(int64_t) * (NUM_VARIABLES - 1));
                current = stack_entry->command; 
                free(stack_entry);
                jumped = true;
                break;
            }
            default:
                intr->had_error = true;
                break;
        }
        // Move to the next command if no errors occurred
        if (!intr->had_error && !jumped) {
            current = current->next;
        }
    }
    // Week 4: free the stack at the end
    while (intr->the_stack) {
        StackEntry *temp = intr->the_stack;
        intr->the_stack = temp->next;
        free(temp);
    }
}

void print_interpreter_state(Interpreter *intr) {
    if (!intr) {
        return;
    }

    printf("Error: %d\n", intr->had_error);
    printf("Flags:\n");
    printf("Is greater: %d\n", intr->is_greater);
    printf("Is equal: %d\n", intr->is_equal);
    printf("Is less: %d\n", intr->is_less);

    printf("\n");

    printf("Variable values:\n");
    for (size_t i = 0; i < NUM_VARIABLES; i++) {
        printf("x%zu: %" PRId64 "", i, intr->variables[i]);

        if (i < NUM_VARIABLES - 1) {
            printf(", ");
        }

        if ((i + 1) % 8 == 0) {
            printf("\n");
        }
    }

    printf("\n");
}

/**
 * @brief Fetches the appropriate value from the given operand.
 *
 * @param intr The pointer to the interpreter holding variable state.
 * @param op The operand used to fetch the value.
 * @param is_im A boolean representing whether this value is an immediate or
 * must be read in from the interpreter state.
 * @return The fetched value.
 */
static int64_t fetch_number_value(Interpreter *intr, Operand *op, bool is_im) {
    // STUDENT TODO: Fetch either a variable from the interpreter's state or directly output a value
    if (is_im) {
        // Immediate value
        return op->num_val;
    } else {
        // Variable value
        int64_t var_num = op->num_val;
        if (var_num < 0 || var_num >= NUM_VARIABLES) {
            intr->had_error = true; 
            return 0;
        }
        return intr->variables[var_num];
    }
}

/**
 * @brief Determines whether a given branch condition holds.
 *
 * @param intr The pointer to the interpreter holding the result of the
 * comparison.
 * @param cond The condition to check.
 * @return True if the given condition holds, false otherwise.
 */
static bool cond_holds(Interpreter *intr, BranchCondition cond) {
    switch (cond) {
        case BRANCH_ALWAYS:          return true;
        case BRANCH_EQUAL:           return intr->is_equal;
        case BRANCH_NOT_EQUAL:       return !intr->is_equal;
        case BRANCH_GREATER:         return intr->is_greater;
        case BRANCH_LESS:            return intr->is_less;
        case BRANCH_GREATER_EQUAL:   return intr->is_greater || intr->is_equal;
        case BRANCH_LESS_EQUAL:      return intr->is_less || intr->is_equal;
        default:                     return false;
    }
}

/**
 * @brief Prints the given command's value in a specified base.
 *
 * @param intr The pointer to the interpreter holding variable state.
 * @param cmd The command being processed.
 * @return True whether the print was successful, false otherwise.
 */
static bool print_base(Interpreter *intr, Command *cmd) {
    // Fetch the value to be printed
    int64_t value = fetch_number_value(intr, &cmd->val_a, cmd->is_a_immediate);
    if (intr->had_error) return false;  // Ensure no errors occurred

    // Declare all necessary variables at the top
    char bit_string[67];  
    int64_t offset;
    char str[MEM_CAPACITY] = {0};  
    size_t i = 0;  // Declare 'i' before switch

    // Handle decimal output case first
    if (cmd->val_b.base == 'd') { 
        printf("%" PRId64 "\n", value);  // Print the integer value
        return true;
    }

    // Handle other cases (binary, hex, string)
    switch (cmd->val_b.base) {
        case 'b': // Binary output
            to_binary_string(value, bit_string, sizeof(bit_string));
            printf("%s\n", bit_string);
            break;
        case 'x': // Hexadecimal output
            printf("0x%" PRIx64 "\n", (uint64_t)value);
            break;
        case 's': // String output
            offset = fetch_number_value(intr, &cmd->val_a, cmd->is_a_immediate);
            if (intr->had_error) return false;

            for (i = 0; i < MEM_CAPACITY - 1; i++) {
                if (!mem_load((uint8_t *)&str[i], offset + i, 1)) {
                    intr->had_error = true;
                    return false;
                }
                if (str[i] == '\0') break;  // Stop when null terminator is reached
            }
            str[i] = '\0';  
            printf("%s\n", str); 
            break;
        default:
            intr->had_error = true;
            return false;
    }
    return true;
}

// Helper Function for Binary Conversion
static void to_binary_string(uint64_t num, char *bit_string, size_t bit_string_size) {
    size_t index = 0;
    // Add the '0b' prefix
    bit_string[index++] = '0';
    bit_string[index++] = 'b';
    // Handle zero case
    if (num == 0) {
        bit_string[index++] = '0';
        bit_string[index] = '\0';
        return;
    }
    int highest_bit = 63;
    while (highest_bit >= 0 && !(num & (1ULL << highest_bit))) {
        highest_bit--;
    }
    for (int i = highest_bit; i >= 0; i--) { 
        if (index >= bit_string_size - 1) {
            bit_string[bit_string_size - 1] = '\0';
            return;
        }
        bit_string[index++] = (num & (1ULL << i)) ? '1' : '0';
    }
    bit_string[index] = '\0'; 
}